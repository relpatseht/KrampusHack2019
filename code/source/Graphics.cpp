#define ALLEGRO_UNSTABLE
#define ALLEGRO_WINDOWS
#include <vector>
#include <atomic>
#include <thread>
#include <fstream>
#include <filesystem>
#include <chrono>
#include "glm/glm.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "allegro5/allegro.h"
#include "allegro5/allegro_opengl.h"

typedef unsigned uint;

namespace 
{
	struct SceneEntry
	{
		glm::mat3 invRot;
		uint8_t _padding0[4];
		glm::vec3 invPos;
		uint8_t _padding1[4];
		glm::float32 scale;
		glm::float32 type;
		uint8_t _padding2[8];
	};

	struct SceneEntryBlock
	{
		static const uint MAX_ENTRIES = 512;
		glm::uint entryCount;
		uint8_t _padding[12];
		SceneEntry entries[MAX_ENTRIES];
	};

	struct DynamicData
	{
		std::vector<glm::mat3x3> invRot;
		std::vector<glm::vec3> invPos;
		std::vector<glm::float32> scale;
		glm::vec3 camPos{ 0.0f, 0.0f, 0.0f };
		glm::vec3 camTarget{ 0.0f, 0.0f, 0.0f };
	};

	struct RenderData
	{
		DynamicData data;
		std::vector<float> type;
	};

	struct RenderStore
	{
		RenderData gpu;
		DynamicData velocity;
	};

	enum SwapState : uint
	{
		INITIALIZING,
		RENDERING,
		SWAP_MESH_STORE,
		RELOAD_SHADERS,
		RESIZE,
		SHUTDOWN
	};
}

struct Graphics
{
	std::atomic<SwapState> state;
	ALLEGRO_DISPLAY* display;
	uint curMeshStore;
	RenderStore meshStores[2];
	std::thread renderThread;

	uint sceneVAO;
	uint sceneUBO;
	uint shader;
};

namespace
{
	namespace err
	{
		static void ClearErrors()
		{
			while (glGetError() != GL_NO_ERROR);
		}

		static bool Error()
		{
			bool ret = false;
			GLenum err;

			while ((err = glGetError()) != GL_NO_ERROR)
			{
				ret = true;
				printf("OpenGL Error %u\n", err);
				__debugbreak();
			}

			return ret;
		}
	}

	namespace shader
	{
		struct SourcePair
		{
			std::string vert, frag;
		};

		struct ShaderFiles
		{
			SourcePair sceneSource;
			SourcePair uiSource;
			std::vector<std::string> includeNames;
		};

		static bool LoadFiles(const char* dir, ShaderFiles* outFiles)
		{
			namespace fs = std::filesystem;

			fs::path root = fs::path(dir).lexically_normal();

			for (const auto& p : fs::recursive_directory_iterator(root.c_str()))
			{
				if (p.is_regular_file())
				{
					fs::path curPath = p.path().lexically_relative(root);

					std::string ext = curPath.extension().string();

					if (ext.size() >= 2)
					{
						std::string curPathStr = curPath.string();
						FILE* const curFile = std::fopen(p.path().string().c_str(), "rb");

						if (!curFile)
						{
							printf("Failed to load '%s'\n", curPathStr.c_str());
						}
						else
						{
							std::string fileData;
							const char extType = tolower(ext[1]);
							SourcePair* const outPair = (tolower(curPath.filename().c_str()[0]) == 'u') ? &outFiles->uiSource : &outFiles->sceneSource;

							std::fseek(curFile, 0, SEEK_END);
							const size_t fileSize = std::ftell(curFile);
							std::fseek(curFile, 0, SEEK_SET);

							fileData.resize(fileSize);
							std::fread(fileData.data(), 1, fileSize, curFile);

							fclose(curFile);

							if (extType == 'f')
								outPair->frag = std::move(fileData);
							else if (extType == 'v')
								outPair->vert = std::move(fileData);
							else
							{
								curPathStr = std::string("/") + curPathStr;
								glNamedStringARB(GL_SHADER_INCLUDE_ARB, static_cast<uint>(curPathStr.size()), curPathStr.c_str(), 
									             static_cast<uint>(fileData.size()), fileData.c_str());
								outFiles->includeNames.emplace_back(std::move(curPathStr));
							}
						}
					}
				}
			}

			return !err::Error();
		}

