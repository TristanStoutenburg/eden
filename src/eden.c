#include "eden_platform.h"

#include <stdlib.h>
#include <stdio.h>

typedef struct {
	int blueOffset;
	int greenOffset;
	int tonePeriod;
	int16_t toneVolume;
	int runningSampleIndex;
} EdnGameState;

void ednUpdateFrame(EdnPlatformState ednPlatformState) {
	EdnGameState *ednGameState = (EdnGameState *)ednPlatformState.gameData;

	// render weird grandient
	{
		if (ednPlatformState.isWPressed) {
			ednGameState->greenOffset += 10;
		} 
		if (ednPlatformState.isAPressed) {
			ednGameState->blueOffset += 10;
		} 
		if (ednPlatformState.isSPressed) {
			ednGameState->greenOffset -= 10;
		} 
		if (ednPlatformState.isDPressed) {
			ednGameState->blueOffset -= 10;
		}

		uint8_t *row = (uint8_t *)ednPlatformState.imageFrameData;

		for (int y = 0; y < ednPlatformState.imageHeight; y++) {
			uint32_t *pixel = (uint32_t *)row;
			for (int x = 0; x < ednPlatformState.imageWidth; x++) {
				uint8_t blue = x + ednGameState->blueOffset;
				uint8_t green = y + ednGameState->greenOffset;
				*pixel++ = ((green << 8) | (blue));
			}
			row += ednPlatformState.imageFrameDataPitch;
		}
	}

}

//	// fill audio
//	{
//		// todo tks this is currently not being used, but might be useful later...
//
//		// these could be constants...
//		int toneVolume = 3000;
//		int tonePeriod = ednPlatformState.audioSamplesPerSecond / 256;
//
//		// maybe we should just do number of samples as the variable...
//		int audioSampleCount = ednPlatformState.audioFrameDataSize / ednPlatformState.audioBytesPerSample;
//
//		int16_t *sampleOut = (int16_t *)ednPlatformState.audioFrameData;
//		for(int sampleIndex = 0; sampleIndex < audioSampleCount; ++sampleIndex) {
//			float t = 2.0f * PI32 * (float)ednGameState->runningSampleIndex / (float)tonePeriod;
//			float sinValue = sinf(t);
//			ednGameState->runningSampleIndex++;
//			int16_t sampleValue = (int16_t) (sinValue * toneVolume);
//			*sampleOut++ = sampleValue;
//			*sampleOut++ = sampleValue;
//		}
//	}
