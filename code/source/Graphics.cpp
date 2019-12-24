#define ALLEGRO_UNSTABLE
#define ALLEGRO_WINDOWS
#include <vector>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <unordered_map>
#include <numeric>
#include "ComponentTypes.h"
#include "Component.h"
#include "Game.h"
#include "bvh/spatial_tree.h"
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
	namespace tree
	{
		using namespace glm;
#include "shaders/bvh_node.glsl"
	}
}

struct Graphics
{
	static const uint MAX_ENTRIES = MAX_SCENE_ENTRIES;
	component_list<glm::mat4> sceneEntries;
	std::vector<tree::Node> sceneBVH;
	glm::vec3 camPos{ 0.0f, 0.0f, 0.0f };
	glm::vec3 camTarget{ 0.0f, 0.0f, 0.0f };
	glm::float1 camInvFov;
	glm::mat3 camViewMtx;
	glm::vec2 res{ 1024.0f, 768.0f };
	
	uint sceneVAO = 0;
	uint sceneEntrySSB = 0;
	uint sceneBVHSSB = 0;
	uint shader = 0;
	std::vector<uint> meshTypeRemap;
	std::unordered_map<uint, uint> objToMesh;
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

	namespace tree
	{
		namespace int_tree
		{
			struct CompNode
			{
				CompNode* children[4];
				glm::vec4 childMinX;
				glm::vec4 childMinY;
				glm::vec4 childMinZ;
				glm::vec4 childMaxX;
				glm::vec4 childMaxY;
				glm::vec4 childMaxZ;
			};

			static CompNode* ConvertToGPU_r(const spatial_tree<uint, 3, 4>& node, std::vector<CompNode>* outNodes)
			{
				const uint nodeIndex = static_cast<uint>(outNodes->size());
				CompNode* const outNode = &outNodes->emplace_back();

				glm::vec4* const outMins[] = { &outNode->childMinX, &outNode->childMinY, &outNode->childMinZ };
				glm::vec4* const outMaxs[] = { &outNode->childMaxX, &outNode->childMaxY, &outNode->childMaxZ };

				for (size_t d = 0; d < 3; ++d)
				{
					const float* const mins = node.child_mins(d);
					const float* const maxs = node.child_maxs(d);

					for (uint c = 0; c < node.child_count(); ++c)
					{
						(*outMins[d])[c] = mins[c];
						(*outMaxs[d])[c] = maxs[c];
					}

					for (uint c = node.child_count(); c < 4; ++c)
					{
						(*outMins[d])[c] = FLT_MAX;
						(*outMaxs[d])[c] = -FLT_MAX;
					}
				}

				if (node.leaf_branch())
				{
					for (uint c = 0; c < node.child_count(); ++c)
						outNode->children[c] = reinterpret_cast<CompNode*>(static_cast<uintptr_t>((node.leaf(c) << 1) | 1));
				}
				else
				{
					for (uint c = 0; c < node.child_count(); ++c)
						(*outNodes)[nodeIndex].children[c] = ConvertToGPU_r(node.subtree(c), outNodes);
				}

				for (uint c = node.child_count(); c < 4; ++c)
					(*outNodes)[nodeIndex].children[c] = nullptr;

				return outNodes->data() + nodeIndex;
			}

			static uint CompressGPUTree_r(CompNode* node)
			{
				uint childChildren[4];
				uint childCount;
				uint leafCount = 0;

				for (childCount = 0; childCount < 4; ++childCount)
				{
					CompNode* const childPtr = node->children[childCount];

					if (!childPtr)
						break;

					if (!(reinterpret_cast<uintptr_t>(childPtr) & 1))
						childChildren[childCount] = CompressGPUTree_r(childPtr);
					else
					{
						childChildren[childCount] = 0;
						++leafCount;
					}
				}

				if (leafCount < childCount)
				{
					uint newChildCount = childCount;
					for (uint c = 0; c < childCount; ++c)
					{
						if (childChildren[c])
						{
							uint emptySlots = 4 - newChildCount;
							if (childChildren[c] <= emptySlots + 1)
							{
								CompNode* const child = node->children[c];

								for (uint childC = 0; childC < childChildren[c]; ++childC)
								{
									const uint dest = !childC ? c : newChildCount++;

									node->children[dest] = child->children[childC];
									node->childMinX[dest] = child->childMinX[childC];
									node->childMinY[dest] = child->childMinY[childC];
									node->childMinZ[dest] = child->childMinZ[childC];
									node->childMaxX[dest] = child->childMaxX[childC];
									node->childMaxY[dest] = child->childMaxY[childC];
									node->childMaxZ[dest] = child->childMaxZ[childC];
								}
							}
						}
					}

					childCount = newChildCount;
				}

				return childCount;
			}

