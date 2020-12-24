#include "eden_platform.h"

#include <stdlib.h>
#include <stdio.h>

typedef struct {
	int blueOffset;
	int greenOffset;
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
