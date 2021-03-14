#include <math.h> // todo tks get rid of this...

#include "eden_platform.h"

 
#define PI32 3.14159265329

#define ERROR_MESSAGE_SIZE 1024

typedef struct {
	char errorMessage[1024];
	int32 tonePeriod;
	int16 toneVolume;
	int32 runningSampleIndex;
	real32 playerXPosition;
	real32 playerYPosition;
} EdnGameState;

char* ednGetError(EdnPlatformState *ednPlatformState) {
	return ((EdnGameState *)ednPlatformState->gamePermanentData)->errorMessage;
}

void ednStringCopy(char *destination, char *source, int32 stringLength) {
	for (int32 stringIndex = 0; stringIndex < stringLength; stringIndex++) {
		if (source[stringIndex] == '\0') return;
		destination[stringIndex] = source[stringIndex];
	}
}

int32 ednInit(EdnPlatformState *ednPlatformState) {
	ednPlatformState->ednAssetCount = 0;
	for (int32 assetIndex; assetIndex < EDN_ASSET_MAX; assetIndex++) {
		ednPlatformState->ednAssets[assetIndex].ednAssetStatus = 0;
	}


	EdnGameState *ednGameState = (EdnGameState *)ednPlatformState->gamePermanentData;
	ednGameState->toneVolume = 0;
	ednGameState->tonePeriod = ednPlatformState->audioSamplesPerSecond / 256;

	return 0;
}

int32 ednRoundFloatToInt32(real32 fValue) {
	return (int32)(fValue + 0.5f);
}

uint32 ednRoundFloatToUInt32(real32 fValue) {
	return (uint32)(fValue + 0.5f);
}


typedef struct {
	real32 a;
	real32 r;
	real32 g;
	real32 b;
} EdnColor4f;

uint32 ednColor4ftoPackedUInt32(EdnColor4f color4f) {
	return (ednRoundFloatToUInt32(color4f.a * 255.0f) << 24) |
		   (ednRoundFloatToUInt32(color4f.r * 255.0f) << 16) |
		   (ednRoundFloatToUInt32(color4f.g * 255.0f) <<  8) |
		   (ednRoundFloatToUInt32(color4f.b * 255.0f) <<  0);
}

void packedUInt32ToEdnColor4f(uint32 packedUInt32, EdnColor4f *dest) {
	dest->a = ((packedUInt32 & 0xFF000000) >> 24) / 255.0f;
	dest->r = ((packedUInt32 & 0x00FF0000) >> 16) / 255.0f;
	dest->g = ((packedUInt32 & 0x0000FF00) >>  8) / 255.0f;
	dest->b = ((packedUInt32 & 0x000000FF) >>  0) / 255.0f;
}

void ednDrawRect(EdnPlatformState *ednPlatformState, real32 fMinX, real32 fMaxX, real32 fMinY, real32 fMaxY, EdnColor4f color4f) {
	int32 minX = ednRoundFloatToUInt32(fMinX);
	if (minX < 0) minX = 0;

	int32 maxX = ednRoundFloatToUInt32(fMaxX);
	if (maxX > ednPlatformState->imageWidth) maxX = ednPlatformState->imageWidth;

	int32 minY = ednRoundFloatToUInt32(fMinY);
	if (minY < 0) minY = 0;

	int32 maxY = ednRoundFloatToUInt32(fMaxY);
	if (maxY > ednPlatformState->imageHeight) maxY = ednPlatformState->imageHeight;

	uint32 colorUint32 = ednColor4ftoPackedUInt32(color4f);


	void *row = ednPlatformState->imageFrameData
					+ (minX * ednPlatformState->imageBytesPerPixel)
					+ (minY * ednPlatformState->imageFrameDataPitch);

	for (int32 yIndex = minY; yIndex < maxY; yIndex++) {
		uint32 *pixel = row;
		for (int32 xIndex = minX; xIndex < maxX; xIndex++) {
			*pixel = colorUint32;
			pixel++;
		}

		row += ednPlatformState->imageFrameDataPitch;
	}

}

