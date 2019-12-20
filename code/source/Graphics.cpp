#define ALLEGRO_UNSTABLE
#define ALLEGRO_WINDOWS
#include <vector>
#include <fstream>
#include <filesystem>
#include <chrono>
#include "ObjectMap.h"
#include "ComponentTypes.h"
#include "shaders/scene_defines.glsl"
#include "glm/glm.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "allegro5/allegro.h"
#include "allegro5/allegro_opengl.h"

#include "Graphics.h"

typedef unsigned uint;

namespace 
{
	struct SceneEntryBlock
	{
		static const uint MAX_ENTRIES = MAX_SCENE_ENTRIES;
		glm::mat4 entries[MAX_ENTRIES]; // entries[X][3][3] is type, not 1. Fix in shader
	};

	static_assert( sizeof( SceneEntryBlock ) <= 16 * 1024, "OpenGL spec as 16KB is the lower min" );
}

struct Graphics
{
	SceneEntryBlock scene;
	glm::uint sceneEntryCount;
	glm::vec3 camPos{ 0.0f, 0.0f, 0.0f };
	glm::vec3 camTarget{ 0.0f, 0.0f, 0.0f };
	
	ALLEGRO_DISPLAY* display = nullptr;
	uint sceneVAO = 0;
	uint sceneUBO = 0;
	uint shader = 0;
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

		static bool UpdateUniforms(const Graphics &g)
		{
			static const uint camPosBinding = 0;
			static const uint camTargetBinding = 1;
			static const uint sceneEntryCountBinding = 2;
			static const uint sceneBlockBinding = 0;

			glUniform3fv(camPosBinding, 1, glm::value_ptr(g.camPos));
			glUniform3fv(camTargetBinding, 1, glm::value_ptr(g.camTarget));
			glUniform1ui(sceneEntryCountBinding, g.sceneEntryCount);
			
			glBindBufferBase( GL_UNIFORM_BUFFER, sceneBlockBinding, g.sceneUBO );

			glBindBuffer(GL_UNIFORM_BUFFER, g.sceneUBO);
			glBufferSubData(GL_UNIFORM_BUFFER, 0, g.sceneEntryCount * sizeof(glm::mat4), g.scene.entries);
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
							g->shader = sceneShader;
							g->sceneUBO = sceneUBO;
							g->sceneVAO = sceneVAO;
							g->display = display;

							g->sceneEntryCount = 0;
							g->camPos = glm::vec3(0.0f, 4.0f, 8.0f);
							g->camTarget = glm::vec3(0.0f, -3.0f, -2.0f);
							std::memset(&g->scene, 0, sizeof(g->scene));

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

		static void Render(const Graphics &g)
		{
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			glUseProgram(g.shader);
			scene::UpdateUniforms(g);

			glBindVertexArray(g.sceneVAO);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

			glBindVertexArray(0);
			glUseProgram(0);

			al_flip_display();
		}
	}
}

namespace gfx
{
	Graphics* Init()
	{
		Graphics* g = new Graphics;

		if (!render::Init(g))
		{
			printf("Graphics failed to initialize.\n");
			delete g;
			g = nullptr;
		}

		return g;
	}

	void Shutdown(Graphics* g)
	{
		glDeleteProgram(g->shader);
		glDeleteBuffers(1, &g->sceneUBO);
		glDeleteVertexArrays(1, &g->sceneVAO);
		al_destroy_display(g->display);

		delete g;
	}


	void Update(Graphics* g)
	{
		render::Render(*g);
	}

	ALLEGRO_DISPLAY* GetDisplay(Graphics* g)
	{
		return g->display;
	}

	void ReloadShaders(Graphics* g)
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
	}

	void Resize(Graphics* g)
	{
		al_acknowledge_resize(g->display);
	}

	bool AddModel( Graphics* g, ObjectMap* objects, uint objectId, MeshType type, const glm::mat4 &transform )
	{
		const uint mapId = g->sceneEntryCount;

		if ( mapId < SceneEntryBlock::MAX_ENTRIES )
		{
			glm::mat4* const model = g->scene.entries + (g->sceneEntryCount++);
			const float typeNum = static_cast<float>(type);
			const float typeFrac = rand() / static_cast<float>(RAND_MAX + 1);

			*model = glm::inverse( transform );
			(*model)[3][3] = typeNum + typeFrac;

			objects->set_object_mapping( objectId, ComponentType::GFX_MODEL, mapId );

			return true;
		}

		return false;
	}

	bool UpdateModels(Graphics* g, ObjectMap* objects, const std::vector<glm::mat4>& transforms)
	{
		return false;
	}

	void DestroyObjects( Graphics* g, ObjectMap* objects, const std::vector<uint>& objectIds )
	{
		uint mapTypes[] = { ComponentType::GFX_MODEL };
		std::vector<uint> freedMeshIds;

		GatherMappedIds(*objects, objectIds, mapTypes, &freedMeshIds);

		if ( !freedMeshIds.empty() )
		{
			uint lastMeshId = static_cast<uint>( g->sceneEntryCount ) - 1;
			
			for ( uint freedMeshId : freedMeshIds )
			{
				assert( freedMeshId <= lastMeshId );

				if ( freedMeshId < lastMeshId )
				{
					const uint lastMeshObjectId = objects->object_for_map_index( ComponentType::GFX_MODEL, lastMeshId );

					g->scene.entries[freedMeshId] = g->scene.entries[lastMeshId];
					
					objects->set_object_mapping( lastMeshObjectId, ComponentType::GFX_MODEL, freedMeshId );
				}

				--lastMeshId;
			}

			g->sceneEntryCount = lastMeshId + 1;
		}
	}

}