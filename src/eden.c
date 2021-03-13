// filename eden.c
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
	float a;
	float r;
	float g;
	float b;
} EdnColor4f;

uint32_t ednColor4ftoPackedUInt32(EdnColor4f color4f) {
	return (ednRoundFloatToUInt32(color4f.a * 255.0f) << 24) |
		   (ednRoundFloatToUInt32(color4f.r * 255.0f) << 16) |
		   (ednRoundFloatToUInt32(color4f.g * 255.0f) <<  8) |
		   (ednRoundFloatToUInt32(color4f.b * 255.0f) <<  0);
}

void packedUInt32ToEdnColor4f(uint32_t packedUInt32, EdnColor4f *dest) {
	dest->a = ((packedUInt32 & 0xFF000000) >> 24) / 255.0f;
	dest->r = ((packedUInt32 & 0x00FF0000) >> 16) / 255.0f;
	dest->g = ((packedUInt32 & 0x0000FF00) >>  8) / 255.0f;
	dest->b = ((packedUInt32 & 0x000000FF) >>  0) / 255.0f;
}

void ednDrawRect(EdnPlatformState ednPlatformState, float fMinX, float fMaxX, float fMinY, float fMaxY, EdnColor4f color4f) {
	int32_t minX = ednRoundFloatToUInt32(fMinX);
	if (minX < 0) minX = 0;

	int32_t maxX = ednRoundFloatToUInt32(fMaxX);
	if (maxX > ednPlatformState.imageWidth) maxX = ednPlatformState.imageWidth;

	int32_t minY = ednRoundFloatToUInt32(fMinY);
	if (minY < 0) minY = 0;

	int32_t maxY = ednRoundFloatToUInt32(fMaxY);
	if (maxY > ednPlatformState.imageHeight) maxY = ednPlatformState.imageHeight;

	uint32_t colorUint32 = ednColor4ftoPackedUInt32(color4f);


	void *row = ednPlatformState.imageFrameData
					+ (minX * ednPlatformState.imageBytesPerPixel)
					+ (minY * ednPlatformState.imageFrameDataPitch);

	for (int32_t yIndex = minY; yIndex < maxY; yIndex++) {
		uint32_t *pixel = row;
		for (int32_t xIndex = minX; xIndex < maxX; xIndex++) {
			*pixel = colorUint32;
			pixel++;
		}

		row += ednPlatformState.imageFrameDataPitch;
	}

}

typedef struct {
	char *bmpSignature;
	uint32_t bmpFileSize;
	uint32_t bmpReserved;
	uint32_t bmpDataOffset;

	uint32_t bmpInfoHeaderSize;
	uint32_t bmpWidth;
	uint32_t bmpHeight;
	uint16_t bmpPlanes;
	uint16_t bmpBitsPerPixel;
	uint32_t bmpCompression;
	uint32_t bmpImageSize;
	uint64_t bmpInfoUnused;

	uint32_t alphaMask;
	uint32_t redMask;
	uint32_t greenMask;
	uint32_t blueMask;

	void *bmpData;
} Bmp;

void parseBmp(void *data, Bmp *dest) {
	dest->bmpSignature = (char *)data;
	dest->bmpFileSize = *(uint32_t *)(data +  2);
	dest->bmpReserved = *(uint32_t *)(data +  6);
	dest->bmpDataOffset = *(uint32_t *)(data +  10);
	dest->bmpInfoHeaderSize = *(uint32_t *)(data + 14);
	dest->bmpWidth = *(uint32_t *)(data + 18);
	dest->bmpHeight = *(uint32_t *)(data + 22);
	dest->bmpPlanes = *(uint16_t *)(data + 26);
	dest->bmpBitsPerPixel = *(uint16_t *)(data + 28);
	dest->bmpCompression = *(uint32_t *)(data + 30);
	dest->bmpImageSize = *(uint32_t *)(data + 34);
	dest->bmpInfoUnused = *(uint64_t *)(data + 38);
	// todo we can do this smarterly
	dest->alphaMask = 0xFF000000;
	dest->redMask = 0x00FF0000;
	dest->greenMask = 0x0000FF00;
	dest->blueMask = 0x000000FF;
	dest->bmpData = data + dest->bmpDataOffset;
}

