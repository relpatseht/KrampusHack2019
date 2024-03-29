#include <filesystem>
#include <iostream>
#include <cstdio>

namespace
{
	static void WavToC(const char* inFile, const char *baseName, const char* outHeader, const char *outSource)
	{
		FILE* in = fopen(inFile, "rb");
		
		if (in)
		{
			size_t inSize;

			fseek(in, 0, SEEK_END);
			inSize = ftell(in);
			fseek(in, 0, SEEK_SET);

			uint8_t* const inBuf = (uint8_t*)malloc(inSize);
			fread(inBuf, 1, inSize, in);

			struct WavHeader
			{
				uint8_t riff[4];
				uint32_t totalSize;
				uint8_t wave[4];
				uint8_t fmtChunkMarker[4];
				uint32_t lengthOfFmt;
				uint16_t formatType;
				uint16_t channelCount;
				uint32_t sampleRate;
				uint32_t byteRate;
				uint16_t blockAlign;
				uint16_t bitsPerSample;
				uint8_t dataChunkHeader[4];
				uint32_t dataSize;
			};

			const WavHeader* const header = (WavHeader*)inBuf;

			if (header->formatType != 1)
			{
				std::cout << "'" << inFile << "' isn't PCM. Skipping." << std::endl;
			}
			else
			{
				FILE* outH = fopen(outHeader, "wb");

				if (outH)
				{
					FILE* outS = fopen(outSource, "wb");
					const unsigned sampleCount = (header->dataSize * 8) / (header->channelCount * header->bitsPerSample);

					fprintf(outH, "#pragma once\r\n\r\n");
					fprintf(outH, "// Generated by WAVToC. Do not modify.\r\n\r\n");
					fprintf(outH, "extern const unsigned %s_channelCount;\r\n", baseName);
					fprintf(outH, "extern const unsigned %s_sampleRate;\r\n", baseName);
					fprintf(outH, "extern const unsigned %s_sampleCount;\r\n", baseName);
					fprintf(outH, "extern const unsigned %s_bitDepth;\r\n", baseName);
					fprintf(outH, "extern const void * const %s_pcmData;\r\n", baseName);

					fprintf(outS, "// Generated by WAVToC. Do not modify.\r\n\r\n");
					fprintf(outS, "static const unsigned char pcmData[] = {\r\n");
					const uint8_t* const pcmData = reinterpret_cast<uint8_t*>(inBuf + sizeof(WavHeader));
					const unsigned dataSize = header->dataSize;
					unsigned curOffset;

					for (curOffset = 0; curOffset < dataSize;)
					{
						fprintf(outS, "\t");
						for (unsigned lineIndex = 0; lineIndex < 16; ++lineIndex)
						{
							fprintf(outS, "0x%02X, ", pcmData[curOffset]);

							if (++curOffset >= dataSize)
								break;
						}

						fprintf(outS, "\r\n");
					}
					
					fprintf(outS, "};\r\n\r\n");

					fprintf(outS, "extern const unsigned %s_channelCount = %u;\r\n", baseName, header->channelCount);
					fprintf(outS, "extern const unsigned %s_sampleRate = %u;\r\n", baseName, header->sampleRate);
					fprintf(outS, "extern const unsigned %s_sampleCount = %u;\r\n", baseName, sampleCount);
					fprintf(outS, "extern const unsigned %s_bitDepth = %u;\r\n", baseName, header->bitsPerSample);
					fprintf(outS, "extern const void * const %s_pcmData = pcmData;\r\n", baseName);
					fclose(outS);
				}

				fclose(outH);
			}

			free(inBuf);
		}

		fclose(in);
	}
}

int main(int argc, char* argv[])
{
	if (argc < 3)
	{
		std::cout << "Usage:" << std::endl;
		std::cout << "wavtoc <input_dir> <output_dir>" << std::endl;
		return -1;
	}

	std::filesystem::path outDir(argv[2]);
	if (!std::filesystem::exists(outDir))
	{
		if (!std::filesystem::create_directories(outDir))
		{
			std::cout << "Failed to create '" << outDir.string() << "'" << std::endl;
			return -1;
		}
	}

	for (auto inFile : std::filesystem::directory_iterator(argv[1]))
	{
		if (inFile.is_regular_file())
		{
			const std::string filename = inFile.path().filename().replace_extension("").string();
			std::filesystem::path outPath = outDir / filename;

			outPath.make_preferred();
			outPath = outPath.lexically_normal();

			WavToC(inFile.path().string().c_str(), filename.c_str(), (outPath.string() + ".h").c_str(), (outPath.string() + ".cpp").c_str());
		}
	}

	return 0;
}