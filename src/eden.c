#include "eden_platform.h"

#include <stdlib.h>
#include <stdio.h>

#include <math.h> // todo tks use built in function
#define PI32 3.14159265329

typedef struct {
	int blueOffset;
	int greenOffset;
	int tonePeriod;
	int16_t toneVolume;
	int runningSampleIndex;
} EdnGameState;

int ednInit(EdnPlatformState ednPlatformState) {
	EdnGameState *ednGameState = (EdnGameState *)ednPlatformState.gameData;
	ednGameState->blueOffset = 0;
	ednGameState->greenOffset = 0;
	ednGameState->toneVolume = 3000;
	ednGameState->tonePeriod = ednPlatformState.audioSamplesPerSecond / 256;

	return 0;
}

int ednUpdateFrame(EdnPlatformState ednPlatformState) {
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

	// fill audio
	{
		// todo tks this is currently not being used, but might be useful later...
		ednGameState->runningSampleIndex = ednPlatformState.audioFrameDataSize * ednPlatformState.frameCount;
		for (int sampleIndex = 0; sampleIndex < ednPlatformState.audioFrameDataSize; sampleIndex++) {
			ednPlatformState.audioFrameData[sampleIndex] = 
				(uint8_t)ednGameState->toneVolume * sinf(2.0f * PI32 * (float)ednGameState->runningSampleIndex / (float)ednGameState->tonePeriod);
			ednGameState->runningSampleIndex += sampleIndex % 2;
		}
	}

	return 0;
}