typedef struct {
	char *bmpSignature;
	uint32 bmpFileSize;
	uint32 bmpReserved;
	uint32 bmpDataOffset;
	uint32 bmpInfoHeaderSize;
	uint32 bmpWidth;
	uint32 bmpHeight;
	uint16 bmpPlanes;
	uint16 bmpBitsPerPixel;
	uint32 bmpCompression;
	uint32 bmpImageSize;
	uint64 bmpInfoUnused;
	// some header fields have been excluded
	void *bmpData;
} Bmp;

void parseBmp(void *data, Bmp *dest) {
	dest->bmpSignature = (char *)data;
	dest->bmpFileSize = *(uint32 *)(data +  2);
	dest->bmpReserved = *(uint32 *)(data +  6);
	dest->bmpDataOffset = *(uint32 *)(data +  10);
	dest->bmpInfoHeaderSize = *(uint32 *)(data + 14);
	dest->bmpWidth = *(uint32 *)(data + 18);
	dest->bmpHeight = *(uint32 *)(data + 22);
	dest->bmpPlanes = *(uint16 *)(data + 26);
	dest->bmpBitsPerPixel = *(uint16 *)(data + 28);
	dest->bmpCompression = *(uint32 *)(data + 30);
	dest->bmpImageSize = *(uint32 *)(data + 34);
	dest->bmpInfoUnused = *(uint64 *)(data + 38);
	// todo we can do this smarterly
	dest->bmpData = data + dest->bmpDataOffset;
}

void ednDrawBmp(EdnPlatformState *ednPlatformState, real32 fMinX, real32 fMaxX, real32 fMinY, real32 fMaxY, Bmp bmp) {
	int32 minX = ednRoundFloatToUInt32(fMinX);
	if (minX < 0) minX = 0;

	int32 maxX = ednRoundFloatToUInt32(fMaxX);
	if (maxX > ednPlatformState->imageWidth) maxX = ednPlatformState->imageWidth;
	if (maxX > minX + bmp.bmpWidth) maxX = minX + bmp.bmpWidth;

	int32 minY = ednRoundFloatToUInt32(fMinY);
	if (minY < 0) minY = 0;

	int32 maxY = ednRoundFloatToUInt32(fMaxY);
	if (maxY > ednPlatformState->imageHeight) maxY = ednPlatformState->imageHeight;
	if (maxY > minY + bmp.bmpHeight) maxY = minY + bmp.bmpHeight;

	void *row = ednPlatformState->imageFrameData
					+ (minX * ednPlatformState->imageBytesPerPixel)
					+ (minY * ednPlatformState->imageFrameDataPitch);

	// the bmp data is (bottom left = 0), with (right = +x) and (up = +y)
	uint32 *bmpRow = ((uint32 *)bmp.bmpData) + ((bmp.bmpHeight - 1) * bmp.bmpWidth);
	EdnColor4f ednColor4f;
	for (int32 yIndex = minY; yIndex < maxY; yIndex++) {
		uint32 *pixel = row;
		uint32 *bmpPixel = bmpRow;
		for (int32 xIndex = minX; xIndex < maxX; xIndex++) {
			if ((*bmpPixel & 0xFF000000) >> 24 == 255) {
				// todo tks alpha multiply
			    *pixel = *bmpPixel;
			}
			pixel++;
			bmpPixel++;
		}

		row += ednPlatformState->imageFrameDataPitch;
		bmpRow -= bmp.bmpWidth;
	}
}