			static uint FinalizeGPUTree_r(CompNode* node, std::vector<Node>* outNodes)
			{
				const uint nodeIndex = static_cast<uint>(outNodes->size());
				Node* outNode = &outNodes->emplace_back();

				outNode->childMinX = node->childMinX;
				outNode->childMinY = node->childMinY;
				outNode->childMinZ = node->childMinZ;
				outNode->childMaxX = node->childMaxX;
				outNode->childMaxY = node->childMaxY;
				outNode->childMaxZ = node->childMaxZ;

				for (uint c = 0; c < 4; ++c)
				{
					CompNode* const childNode = node->children[c];

					if (childNode)
					{
						const uintptr_t childNodeBits = reinterpret_cast<uintptr_t>(childNode);
						const bool isLeaf = childNodeBits & 1;

						if (isLeaf)
						{
							outNode->childOffsets[c] = static_cast<uint>((childNodeBits >> 1) | LEAF_NODE_MASK);
						}
						else
						{
							const uint childNodeOffset = FinalizeGPUTree_r(childNode, outNodes);

							outNode = outNodes->data() + nodeIndex;
							outNode->childOffsets[c] = childNodeOffset;
						}
					}
				}

				return nodeIndex;
			}
		}

		static std::vector<Node> BuildSceneBVH(const Graphics& g)
		{
			using namespace int_tree;
			const uint entryCount = static_cast<uint>(g.sceneEntries.size());
			const glm::mat4* const entries = g.sceneEntries.data();
			uint* const indices = (uint*)_malloca(sizeof(uint) * entryCount);
			spatial_tree<uint, 3, 4> buildTree;

			std::iota(indices, indices + entryCount, 0);
			buildTree.insert(indices, indices + entryCount, [entries](uint entryIndex, float(&outMins)[3], float(&outMaxs)[3])
			{
				glm::mat4 entry = entries[entryIndex];
				const uint entryType = static_cast<uint>(entry[3][3]);
				entry[3][3] = 1.0f;
				entry = glm::inverse(entry);

				const glm::vec3 entryPos = entry[3]; // inverse transform stored
				glm::vec3& mins = reinterpret_cast<glm::vec3&>(outMins);
				glm::vec3& maxs = reinterpret_cast<glm::vec3&>(outMaxs);

				mins = entryPos;
				maxs = entryPos;

				switch (entryType)
				{
				case MESH_TYPE_PLAYER:
					mins -= glm::vec3(PLAYER_WIDTH * 0.5f, 0.0f, PLAYER_WIDTH * 0.5f);
					maxs += glm::vec3(PLAYER_WIDTH * 0.5f, PLAYER_HEIGHT, PLAYER_WIDTH * 0.5f);
					break;
				case MESH_TYPE_HELPER:
					mins -= glm::vec3(HELPER_RADIUS);
					maxs += glm::vec3(HELPER_RADIUS);
					break;
				case MESH_TYPE_SNOW_FLAKE:
					mins -= glm::vec3(SNOWFLAKE_RADIUS);
					maxs += glm::vec3(SNOWFLAKE_RADIUS);
					break;
				case MESH_TYPE_SNOW_BALL:
					mins -= glm::vec3(SNOWBALL_RADIUS);
					maxs += glm::vec3(SNOWBALL_RADIUS);
					break;
				case MESH_TYPE_SNOW_MAN:
					break;
				case MESH_TYPE_STATIC_PLATFORMS:
				{
					float xOffs = -BOTTOM_LEFT_PLATFORM_X + BOTTOM_PLATFORM_WIDTH;
					float yMin = -BOTTOM_LEFT_PLATFORM_Y + PLATFORM_DIM;
					float yMax = -yMin + (PLATFORM_VERTICAL_SPACE * PLATFORM_COUNT) + PLATFORM_DIM;

					mins -= glm::vec3(xOffs, yMin, PLATFORM_DIM);
					maxs += glm::vec3(xOffs, yMax, PLATFORM_DIM);
				}
				break;
				case MESH_TYPE_WORLD_BOUNDS:
					mins -= glm::vec3(BOUNDS_HALF_WIDTH, BOUNDS_HALF_HEIGHT, 0.1f);
					maxs += glm::vec3(BOUNDS_HALF_WIDTH, BOUNDS_HALF_HEIGHT, 0.1f);
					break;
				}
			});

			std::vector<CompNode> intGPUNodes;
			intGPUNodes.reserve(entryCount * 2);
			CompNode* const headIntNode = ConvertToGPU_r(buildTree, &intGPUNodes);
			CompressGPUTree_r(headIntNode);

			std::vector<Node> gpuNodes;
			gpuNodes.reserve(intGPUNodes.size());
			FinalizeGPUTree_r(headIntNode, &gpuNodes);

			_freea(indices);

			return gpuNodes;
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

		static bool GenSSB(uint* outSSB, size_t size)
		{
			uint ssb;

			glGenBuffers(1, &ssb);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssb);
			glBufferData(GL_SHADER_STORAGE_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

			if (err::Error())
			{
				*outSSB = 0;
				return false;
			}
			
			*outSSB = ssb;
			return true;
		}

		static bool UpdateUniforms(const Graphics &g)
		{
			static const uint camPosBinding = 0;
			static const uint camTargetBinding = 1;
			static const uint camInvFovBinding = 2;
			static const uint camViewBinding = 3;
			static const uint resolutionBinding = 6;
			static const uint sceneEntryCountBinding = 7;
			static const uint sceneEntryBufferBinding = 0;
			static const uint sceneBVHBufferBinding = 1;
			const uint sceneEntryCount = static_cast<uint>(g.sceneEntries.size());

			glUniform3fv(camPosBinding, 1, glm::value_ptr(g.camPos));
			glUniform3fv(camTargetBinding, 1, glm::value_ptr(g.camTarget));
			glUniform1f(camInvFovBinding, g.camInvFov);
			glUniformMatrix3fv(camViewBinding, 1, true, glm::value_ptr(g.camViewMtx));
			glUniform2fv(resolutionBinding, 1, glm::value_ptr(g.res));
			glUniform1ui(sceneEntryCountBinding, sceneEntryCount);
			
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, sceneEntryBufferBinding, g.sceneEntrySSB );

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, g.sceneEntrySSB);
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sceneEntryCount * sizeof(glm::mat4), g.sceneEntries.data());
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, sceneBVHBufferBinding, g.sceneBVHSSB);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, g.sceneBVHSSB);
			glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, g.sceneBVH.size() * sizeof(tree::Node), g.sceneBVH.data());
			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

			return err::Error();
		}
	}

	namespace render
	{
		static bool Init(Graphics* g)
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
					uint sceneEntrySSB;

					scene::GenSSB(&sceneEntrySSB, sizeof(glm::mat4) * Graphics::MAX_ENTRIES);

					if (sceneEntrySSB == 0)
					{
						printf("Failed to generate shader storage buffer for scene.\n");
					}
					else
					{
						uint sceneBVHSSB;

						scene::GenSSB(&sceneBVHSSB, sizeof(tree::Node) * Graphics::MAX_ENTRIES * 2);

						if (sceneBVHSSB == 0)
						{
							printf("Failed to generate shader storage buffer for bvh.\n");
						}
						else
						{
							g->shader = sceneShader;
							g->sceneEntrySSB = sceneEntrySSB;
							g->sceneBVHSSB = sceneBVHSSB;
							g->sceneVAO = sceneVAO;

							const float fov = 45.0f;
							g->camPos = glm::vec3(0.0f, 0.0f, 39.0f);
							g->camTarget = glm::vec3(0.0f, 0.0f, 0.0f);
							g->camInvFov = static_cast<float>(1.0 / std::tan((fov * 3.1415926535898) / 360.0));
							g->camViewMtx = glm::lookAt(g->camPos, g->camTarget, glm::vec3(0.0f, 1.0f, 0.0f));

							g->meshTypeRemap.resize(MESH_TYPE_COUNT);
							g->meshTypeRemap[static_cast<uint>(gfx::MeshType::PLAYER)] = MESH_TYPE_PLAYER;
							g->meshTypeRemap[static_cast<uint>(gfx::MeshType::HELPER)] = MESH_TYPE_HELPER;
							g->meshTypeRemap[static_cast<uint>(gfx::MeshType::SNOW_FLAKE)] = MESH_TYPE_SNOW_FLAKE;
							g->meshTypeRemap[static_cast<uint>(gfx::MeshType::SNOW_BALL)] = MESH_TYPE_SNOW_BALL;
							g->meshTypeRemap[static_cast<uint>(gfx::MeshType::SNOW_MAN)] = MESH_TYPE_SNOW_MAN;
							g->meshTypeRemap[static_cast<uint>(gfx::MeshType::STATIC_PLATFORMS)] = MESH_TYPE_STATIC_PLATFORMS;
							g->meshTypeRemap[static_cast<uint>(gfx::MeshType::WORLD_BOUNDS)] = MESH_TYPE_WORLD_BOUNDS;

							return true;
						}

						glDeleteBuffers(1, &sceneEntrySSB);
					}

					glDeleteVertexArrays(1, &sceneVAO);
				}

				glDeleteProgram(sceneShader);
				glDeleteProgram(uiShader);
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
		glDeleteBuffers(1, &g->sceneEntrySSB);
		glDeleteBuffers(1, &g->sceneBVHSSB);
		glDeleteVertexArrays(1, &g->sceneVAO);

		delete g;
	}


	void Update(Graphics* g)
	{
		render::Render(*g);
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

	void Resize(Graphics* g, uint x, uint y)
	{
		g->res = glm::vec2(static_cast<float>(x), static_cast<float>(y));
	}


	void PixelToWolrd(const Graphics& g, float* inoutX, float* inoutY)
	{
		const glm::vec2 pixel = glm::vec2(g.res.x - *inoutX, *inoutY) - (g.res * 0.5f);
		const float z = g.res.y * g.camInvFov;
		const glm::vec3 viewRayDir = glm::normalize(glm::vec3(pixel, z));
		const glm::vec3 worldRayDir = g.camViewMtx * viewRayDir;
		const float t = -g.camPos.z / worldRayDir.z;
		const glm::vec3 worldPos = g.camPos + worldRayDir * t;

		*inoutX = worldPos.x;
		*inoutY = worldPos.y;
	}

	bool AddModel( Graphics* g, uint objectId, MeshType type, const glm::mat4 &transform )
	{
		const uint modelId = static_cast<uint>(g->sceneEntries.size());

		if (modelId < Graphics::MAX_ENTRIES )
		{
			glm::mat4* const model = &g->sceneEntries.add_to_object(objectId);
			const float typeNum = static_cast<float>(g->meshTypeRemap[static_cast<uint>(type)]);
			const float typeFrac = rand() / static_cast<float>(RAND_MAX + 1);

			*model = glm::inverse( transform );
			(*model)[3][3] = typeNum + typeFrac;

			return true;
		}

		return false;
	}

	bool UpdateModels(Graphics* g, const std::vector<uint> &objectIds, const std::vector<glm::mat4>& transforms)
	{
		assert(objectIds.size() == transforms.size());

		for (size_t objIndex = 0; objIndex < objectIds.size(); ++objIndex)
		{
			const uint objId = objectIds[objIndex];
			glm::mat4* const outTrans = g->sceneEntries.for_object(objId);

			if (outTrans)
			{
				const glm::mat4 &trans = transforms[objIndex];
				const float meshType = (*outTrans)[3][3];

				*outTrans = glm::inverse(trans);
				(*outTrans)[3][3] = meshType;
			}
		}

		g->sceneBVH = tree::BuildSceneBVH(*g);

		return true;
	}

	void DestroyObjects( Graphics* g, const std::vector<uint>& objectIds )
	{
		g->sceneEntries.remove_objs(objectIds);
	}

}