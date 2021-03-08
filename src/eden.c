#include "eden_platform.h"

#include <stdlib.h>
#include <stdio.h>

#include <math.h> // todo tks use built in function
#define PI32 3.14159265329
#define ERROR_MESSAGE_SIZE 1024

typedef struct {
	char errorMessage[1024];
	int tonePeriod;
	int16_t toneVolume;
	int runningSampleIndex;
	float playerXPosition;
	float playerYPosition;
} EdnGameState;

char* ednGetError(EdnPlatformState ednPlatformState) {
	return ((EdnGameState *)ednPlatformState.gamePermanentData)->errorMessage;
}

int ednInit(EdnPlatformState ednPlatformState) {
	EdnGameState *ednGameState = (EdnGameState *)ednPlatformState.gamePermanentData;
	ednGameState->toneVolume = 0;
	ednGameState->tonePeriod = ednPlatformState.audioSamplesPerSecond / 256;

	return 0;
}

int32_t ednRoundFloatToInt32(float fValue) {
	return (int32_t)(fValue + 0.5f);
}

uint32_t ednRoundFloatToUInt32(float fValue) {
	return (uint32_t)(fValue + 0.5f);
}


typedef struct {
	float r;
	float g;
	float b;
} EdnColor3F;

uint32_t ednColor3FtoPackedUInt32(EdnColor3F color3F) {
	return (ednRoundFloatToUInt32(color3F.r * 255.0f) << 16) |
		   (ednRoundFloatToUInt32(color3F.g * 255.0f) <<  8) |
		   (ednRoundFloatToUInt32(color3F.b * 255.0f) <<  0);
}

void ednDrawRect(EdnPlatformState ednPlatformState, float fMinX, float fMaxX, float fMinY, float fMaxY, EdnColor3F color3F) {
	int32_t minX = ednRoundFloatToUInt32(fMinX);
	if (minX < 0) minX = 0;

	int32_t maxX = ednRoundFloatToUInt32(fMaxX);
	if (maxX > ednPlatformState.imageWidth) maxX = ednPlatformState.imageWidth;

	int32_t minY = ednRoundFloatToUInt32(fMinY);
	if (minY < 0) minY = 0;

	int32_t maxY = ednRoundFloatToUInt32(fMaxY);
	if (maxY > ednPlatformState.imageHeight) maxY = ednPlatformState.imageHeight;

	uint32_t colorUint8 = ednColor3FtoPackedUInt32(color3F);


	void *row = ednPlatformState.imageFrameData
					+ (minX * ednPlatformState.imageBytesPerPixel)
					+ (minY * ednPlatformState.imageFrameDataPitch);

	for (int32_t yIndex = minY; yIndex < maxY; yIndex++) {
		uint32_t *pixel = row;
		for (int32_t xIndex = minX; xIndex < maxX; xIndex++) {
			*pixel = colorUint8;
			pixel++;
		}

		row += ednPlatformState.imageFrameDataPitch;
	}

}


int ednUpdateFrame(EdnPlatformState ednPlatformState) {
	EdnGameState *ednGameState = (EdnGameState *)ednPlatformState.gamePermanentData;

	// clear to black
	{
		EdnColor3F clearColor;
		clearColor.r = 1.0f;
		clearColor.g = 1.0f;
		clearColor.b = 0.0f;
		ednDrawRect(ednPlatformState, 0.0f, (float)ednPlatformState.imageWidth, 0.0f, (float)ednPlatformState.imageHeight, clearColor);

	}

	// player position change
	{
		ednGameState->playerXPosition += ednPlatformState.ednInput.isAPressed ? -5.0f : 0.0f;
		ednGameState->playerXPosition += ednPlatformState.ednInput.isDPressed ?  5.0f : 0.0f;
		ednGameState->playerYPosition += ednPlatformState.ednInput.isWPressed ? -5.0f : 0.0f;
		ednGameState->playerYPosition += ednPlatformState.ednInput.isSPressed ?  5.0f : 0.0f;
	}

	// draw the player and tile map
	{

		uint32_t tileMap[9][16] = {
			{ 1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1,  1, 1, 1, 1},
			{ 1, 0, 0, 0,  0, 0, 0, 1,  0, 0, 0, 0,  0, 0, 0, 1},
			{ 1, 0, 0, 0,  0, 0, 0, 1,  0, 0, 0, 0,  0, 0, 0, 1},
			{ 1, 0, 0, 1,  0, 0, 0, 1,  0, 0, 1, 1,  1, 1, 0, 1},
			{ 1, 0, 0, 1,  0, 0, 0, 0,  0, 0, 0, 1,  0, 0, 0, 0},
			{ 1, 0, 0, 0,  0, 0, 0, 1,  0, 0, 0, 1,  0, 0, 0, 1},
			{ 1, 0, 0, 0,  0, 0, 0, 1,  0, 0, 0, 1,  0, 0, 0, 1},
			{ 1, 0, 0, 0,  0, 0, 0, 1,  0, 0, 0, 0,  0, 0, 0, 1},
			{ 1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 0, 1, 1}
		};

		for (int tileRow = 0; tileRow < 9; tileRow++) {
			for (int tileColumn = 0; tileColumn < 16; tileColumn++) {
				EdnColor3F tileColor3F;
				if (tileMap[tileRow][tileColumn] == 0) {
					tileColor3F.r = 0.0f;
					tileColor3F.g = 0.0f;
					tileColor3F.b = 0.0f;
				} else {
					tileColor3F.r = 0.7f;
					tileColor3F.g = 0.7f;
					tileColor3F.b = 0.7f;
				}
				float tileWidth = ((float) ednPlatformState.imageWidth) / 16.0f;
				float tileHeight = ((float) ednPlatformState.imageHeight) / 9.0f;
				ednDrawRect(ednPlatformState,
						tileWidth * (float)(tileColumn), tileWidth * (float)(tileColumn + 1),
						tileHeight * (float)(tileRow), tileHeight * (float)(tileRow + 1),
						tileColor3F);
			}
		}

		EdnColor3F playerColor;
		playerColor.r = 1.0f;
		playerColor.g = 1.0f;
		playerColor.b = 1.0f;
		ednDrawRect(ednPlatformState, ednGameState->playerXPosition, ednGameState->playerXPosition + 40.0f,
				ednGameState->playerYPosition, ednGameState ->playerYPosition + 40.0f,
				playerColor);
	}

	// fill audio
	{

		if (ednPlatformState.ednInput.isMPressed && ednGameState->toneVolume != 0) {
			ednGameState->toneVolume = 0;

		} else if (ednPlatformState.ednInput.isMPressed && ednGameState->toneVolume == 0) {
			ednGameState->toneVolume = 1000;
		}

		for (int sampleIndex = 0; sampleIndex < ednPlatformState.audioFrameDataSize; sampleIndex++) {
			ednPlatformState.audioFrameData[sampleIndex] = 
				(int16_t)
				(ednGameState->toneVolume * sinf(2.0f * PI32 * (float)ednGameState->runningSampleIndex / (float)ednGameState->tonePeriod));
			ednGameState->runningSampleIndex += sampleIndex % 2;
		}
	}

	return 0;
}









