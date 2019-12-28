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

#include "generated_shaders/blur.frag.h"
#include "generated_shaders/downsample.frag.h"
#include "generated_shaders/output.frag.h"
#include "generated_shaders/scene.frag.h"
#include "generated_shaders/fullscreen_quad.vert.h"

#include "Graphics.h"
#include <iostream>

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
	static const uint BLUR_PASSES = BLOOM_BLUR_PASSES;

	component_list<glm::mat4> sceneEntries;
	std::vector<tree::Node> sceneBVH;
	glm::vec3 camPos{ 0.0f, 0.0f, 0.0f };
	glm::vec3 camTarget{ 0.0f, 0.0f, 0.0f };
	glm::float1 camInvFov;
	glm::mat3 camViewMtx;
	glm::vec2 res{ 1024.0f, 768.0f };
	glm::uint frameCount = 0;
	uint noiseTex = 0;

	uint sceneFBO = 0;
	uint sceneVAO = 0;
	uint sceneEntrySSB = 0;
	uint sceneBVHSSB = 0;
	uint sceneShader = 0;
	uint sceneMainTex = 0;
	uint sceneBrightTex[BLUR_PASSES] = {};

	uint downsampleFBO[BLUR_PASSES] = {};
	uint downsampleShader = 0;

	uint blurShader = 0;
	uint blurFBO[BLUR_PASSES] = {};
	uint blurTex[BLUR_PASSES] = {};


	uint outputShader = 0;
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

		void APIENTRY DebugOutput(GLenum source,
			GLenum type,
			GLuint id,
			GLenum severity,
			GLsizei length,
			const GLchar* message,
			void* userParam)
		{
			// ignore non-significant error/warning codes
			//if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return;

			std::cout << "---------------" << std::endl;
			std::cout << "Debug message (" << id << "): " << message << std::endl;

			switch (source)
			{
			case GL_DEBUG_SOURCE_API:             std::cout << "Source: API"; break;
			case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   std::cout << "Source: Window System"; break;
			case GL_DEBUG_SOURCE_SHADER_COMPILER: std::cout << "Source: Shader Compiler"; break;
			case GL_DEBUG_SOURCE_THIRD_PARTY:     std::cout << "Source: Third Party"; break;
			case GL_DEBUG_SOURCE_APPLICATION:     std::cout << "Source: Application"; break;
			case GL_DEBUG_SOURCE_OTHER:           std::cout << "Source: Other"; break;
			} std::cout << std::endl;

			switch (type)
			{
			case GL_DEBUG_TYPE_ERROR:               std::cout << "Type: Error"; break;
			case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: std::cout << "Type: Deprecated Behaviour"; break;
			case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  std::cout << "Type: Undefined Behaviour"; break;
			case GL_DEBUG_TYPE_PORTABILITY:         std::cout << "Type: Portability"; break;
			case GL_DEBUG_TYPE_PERFORMANCE:         std::cout << "Type: Performance"; break;
			case GL_DEBUG_TYPE_MARKER:              std::cout << "Type: Marker"; break;
			case GL_DEBUG_TYPE_PUSH_GROUP:          std::cout << "Type: Push Group"; break;
			case GL_DEBUG_TYPE_POP_GROUP:           std::cout << "Type: Pop Group"; break;
			case GL_DEBUG_TYPE_OTHER:               std::cout << "Type: Other"; break;
			} std::cout << std::endl;

			switch (severity)
			{
			case GL_DEBUG_SEVERITY_HIGH:         std::cout << "Severity: high"; break;
			case GL_DEBUG_SEVERITY_MEDIUM:       std::cout << "Severity: medium"; break;
			case GL_DEBUG_SEVERITY_LOW:          std::cout << "Severity: low"; break;
			case GL_DEBUG_SEVERITY_NOTIFICATION: std::cout << "Severity: notification"; break;
			} std::cout << std::endl;
			std::cout << std::endl;
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
			spatial_tree<uint, 3, 2> buildTree;

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
					mins -= glm::vec3(SNOWBALL_RADIUS * 1.3f);
					maxs += glm::vec3(SNOWBALL_RADIUS * 1.3f);
					break;
				case MESH_TYPE_SNOW_MAN:
					mins -= glm::vec3(-SNOWMAN_X + SNOWMAN_BOT_RADIUS, -SNOWMAN_BOT_Y + SNOWMAN_BOT_RADIUS, -SNOWMAN_Z + SNOWMAN_BOT_RADIUS);
					maxs += glm::vec3(SNOWMAN_X + SNOWMAN_BOT_RADIUS, SNOWMAN_TOP_Y + SNOWMAN_TOP_RADIUS, SNOWMAN_Z + SNOWMAN_BOT_RADIUS);
					break;
				case MESH_TYPE_FIRE_BALL:
					mins -= glm::vec3(FIREBALL_RADIUS * 2.0f);
					maxs += glm::vec3(FIREBALL_RADIUS * 2.0f);
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
				case MESH_TYPE_GROUND_PLANE: 
					mins -= glm::vec3(100.0f, 1.5f, 100.0f);
					maxs += glm::vec3(100.0f, 1.5f, 100.0f);
					break;
				case MESH_TYPE_TREE:
					mins -= glm::vec3(2.0f, 2.0f, 2.0f);
					maxs += glm::vec3(2.0f, 5.0f, 2.0f);
					break;
				}
			});
			 
			std::vector<CompNode> intGPUNodes;
			intGPUNodes.reserve(entryCount * 2);
			CompNode* const headIntNode = ConvertToGPU_r(buildTree, &intGPUNodes);
			assert(intGPUNodes.size() < entryCount * 2);
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
		static bool CompileShader(const ShaderModule &input, uint* outShader)
		{
			static const uint GL_SHADER_BINARY_FORMAT_SPIR_V = 0x9551;
			static const uint GL_SPIR_V_BINARY = 0x9552;
			typedef void (*PFNGLSHADERBINARYPROC) (GLsizei count, const GLuint* shaders, GLenum binaryformat, const void* binary, GLsizei length);
			typedef void (*PFNGLSPECIALIZESHADERPROC) (GLuint shader, const GLchar* pEntryPoint, GLuint numSpecializationConstants, const GLuint* pConstantIndex, const GLuint* pConstantValue);
			static PFNGLSHADERBINARYPROC glShaderBinary = (PFNGLSHADERBINARYPROC)al_get_opengl_proc_address("glShaderBinary");
			static PFNGLSPECIALIZESHADERPROC glSpecializeShader = (PFNGLSPECIALIZESHADERPROC)al_get_opengl_proc_address("glSpecializeShader");

			const uint shader = glCreateShader(input.type == ShaderStageType::VERTEX ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
			bool ret = true;

			glShaderBinary(1, &shader, GL_SHADER_BINARY_FORMAT_SPIR_V, input.progam, input.programSizeDWords * sizeof(unsigned));
			glSpecializeShader(shader, input.entryPoint, 0, nullptr, nullptr);

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

		static bool BuildShader(uint vertShader, const ShaderModule& fragSource, uint* outShader)
		{
			uint fragShader;

			if (CompileShader(fragSource, &fragShader))
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
						glDeleteShader(fragShader);

						*outShader = prog;
						return true;
					}
				}

				glDeleteProgram(prog);
			}

			glDeleteShader(fragShader);

			*outShader = 0;
			err::Error();
			return false;
		}

		static bool LoadShaders(uint* outScene, uint* outBlur, uint* outDown, uint* outOutShader)
		{
			uint vertShader;
			if (CompileShader(fullscreen_quad_vert_shader, &vertShader))
			{
				uint sceneShader, blurShader, downShader, outShader;

				if (BuildShader(vertShader, scene_frag_shader, &sceneShader))
					*outScene = sceneShader;
				else
					*outScene = 0;
				 
				if (BuildShader(vertShader, blur_frag_shader, &blurShader))
					*outBlur = blurShader;
				else
					*outBlur = 0;

				if (BuildShader(vertShader, downsample_frag_shader, &downShader))
					*outDown = downShader;
				else
					*outDown = 0;

				if (BuildShader(vertShader, output_frag_shader, &outShader))
					*outOutShader = outShader;
				else
					*outOutShader = 0;

				glDeleteShader(vertShader);
			}

			return err::Error();
		}
	}

	namespace scene
	{
		static bool UpdateUniforms(const Graphics &g)
		{
			static const uint camPosBinding = 0;
			static const uint camTargetBinding = 1;
			static const uint camInvFovBinding = 2;
			static const uint camViewBinding = 3;
			static const uint resolutionBinding = 6;
			static const uint frameCountBinding = 7;
			static const uint sceneEntryCountBinding = 8;
			static const uint sceneEntryBufferBinding = 0;
			static const uint sceneBVHBufferBinding = 1;
			const uint sceneEntryCount = static_cast<uint>(g.sceneEntries.size());

			glUniform3fv(camPosBinding, 1, glm::value_ptr(g.camPos));
			glUniform3fv(camTargetBinding, 1, glm::value_ptr(g.camTarget));
			glUniform1f(camInvFovBinding, g.camInvFov);
			glUniformMatrix3fv(camViewBinding, 1, true, glm::value_ptr(g.camViewMtx));
			glUniform2fv(resolutionBinding, 1, glm::value_ptr(g.res));
			glUniform1ui(frameCountBinding, g.frameCount);
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
		namespace init
		{
			static bool GenFBO(uint* outFBO)
			{
				uint fbo;

				glGenFramebuffers(1, &fbo);

				if (err::Error())
				{
					*outFBO = 0;
					return false;
				}

				*outFBO = fbo;
				return true;
			}

			static bool GenFrameBufferTexture(uint width, uint height, GLenum type, uint *outFBO, uint *outTex)
			{
				uint fbo = 0;
				uint tex = 0;

				GenFBO(&fbo);
				glGenTextures(1, &tex);

				if (!err::Error() && fbo != 0 && tex != 0)
				{
					glBindFramebuffer(GL_FRAMEBUFFER, fbo);
					glBindTexture(GL_TEXTURE_2D, tex);

					glTexImage2D(GL_TEXTURE_2D, 0, type, width, height, 0, GL_RGB, GL_FLOAT, nullptr);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

					glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

					glBindFramebuffer(GL_FRAMEBUFFER, 0);
					glBindTexture(GL_TEXTURE_2D, 0);
				}				

				*outTex = tex;
				*outFBO = fbo;
				return tex != 0;
			}

			static bool GenNoiseTexture(uint width, uint height, uint* outTex)
			{
				uint tex = 0;
				glGenTextures(1, &tex);

				if (!err::Error() && tex != 0)
				{
					uint8_t* const pixels = (uint8_t*)malloc(width * height);
					uint8_t* const pixEnd = pixels + width * height;
					uint8_t* pixCur = pixels;

					while (pixCur != pixEnd)
						*pixCur++ = static_cast<uint8_t>(rand() % 256);

					err::Error();
					glBindTexture(GL_TEXTURE_2D, tex);

					err::Error();
					glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, pixels);

					err::Error();
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

					err::Error();
					glBindTexture(GL_TEXTURE_2D, 0);

					free(pixels);
				}

				*outTex = tex;
				return tex != 0;
			}

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

			static bool GenerateFrameBuffers(Graphics* g)
			{
				uint w = static_cast<uint>(g->res.x);
				uint h = static_cast<uint>(g->res.y);

				GenFrameBufferTexture(w, h, GL_RGB8, &g->sceneFBO, &g->sceneMainTex);
				if (!g->sceneMainTex)
				{
					std::printf("Failed to make scene framebuffer");
					glDeleteTextures(1, &g->sceneMainTex);
					return false;
				}

				for (uint p = 0; p < Graphics::BLUR_PASSES; ++p)
				{
					GenFrameBufferTexture(w, h, GL_RGB16F, &g->downsampleFBO[p], &g->sceneBrightTex[p]);

					if (!g->sceneBrightTex[p])
					{
						std::printf("Failed to make brighness texture pass %u\n", p);
						glDeleteTextures(1, &g->sceneMainTex);
						glDeleteTextures(p, g->sceneBrightTex);
						return false;
					}

					GenFrameBufferTexture(w, h, GL_RGB16F, &g->blurFBO[p], &g->blurTex[p]);

					if (!g->blurTex[p])
					{
						std::printf("Failed to make blur texture pass %u\n", p);
						glDeleteTextures(1, &g->sceneMainTex);
						glDeleteTextures(p + 1, g->sceneBrightTex);
						glDeleteTextures(p, g->blurTex);
						return false;
					}

					w >>= 1;
					h >>= 1;
				}

				glBindFramebuffer(GL_FRAMEBUFFER, g->sceneFBO);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, g->sceneBrightTex[0], 0);
				glBindFramebuffer(GL_FRAMEBUFFER, 0);

				return true;
			}

			static void DestroyFrameBuffers(Graphics* g)
			{
				glDeleteTextures(1, &g->sceneMainTex);
				glDeleteTextures(Graphics::BLUR_PASSES, g->sceneBrightTex);
				glDeleteTextures(Graphics::BLUR_PASSES, g->blurTex);

				g->sceneMainTex = 0;
				std::memset(g->sceneBrightTex, 0, sizeof(g->sceneBrightTex));
				std::memset(g->blurTex, 0, sizeof(g->blurTex));
			}

			static bool Init(Graphics* g)
			{
				uint sceneShader, blurShader, downShader, outShader;

				shader::LoadShaders(&sceneShader, &blurShader, &downShader, &outShader);

				if (sceneShader == 0 || blurShader == 0 || outShader == 0)
				{
					printf("Failed to find shaders in 'shaders' directory.\n");
				}
				else
				{
					uint noiseTex;
					uint sceneVAO;
					uint sceneEntrySSB;
					uint sceneBVHSSB;

					GenVAO(&sceneVAO);
					GenSSB(&sceneEntrySSB, sizeof(glm::mat4) * Graphics::MAX_ENTRIES);
					GenSSB(&sceneBVHSSB, sizeof(tree::Node) * Graphics::MAX_ENTRIES * 2);
					
					GenerateFrameBuffers(g);
					GenNoiseTexture(256, 256, &noiseTex);

					g->downsampleShader = downShader;
					g->blurShader = blurShader;
					g->sceneShader = sceneShader;
					g->sceneEntrySSB = sceneEntrySSB;
					g->sceneBVHSSB = sceneBVHSSB;
					g->sceneVAO = sceneVAO;
					g->outputShader = outShader;

					g->noiseTex = noiseTex;

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
					g->meshTypeRemap[static_cast<uint>(gfx::MeshType::FIRE_BALL)] = MESH_TYPE_FIRE_BALL;
					g->meshTypeRemap[static_cast<uint>(gfx::MeshType::STATIC_PLATFORMS)] = MESH_TYPE_STATIC_PLATFORMS;
					g->meshTypeRemap[static_cast<uint>(gfx::MeshType::WORLD_BOUNDS)] = MESH_TYPE_WORLD_BOUNDS;
					g->meshTypeRemap[static_cast<uint>(gfx::MeshType::GROUND_PLANE)] = MESH_TYPE_GROUND_PLANE;
					g->meshTypeRemap[static_cast<uint>(gfx::MeshType::TREE)] = MESH_TYPE_TREE;

					return true;
				}

				return false;
			}
		}

		static void Render_Scene(const Graphics& g)
		{
			const GLenum sceneAttachments[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };

			glUseProgram(g.sceneShader);

			glBindFramebuffer(GL_FRAMEBUFFER, g.sceneFBO);
			glBindTexture(GL_TEXTURE_2D, g.noiseTex);
			scene::UpdateUniforms(g);

			glDrawBuffers(2, sceneAttachments);

			glBindVertexArray(g.sceneVAO);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

			glBindVertexArray(0);
			glDrawBuffers(1, sceneAttachments);
			glBindTexture(GL_TEXTURE_2D, 0);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glUseProgram(0);
		}

		static void Render_Downsample(const Graphics& g)
		{
			const GLenum downsampleAttachemnts[] = { GL_COLOR_ATTACHMENT0 };
			uint w = static_cast<uint>(g.res.x);
			uint h = static_cast<uint>(g.res.y);

			for (uint p = 1; p < BLOOM_BLUR_PASSES; ++p)
			{
				static const uint resolutionBinding = 1;

				w >>= 1;
				h >>= 1;

				glm::vec2 res(static_cast<float>(w), static_cast<float>(h));

				glViewport(0, 0, w, h);
				glUseProgram(g.downsampleShader);
				glBindFramebuffer(GL_FRAMEBUFFER, g.downsampleFBO[p]);
				glBindTexture(GL_TEXTURE_2D, g.sceneBrightTex[p - 1]);
				glUniform1i(0, 0);
				glUniform2fv(resolutionBinding, 1, glm::value_ptr(res));

				glDrawBuffers(1, downsampleAttachemnts);

				glBindVertexArray(g.sceneVAO);
				glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

				glBindVertexArray(0);
				glBindTexture(GL_TEXTURE_2D, 0);
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				glUseProgram(0);
			}
		}

		static void Render_Blur(const Graphics& g)
		{
			GLenum blurAttachemnts[Graphics::BLUR_PASSES] = { GL_COLOR_ATTACHMENT0 };

			glUseProgram(g.blurShader);

			for (uint pass = 0; pass < 2; ++pass)
			{
				uint w = static_cast<uint>(g.res.x);
				uint h = static_cast<uint>(g.res.y);

				for (uint p = 0; p < Graphics::BLUR_PASSES; ++p)
				{
					glViewport(0, 0, w, h);
					glBindFramebuffer(GL_FRAMEBUFFER, pass ? g.downsampleFBO[p] : g.blurFBO[p]);
					glBindTexture(GL_TEXTURE_2D, pass ? g.blurTex[p] : g.sceneBrightTex[p]);
					glUniform1i(0, 0);

					glm::vec2 res(static_cast<float>(w), static_cast<float>(h));
					static const uint horizBinding = 10;
					static const uint resolutionBinding = 11;

					glUniform1i(horizBinding, pass);
					glUniform2fv(resolutionBinding, 1, glm::value_ptr(res));

					glDrawBuffers(Graphics::BLUR_PASSES, blurAttachemnts);

					glBindVertexArray(g.sceneVAO);
					glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);


					glBindVertexArray(0);
					glBindTexture(GL_TEXTURE_2D, 0);
					glBindFramebuffer(GL_FRAMEBUFFER, 0);

					w >>= 1;
					h >>= 1;
				}
			}
		}

		static void Render_Output(const Graphics& g)
		{
			glViewport(0, 0, static_cast<uint>(g.res.x), static_cast<uint>(g.res.y));
			glUseProgram(g.outputShader);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, g.sceneMainTex);
			glUniform1i(0, 0);

			for (uint p = 0; p < Graphics::BLUR_PASSES; ++p)
			{
				const uint texIndex = p + 1;

				glActiveTexture(GL_TEXTURE0 + texIndex);
				glBindTexture(GL_TEXTURE_2D, g.sceneBrightTex[p]);
				glUniform1i(texIndex, texIndex);
			}

			static const uint resolutionBinding = 11;
			glUniform2fv(resolutionBinding, 1, glm::value_ptr(g.res));

			glBindVertexArray(g.sceneVAO);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

			glBindVertexArray(0); 
			for (uint p = Graphics::BLUR_PASSES; p; --p)
			{
				glActiveTexture(GL_TEXTURE0 + p);
				glBindTexture(GL_TEXTURE_2D, 0);
			}

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, 0);
			glUseProgram(0);
		}

		static void Render(const Graphics &g)
		{
			Render_Scene(g);
			Render_Downsample(g);
			Render_Blur(g);
			Render_Output(g);

			glBindVertexArray(0);
			glUseProgram(0);

			err::Error();
		}
	}
}