int32 ednUpdateFrame(EdnPlatformState *ednPlatformState) {
	EdnGameState *ednGameState = (EdnGameState *)ednPlatformState->gamePermanentData;

	// clear to black
	{
		EdnColor4f clearColor;
		clearColor.a = 1.0f;
		clearColor.r = 1.0f;
		clearColor.g = 1.0f;
		clearColor.b = 0.0f;
		ednDrawRect(ednPlatformState, 0.0f, (real32)ednPlatformState->imageWidth, 0.0f, (real32)ednPlatformState->imageHeight, clearColor);

	}

	// player position change
	{
		ednGameState->playerXPosition += ednPlatformState->ednInput.isAPressed ? -5.0f : 0.0f;
		ednGameState->playerXPosition += ednPlatformState->ednInput.isDPressed ?  5.0f : 0.0f;
		ednGameState->playerYPosition += ednPlatformState->ednInput.isWPressed ? -5.0f : 0.0f;
		ednGameState->playerYPosition += ednPlatformState->ednInput.isSPressed ?  5.0f : 0.0f;
	}

	// draw the player and tile map
	{

		uint32 tileMap[9][16] = {
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

		for (int32 tileRow = 0; tileRow < 9; tileRow++) {
			for (int32 tileColumn = 0; tileColumn < 16; tileColumn++) {
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
				real32 tileWidth = ((real32) ednPlatformState->imageWidth) / 16.0f;
				real32 tileHeight = ((real32) ednPlatformState->imageHeight) / 9.0f;
				ednDrawRect(ednPlatformState,
						tileWidth * (real32)(tileColumn), tileWidth * (real32)(tileColumn + 1),
						tileHeight * (real32)(tileRow), tileHeight * (real32)(tileRow + 1),
						tileColor4f);
			}
		}

		// todo tks better assets/file io requests
		if (ednPlatformState->ednAssets[0].ednAssetStatus == LOADED) {
			Bmp bmp;
			parseBmp(ednPlatformState->ednAssets[0].ednAssetData, &bmp);
			ednDrawBmp(ednPlatformState, ednGameState->playerXPosition, ednGameState->playerXPosition + 50.0f,
					ednGameState->playerYPosition, ednGameState ->playerYPosition + 50.0f,
					bmp);
		} else {
			ednPlatformState->ednAssets[0].ednAssetStatus = LOADING;
			ednStringCopy(ednPlatformState->ednAssets[0].fileName, "../assets/test.bmp", EDN_ASSET_NAME_MAX);
			ednPlatformState->ednAssets[0].ednAssetData = ednPlatformState->gameTransientData;
			ednPlatformState->ednAssets[0].ednAssetDataByteCount = 50000;
			ednPlatformState->ednAssetCount = 1;
		}
	}



	// fill audio
	{

		if (ednPlatformState->ednAssets[1].ednAsetStatus == LOADED) {

		} else {
			// todo tks better memory management too
			// todo tks maybe an asset find? or an asset reserve? an asset free? for these file ids
			ednPlatformState->ednAssets[1].ednAssetStatus = LOADING;
			ednStringCopy(ednPlatformState->ednAssets[1].fileName, "../assets/test.wav", EDN_ASSET_NAME_MAX);
			ednPlatformState->ednAssets[1].ednAssetData = ednPlatformState->gameTransientData + 50000;
			ednPlatformState->ednAssets[1].ednAssetDataByteCount = 800000;
			ednPlatformState->ednAssetCount = 2;
		}


		if (ednPlatformState->ednInput.isMPressed == 1 && ednGameState->toneVolume != 0) {
			ednGameState->toneVolume = 0;

		} else if (ednPlatformState->ednInput.isMPressed == 1 && ednGameState->toneVolume == 0) {
			ednGameState->toneVolume = 1000;
		}

		for (int32 sampleIndex = 0; sampleIndex < ednPlatformState->audioFrameDataSize; sampleIndex++) {
			ednPlatformState->audioFrameData[sampleIndex] = 
				(int16)
				(ednGameState->toneVolume * sinf(2.0f * PI32 * (real32)ednGameState->runningSampleIndex / (real32)ednGameState->tonePeriod));
			ednGameState->runningSampleIndex += sampleIndex % 2;
		}
	}

	return 0;
}