		static bool CompileShader(const std::string& source, GLenum type, uint* outShader)
		{
			const uint shader = glCreateShader(type);
			const char* sourceStr = source.c_str();
			bool ret = true;

			glShaderSource(shader, 1, &sourceStr, 0);
			glCompileShader(shader);

			GLint isCompiled = 0;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);

			if (isCompiled == GL_FALSE)
			{
				GLint infoLen = 0;
				std::vector<GLchar> errLog;

				glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);

				errLog.resize(infoLen);
				glGetShaderInfoLog(shader, infoLen, &infoLen, errLog.data());

				printf("Failed to build shader with '%s'\n", errLog.data());

				ret = false;
			}

			if (err::Error() || !ret)
			{
				glDeleteShader(shader);
				*outShader = 0;

				return false;
			}

			*outShader = shader;
			return true;
		}

		static bool BuildShader(const SourcePair& source, uint* outShader)
		{
			uint fragShader, vertShader;

			if (!source.frag.empty() && CompileShader(source.frag, GL_FRAGMENT_SHADER, &fragShader))
			{
				if (!source.vert.empty() && CompileShader(source.vert, GL_VERTEX_SHADER, &vertShader))
				{
					uint prog = glCreateProgram();

					glAttachShader(prog, fragShader);
					glAttachShader(prog, vertShader);

					glLinkProgram(prog);

					GLint isLinked = 0;
					glGetProgramiv(prog, GL_LINK_STATUS, &isLinked);

					if (isLinked == GL_FALSE)
					{
						GLint infoLen = 0;
						std::vector<GLchar> errLog;

						glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &infoLen);

						errLog.resize(infoLen);
						glGetProgramInfoLog(prog, infoLen, &infoLen, errLog.data());

						printf("Failed to link shader with '%s'\n", errLog.data());
					}
					else
					{
						if (!err::Error())
						{
							glDetachShader(prog, vertShader);
							glDetachShader(prog, fragShader);

							*outShader = prog;
							return true;
						}
					}

					glDeleteShader(vertShader);
					glDeleteProgram(prog);
				}

				glDeleteShader(fragShader);
			}

			*outShader = 0;
			err::Error();
			return false;
		}

		static bool LoadShaders(const char* dir, uint* outScene, uint* outUI)
		{
			ShaderFiles files;

			err::ClearErrors();

			if (LoadFiles(dir, &files))
			{
				uint sceneShader, uiShader;

				if (BuildShader(files.sceneSource, &sceneShader))
					*outScene = sceneShader;
				else
					*outScene = 0;

				if (BuildShader(files.uiSource, &uiShader))
					*outUI = uiShader;
				else
					*outUI = 0;
			}

			for (const std::string& inc : files.includeNames)
			{
				glDeleteNamedStringARB(static_cast<uint>(inc.size()), inc.c_str());
			}

			return err::Error();
		}
	}

	namespace scene
	{
		static bool GenVAO(uint* outVAO)
		{
			uint vao;

			glGenVertexArrays(1, &vao);

			if (err::Error())
			{
				*outVAO = 0;
				return false;
			}

			*outVAO = vao;
			return true;
		}

		static bool GenUBO(uint* outUBO)
		{
			uint ubo;

			glGenBuffers(1, &ubo);
			glBindBuffer(GL_UNIFORM_BUFFER, ubo);
			glBufferData(GL_UNIFORM_BUFFER, sizeof(SceneEntryBlock), nullptr, GL_DYNAMIC_DRAW);
			glBindBuffer(GL_UNIFORM_BUFFER, 0);

			if (err::Error())
			{
				*outUBO = 0;
				return false;
			}
			
			*outUBO = ubo;
			return true;
		}

		static bool UpdateUniforms(uint ubo, const RenderData& data)
		{
			static const uint camPosBinding = 0;
			static const uint camTargetBinding = 1;
			static const uint sceneBlockBinding = 0;
			std::vector<SceneEntry> entries;

			glUniform3fv(camPosBinding, 1, glm::value_ptr(data.data.camPos));
			glUniform3fv(camTargetBinding, 1, glm::value_ptr(data.data.camTarget));
			
			uint entryCount = static_cast<uint>(data.data.invPos.size());

			if (entryCount > SceneEntryBlock::MAX_ENTRIES)
			{
				printf("Warning: %u/%u scene entries. Discarding last %u.\n", entryCount, SceneEntryBlock::MAX_ENTRIES, entryCount - SceneEntryBlock::MAX_ENTRIES);
				entryCount = SceneEntryBlock::MAX_ENTRIES;
			}
			entries.resize(entryCount);

			assert(data.data.invPos.size() == data.data.invRot.size());
			assert(data.data.invRot.size() == data.data.scale.size());
			assert(data.data.scale.size() == data.type.size());

			for (uint entryIndex = 0; entryIndex < entryCount; ++entryIndex)
			{
				SceneEntry* const entry = entries.data() + entryIndex;

				entry->invPos = data.data.invPos[entryIndex];
				entry->invRot = data.data.invRot[entryIndex];
				entry->scale = data.data.scale[entryIndex];
				entry->type = data.type[entryIndex];
			}

			glBindBuffer(GL_UNIFORM_BUFFER, ubo);
			glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(uint), &entryCount);
			glBufferSubData(GL_UNIFORM_BUFFER, 16, entryCount * sizeof(SceneEntry), entries.data());
			glBindBuffer(GL_UNIFORM_BUFFER, 0);

			return err::Error();
		}
	}

	namespace render
	{
		static bool Init(Graphics* g)
		{
			al_set_new_display_flags(ALLEGRO_WINDOWED | ALLEGRO_RESIZABLE | ALLEGRO_OPENGL | ALLEGRO_OPENGL_3_0 | ALLEGRO_OPENGL_FORWARD_COMPATIBLE);
			
			ALLEGRO_DISPLAY* const display = al_create_display(1024, 768);

			if (!display)
			{
				printf("Failed to create display.\n");
			}
			else
			{
				uint sceneShader, uiShader;

				shader::LoadShaders("shaders", &sceneShader, &uiShader);

				if (sceneShader == 0)
				{
					printf("Failed to find shaders in 'shaders' directory.\n");
				}
				else
				{
					uint sceneVAO;

					scene::GenVAO(&sceneVAO);

					if (sceneVAO == 0)
					{
						printf("Failed to generate verts for scene.\n");
					}
					else
					{
						uint sceneUBO;

						scene::GenUBO(&sceneUBO);

						if (sceneUBO == 0)
						{
							printf("Failed to generate uniform buffer for scene.\n");
						}
						else
						{
							g->curMeshStore = 0;
							g->shader = sceneShader;
							g->sceneUBO = sceneUBO;
							g->sceneVAO = sceneVAO;
							g->display = display;

							g->meshStores[0].gpu.data.camPos = glm::vec3(0.0f, 5.0f, -10.0f);
							g->meshStores[0].gpu.data.camTarget = glm::vec3(0.0f, 0.0f, 0.0f);

							g->meshStores[1].gpu.data.camPos = glm::vec3(0.0f, 5.0f, -10.0f);
							g->meshStores[1].gpu.data.camTarget = glm::vec3(0.0f, 0.0f, 0.0f);

							return true;
						}

						glDeleteVertexArrays(1, &sceneVAO);
					}

					glDeleteProgram(sceneShader);
					glDeleteProgram(uiShader);
				}

				al_destroy_display(display);
			}

			return false;
		}

		static bool UpdateState(Graphics* g)
		{
			switch (g->state)
			{
				case SWAP_MESH_STORE:
					g->curMeshStore = !g->curMeshStore;
					g->state = SwapState::RENDERING;
				break;
				case RELOAD_SHADERS:
				{
					uint sceneShader, uiShader;

					shader::LoadShaders("shaders", &sceneShader, &uiShader);

					if (sceneShader != 0)
					{
						glDeleteProgram(g->shader);
						g->shader = sceneShader;
					}
					else
					{
						printf("Shaders failed to compile. Not reloading.\n");
					}

					g->state = SwapState::RENDERING;
				}
				break;
				case RESIZE:
					al_acknowledge_resize(g->display);
					g->state = SwapState::RENDERING;
				break;
				case SHUTDOWN:
					return false;
			}

			return true;
		}

		static void Update(RenderStore* store, float elapsedSeconds)
		{
			DynamicData* const cur = &store->gpu.data;
			const DynamicData& vel = store->velocity;

			assert(cur->invPos.size() == vel.invPos.size());
			for (size_t posIndex = 0; posIndex < cur->invPos.size(); ++posIndex)
			{
				cur->invPos[posIndex] += vel.invPos[posIndex] * elapsedSeconds;
			}

			assert(cur->invRot.size() == vel.invRot.size());
			assert(cur->invRot.size() == cur->invPos.size());
			for (size_t rotIndex = 0; rotIndex < cur->invRot.size(); ++rotIndex)
			{
				cur->invRot[rotIndex] *= vel.invRot[rotIndex] * elapsedSeconds;
			}

			assert(cur->scale.size() == vel.scale.size());
			assert(cur->scale.size() == cur->invRot.size());
			for (size_t rotIndex = 0; rotIndex < cur->invRot.size(); ++rotIndex)
			{
				cur->scale[rotIndex] += vel.scale[rotIndex] * elapsedSeconds;
			}

			cur->camPos += vel.camPos * elapsedSeconds;
			cur->camTarget += vel.camTarget * elapsedSeconds;
		}

		static void Render(Graphics* g)
		{
			assert(g->state == SwapState::INITIALIZING);

			if (!Init(g))
			{
				g->state = SwapState::SHUTDOWN;
			}
			else
			{
				g->state = SwapState::RENDERING;

				auto frameStart = std::chrono::high_resolution_clock::now();

				while (UpdateState(g))
				{
					RenderStore* const store = g->meshStores + g->curMeshStore;
					const RenderData& data = store->gpu;
					const auto frameEnd = std::chrono::high_resolution_clock::now();
					std::chrono::duration<float> frameDurationSec = frameEnd - frameStart;
					const float elapsedSec = frameDurationSec.count();
					frameStart = frameEnd;

					Update(store, elapsedSec);

					glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

					glUseProgram(g->shader);
					scene::UpdateUniforms(g->sceneUBO, store->gpu);

					glBindVertexArray(g->sceneVAO);
					glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

					glBindVertexArray(0);
					glUseProgram(0);

					al_flip_display();
				}

				glDeleteProgram(g->shader);
				glDeleteBuffers(1, &g->sceneUBO);
				glDeleteVertexArrays(1, &g->sceneVAO);
				al_destroy_display(g->display);
			}
		}
	}
}

namespace gfx
{
	Graphics* Init()
	{
		Graphics* g = new Graphics;

		g->state = SwapState::INITIALIZING;
		g->renderThread = std::thread(render::Render, g);

		while (g->state == SwapState::INITIALIZING)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}

		if (g->state != SwapState::RENDERING)
		{
			printf("Graphics failed to initialize.\n");
			g->renderThread.join();
			delete g;
			g = nullptr;
		}

		return g;
	}

	void ReloadShaders(Graphics* g)
	{
		assert(g->state == SwapState::RENDERING);
		g->state = SwapState::RELOAD_SHADERS; // We don't wait for the reload to happen for now. Maybe should if we get too multithreaded with too many states
	}

	void Resize(Graphics* g)
	{
		assert(g->state == SwapState::RENDERING);
		g->state = SwapState::RESIZE; // We don't wait for the reload to happen for now. Maybe should if we get too multithreaded with too many states
	}

	ALLEGRO_DISPLAY* GetDisplay(Graphics* g)
	{
		return g->display;
	}

	void Shutdown(Graphics *g)
	{
		g->state = SwapState::SHUTDOWN;
		g->renderThread.join();

		delete g;
	}
}