namespace gfx
{
	Graphics* Init()
	{
		Graphics* g = new Graphics;

		//glEnable(GL_DEBUG_OUTPUT);
		//glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		//glDebugMessageCallback((GLDEBUGPROC )&err::DebugOutput, nullptr);
		//glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);

		if (!render::init::Init(g))
		{
			printf("Graphics failed to initialize.\n");
			delete g;
			g = nullptr;
		}

		return g;
	}

	void Shutdown(Graphics* g)
	{
		render::init::DestroyFrameBuffers(g);

		glDeleteProgram(g->sceneShader);
		glDeleteProgram(g->blurShader);
		glDeleteProgram(g->outputShader);
		glDeleteBuffers(1, &g->sceneFBO);
		glDeleteBuffers(Graphics::BLUR_PASSES, g->blurFBO);
		glDeleteBuffers(Graphics::BLUR_PASSES, g->downsampleFBO);
		glDeleteBuffers(1, &g->sceneEntrySSB);
		glDeleteBuffers(1, &g->sceneBVHSSB);
		glDeleteTextures(1, &g->noiseTex);
		glDeleteTextures(1, &g->sceneMainTex);
		glDeleteTextures(Graphics::BLUR_PASSES, g->sceneBrightTex);
		glDeleteTextures(Graphics::BLUR_PASSES, g->blurTex);
		glDeleteVertexArrays(1, &g->sceneVAO);

		delete g;
	}

