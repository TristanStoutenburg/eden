#include "eden_platform.h"

#include <stdlib.h>
#include <stdio.h>

#include <math.h> // todo tks use built in function
#define PI32 3.14159265329
#define ERROR_MESSAGE_SIZE 1024

typedef struct {
	char errorMessage[1024];
	int blueOffset;
	int greenOffset;
	int tonePeriod;
	int16_t toneVolume;
	int runningSampleIndex;
} EdnGameState;

char* ednGetError(EdnPlatformState ednPlatformState) {
	return ((EdnGameState *)ednPlatformState.gamePermanentData)->errorMessage;
}

int ednInit(EdnPlatformState ednPlatformState) {
	EdnGameState *ednGameState = (EdnGameState *)ednPlatformState.gamePermanentData;
	ednGameState->blueOffset = 0;
	ednGameState->greenOffset = 0;
	ednGameState->toneVolume = 1000;
	ednGameState->tonePeriod = ednPlatformState.audioSamplesPerSecond / 256;

	return 0;
}

int ednUpdateFrame(EdnPlatformState ednPlatformState) {
	EdnGameState *ednGameState = (EdnGameState *)ednPlatformState.gamePermanentData;

	// render weird grandient
	{
		if (ednPlatformState.ednInput.isWPressed) {
			ednGameState->greenOffset += 10;
		} 
		if (ednPlatformState.ednInput.isAPressed) {
			ednGameState->blueOffset += 10;
		} 
		if (ednPlatformState.ednInput.isSPressed) {
			ednGameState->greenOffset -= 10;
		} 
		if (ednPlatformState.ednInput.isDPressed) {
			ednGameState->blueOffset -= 10;
		}

		uint8_t *row = (uint8_t *)ednPlatformState.imageFrameData;

		for (int y = 0; y < ednPlatformState.imageHeight; y++) {
			uint32_t *pixel = (uint32_t *)row;
			for (int x = 0; x < ednPlatformState.imageWidth; x++) {
				uint8_t blue = x + ednGameState->blueOffset;
				uint8_t green = y + ednGameState->greenOffset;
				*pixel++ = ((green << 16) | (blue));
			}
			row += ednPlatformState.imageFrameDataPitch;
		}
	}

	// fill audio
	{
		for (int sampleIndex = 0; sampleIndex < ednPlatformState.audioFrameDataSize; sampleIndex++) {
#if 0
			ednPlatformState.audioFrameData[sampleIndex] = 
				(int16_t)
				(ednGameState->toneVolume * sinf(2.0f * PI32 * (float)ednGameState->runningSampleIndex / (float)ednGameState->tonePeriod));
			ednGameState->runningSampleIndex += sampleIndex % 2;
#endif
			ednPlatformState.audioFrameData[sampleIndex] = 0;
		}
	}

	return 0;
}