void ednDrawBmp(EdnPlatformState ednPlatformState, float fMinX, float fMaxX, float fMinY, float fMaxY, Bmp bmp) {
	int32_t minX = ednRoundFloatToUInt32(fMinX);
	if (minX < 0) minX = 0;

	int32_t maxX = ednRoundFloatToUInt32(fMaxX);
	if (maxX > ednPlatformState.imageWidth) maxX = ednPlatformState.imageWidth;
	if (maxX > minX + bmp.bmpWidth) maxX = minX + bmp.bmpWidth;

	int32_t minY = ednRoundFloatToUInt32(fMinY);
	if (minY < 0) minY = 0;

	int32_t maxY = ednRoundFloatToUInt32(fMaxY);
	if (maxY > ednPlatformState.imageHeight) maxY = ednPlatformState.imageHeight;
	if (maxY > minY + bmp.bmpHeight) maxY = minY + bmp.bmpHeight;

	void *row = ednPlatformState.imageFrameData
					+ (minX * ednPlatformState.imageBytesPerPixel)
					+ (minY * ednPlatformState.imageFrameDataPitch);

	// the bmp data is (bottom left = 0), with (right = +x) and (up = +y)
	uint32_t *bmpRow = ((uint32_t *)bmp.bmpData) + ((bmp.bmpHeight - 1) * bmp.bmpWidth);
	EdnColor4f ednColor4f;
	for (int32_t yIndex = minY; yIndex < maxY; yIndex++) {
		uint32_t *pixel = row;
		uint32_t *bmpPixel = bmpRow;
		for (int32_t xIndex = minX; xIndex < maxX; xIndex++) {
			if ((*bmpPixel & bmp.alphaMask) >> 24 == 255) {
				// todo tks alpha multiply
			    *pixel = *bmpPixel;
			}
			pixel++;
			bmpPixel++;
		}

		row += ednPlatformState.imageFrameDataPitch;
		bmpRow -= bmp.bmpWidth;
	}
}


int ednUpdateFrame(EdnPlatformState ednPlatformState) {
	EdnGameState *ednGameState = (EdnGameState *)ednPlatformState.gamePermanentData;

	// clear to black
	{
		EdnColor4f clearColor;
		clearColor.a = 1.0f;
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
			{ 1, 0, 0, 1,  0, 0, 0, 1,  0, 0, 0, 0,  0, 0, 0, 1},
			{ 1, 0, 0, 1,  0, 0, 0, 1,  0, 0, 0, 0,  0, 0, 0, 1},
			{ 1, 0, 0, 1,  0, 0, 0, 1,  0, 0, 1, 1,  1, 1, 0, 1},
			{ 1, 0, 0, 1,  0, 0, 0, 0,  0, 0, 0, 1,  0, 0, 0, 0},
			{ 1, 0, 0, 0,  0, 0, 0, 1,  0, 0, 0, 1,  0, 0, 0, 1},
			{ 1, 0, 0, 0,  0, 0, 0, 1,  0, 0, 0, 1,  0, 0, 0, 1},
			{ 1, 0, 0, 0,  0, 0, 0, 1,  0, 0, 0, 0,  0, 0, 0, 1},
			{ 1, 1, 1, 1,  1, 1, 1, 1,  0, 1, 1, 1,  1, 0, 1, 1}
		};

		for (int tileRow = 0; tileRow < 9; tileRow++) {
			for (int tileColumn = 0; tileColumn < 16; tileColumn++) {
				EdnColor4f tileColor4f;
				if (tileMap[tileRow][tileColumn] == 0) {
					tileColor4f.a = 1.0f;
					tileColor4f.r = 0.0f;
					tileColor4f.g = 0.0f;
					tileColor4f.b = 0.0f;
				} else {
					tileColor4f.a = 1.0f;
					tileColor4f.r = 0.7f;
					tileColor4f.g = 0.7f;
					tileColor4f.b = 0.7f;
				}
				float tileWidth = ((float) ednPlatformState.imageWidth) / 16.0f;
				float tileHeight = ((float) ednPlatformState.imageHeight) / 9.0f;
				ednDrawRect(ednPlatformState,
						tileWidth * (float)(tileColumn), tileWidth * (float)(tileColumn + 1),
						tileHeight * (float)(tileRow), tileHeight * (float)(tileRow + 1),
						tileColor4f);
			}
		}

		EdnColor4f playerColor;

		// todo tks in progress bmp code:
		Bmp bmp;
		parseBmp(ednPlatformState.gameTransientData, &bmp);
		ednDrawBmp(ednPlatformState, ednGameState->playerXPosition, ednGameState->playerXPosition + 50.0f,
				ednGameState->playerYPosition, ednGameState ->playerYPosition + 50.0f,
				bmp);

		// playerColor.a = 1.0f;
		// playerColor.r = 0.5f;
		// playerColor.g = 0.0f;
		// playerColor.b = 1.0f;
		// ednDrawRect(ednPlatformState, ednGameState->playerXPosition, ednGameState->playerXPosition + 40.0f,
				// ednGameState->playerYPosition, ednGameState ->playerYPosition + 40.0f,
				// playerColor);
	}

	// fill audio
	{

		if (ednPlatformState.ednInput.isMPressed == 1 && ednGameState->toneVolume != 0) {
			ednGameState->toneVolume = 0;

		} else if (ednPlatformState.ednInput.isMPressed == 1 && ednGameState->toneVolume == 0) {
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