	void Update(Graphics* g)
	{
		++g->frameCount;
		render::Render(*g); 
	}
	 
	bool Resize(Graphics* g, uint x, uint y)
	{
		g->res = glm::vec2(static_cast<float>(x), static_cast<float>(y));
		render::init::DestroyFrameBuffers(g);
		return render::init::GenerateFrameBuffers(g);
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
				glm::mat4 trans = transforms[objIndex];
				const float meshType = (*outTrans)[3][3];

				switch (static_cast<uint>(meshType))
				{
				case MESH_TYPE_SNOW_FLAKE:
					trans[3][2] += 0.3;
					break;
				}

				*outTrans = glm::inverse(trans);
				(*outTrans)[3][3] = meshType;
			}
		}

		g->sceneBVH = tree::BuildSceneBVH(*g);

		return true;
	}

	void UpdateModelSubType(Graphics* g, uint objectId, float subType)
	{
		glm::mat4* const outTrans = g->sceneEntries.for_object(objectId);

		if (outTrans)
		{
			assert(subType >= 0.0f && subType < 1.0f);

			(*outTrans)[3][3] = std::floor((*outTrans)[3][3]) + subType;
		}
	}

	void DestroyObjects( Graphics* g, const std::vector<uint>& objectIds )
	{
		g->sceneEntries.remove_objs(objectIds);
	}

}