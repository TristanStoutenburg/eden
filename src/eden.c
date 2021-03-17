#include <math.h> // todo tks get rid of this...
#include <stdio.h>

#include "eden_platform.h"

 
#define PI32 3.14159265329

#define ERROR_MESSAGE_SIZE 1024

typedef struct {
	char errorMessage[1024];
	s32 tonePeriod;
	u16 toneVolume;
	s32 runningSampleIndex;
	f32 playerXPosition;
	f32 playerYPosition;
	u64 waveBytePosition;
} EdnGameState;

char* ednGetError(EdnPlatformState *ednPlatformState) {
	return ((EdnGameState *)ednPlatformState->gamePermanentData)->errorMessage;
}

void ednStringCopy(char *destination, char *source, s32 stringLength) {
	for (s32 stringIndex = 0; stringIndex < stringLength; stringIndex++) {
		if (source[stringIndex] == '\0') return;
		destination[stringIndex] = source[stringIndex];
	}
}

s32 ednInit(EdnPlatformState *ednPlatformState) {
	ednPlatformState->ednAssetCount = 0;
	for (s32 assetIndex; assetIndex < EDN_ASSET_MAX; assetIndex++) {
		ednPlatformState->ednAssets[assetIndex].ednAssetStatus = 0;
	}


	EdnGameState *ednGameState = (EdnGameState *)ednPlatformState->gamePermanentData;
	ednGameState->toneVolume = 0;
	ednGameState->tonePeriod = ednPlatformState->audioSamplesPerSecond / 256;
	ednGameState->waveBytePosition = 0;

	return 0;
}

s32 ednRoundFloatToInt32(f32 fValue) {
	return (s32)(fValue + 0.5f);
}

u32 ednRoundFloatToUInt32(f32 fValue) {
	return (u32)(fValue + 0.5f);
}


typedef struct {
	f32 a;
	f32 r;
	f32 g;
	f32 b;
} EdnColor4f;

u32 ednColor4ftoPackedUInt32(EdnColor4f color4f) {
	return (ednRoundFloatToUInt32(color4f.a * 255.0f) << 24) |
		   (ednRoundFloatToUInt32(color4f.r * 255.0f) << 16) |
		   (ednRoundFloatToUInt32(color4f.g * 255.0f) <<  8) |
		   (ednRoundFloatToUInt32(color4f.b * 255.0f) <<  0);
}

void packedUInt32ToEdnColor4f(u32 packedUInt32, EdnColor4f *dest) {
	dest->a = ((packedUInt32 & 0xFF000000) >> 24) / 255.0f;
	dest->r = ((packedUInt32 & 0x00FF0000) >> 16) / 255.0f;
	dest->g = ((packedUInt32 & 0x0000FF00) >>  8) / 255.0f;
	dest->b = ((packedUInt32 & 0x000000FF) >>  0) / 255.0f;
}

void ednDrawRect(EdnPlatformState *ednPlatformState, f32 fMinX, f32 fMaxX, f32 fMinY, f32 fMaxY, EdnColor4f color4f) {
	s32 minX = ednRoundFloatToUInt32(fMinX);
	if (minX < 0) minX = 0;

	s32 maxX = ednRoundFloatToUInt32(fMaxX);
	if (maxX > ednPlatformState->imageWidth) maxX = ednPlatformState->imageWidth;

	s32 minY = ednRoundFloatToUInt32(fMinY);
	if (minY < 0) minY = 0;

	s32 maxY = ednRoundFloatToUInt32(fMaxY);
	if (maxY > ednPlatformState->imageHeight) maxY = ednPlatformState->imageHeight;

	u32 colorUint32 = ednColor4ftoPackedUInt32(color4f);


	void *row = ednPlatformState->imageFrameData
					+ (minX * ednPlatformState->imageBytesPerPixel)
					+ (minY * ednPlatformState->imageFrameDataPitch);

	for (s32 yIndex = minY; yIndex < maxY; yIndex++) {
		u32 *pixel = row;
		for (s32 xIndex = minX; xIndex < maxX; xIndex++) {
			*pixel = colorUint32;
			pixel++;
		}

		row += ednPlatformState->imageFrameDataPitch;
	}

}

typedef struct {
	char *bmpSignature;
	u32 bmpFileSize;
	u32 bmpReserved;
	u32 bmpDataOffset;
	u32 bmpInfoHeaderSize;
	u32 bmpWidth;
	u32 bmpHeight;
	u16 bmpPlanes;
	u16 bmpBitsPerPixel;
	u32 bmpCompression;
	u32 bmpImageSize;
	u64 bmpInfoUnused;
	// some header fields have been excluded
	void *bmpData;
} EdnBmp;

void parseEdnBmp(void *data, EdnBmp *dest) {
	dest->bmpSignature = (char *)data;
	dest->bmpFileSize = *(u32 *)(data +  2);
	dest->bmpReserved = *(u32 *)(data +  6);
	dest->bmpDataOffset = *(u32 *)(data +  10);
	dest->bmpInfoHeaderSize = *(u32 *)(data + 14);
	dest->bmpWidth = *(u32 *)(data + 18);
	dest->bmpHeight = *(u32 *)(data + 22);
	dest->bmpPlanes = *(u16 *)(data + 26);
	dest->bmpBitsPerPixel = *(u16 *)(data + 28);
	dest->bmpCompression = *(u32 *)(data + 30);
	dest->bmpImageSize = *(u32 *)(data + 34);
	dest->bmpInfoUnused = *(u64 *)(data + 38);
	// todo we can do this smarterly
	dest->bmpData = data + dest->bmpDataOffset;
}

typedef struct {
	char wavRiffCkId[4];
	u32 wavCkSize;
	char wavWavCkId[4];
	char wavFmtCkId[4];
	u16 wavFmtCkSize;
	u8 wavFmtTag;
	u16 wavNumberChannels;
	u32 wavSamplesPerSecond;
	u32 wavAverageBytesPerSecond;
	u16 wavBlockAllign;
	u16 wavBitsPerSample;
	char wavDataCkId[4];
	u16 wavDataCkSize;
	void *wavDataSamples;
} EdnWav;

s32 parseEdnWav(void *wavData, EdnWav *dest) {
	s32 result = 0;

	// todo check file validity
	dest->wavRiffCkId[0]           = *(char *)(wavData +  0); // R
	dest->wavRiffCkId[1]           = *(char *)(wavData +  1); // I
	dest->wavRiffCkId[2]           = *(char *)(wavData +  2); // F
	dest->wavRiffCkId[3]           = *(char *)(wavData +  3); // F
	dest->wavCkSize                = *(u32  *)(wavData +  4);
	dest->wavWavCkId[0]            = *(char *)(wavData +  8); // W
	dest->wavWavCkId[1]            = *(char *)(wavData +  9); // A
	dest->wavWavCkId[2]            = *(char *)(wavData + 10); // V
	dest->wavWavCkId[3]            = *(char *)(wavData + 11); // E
	dest->wavFmtCkId[0]            = *(char *)(wavData + 12); // f
	dest->wavFmtCkId[1]            = *(char *)(wavData + 13); // m
	dest->wavFmtCkId[2]            = *(char *)(wavData + 14); // t
	dest->wavFmtCkId[3]            = *(char *)(wavData + 15); // ' '
	dest->wavFmtCkSize			   = *(u32  *)(wavData + 16); // must be 16
	dest->wavFmtTag                = *(u16  *)(wavData + 20); // must be 1
	dest->wavNumberChannels        = *(u16  *)(wavData + 22); // must be 2
	dest->wavSamplesPerSecond      = *(u32  *)(wavData + 24); // must be 44100
	dest->wavAverageBytesPerSecond = *(u32  *)(wavData + 28);
	dest->wavBlockAllign           = *(u16  *)(wavData + 32);
	dest->wavBitsPerSample         = *(u16  *)(wavData + 34); // must be 16
	dest->wavDataCkId[0]		   = *(char *)(wavData + 36); // d
	dest->wavDataCkId[1]		   = *(char *)(wavData + 37); // a
	dest->wavDataCkId[2]		   = *(char *)(wavData + 38); // t
	dest->wavDataCkId[3]		   = *(char *)(wavData + 39); // a
	dest->wavDataSamples		   =          (wavData + 40);

	if (      dest->wavRiffCkId[0] != 'R' 
		   || dest->wavRiffCkId[1] != 'I' 
		   || dest->wavRiffCkId[2] != 'F' 
		   || dest->wavRiffCkId[3] != 'F') {
		printf("no riff id %c%c%c%c\n",
				dest->wavRiffCkId[0],
				dest->wavRiffCkId[1],
				dest->wavRiffCkId[2],
				dest->wavRiffCkId[3]);
		result++;
	}

	if (      dest->wavWavCkId[0] != 'W'
		   || dest->wavWavCkId[1] != 'A'
		   || dest->wavWavCkId[2] != 'V'
		   || dest->wavWavCkId[3] != 'E') {
		printf("no wav id %c%c%c%c\n",
				dest->wavWavCkId[0],
				dest->wavWavCkId[1],
				dest->wavWavCkId[2],
				dest->wavWavCkId[3]);
		result++;
	}

	if (      dest->wavFmtCkId[0] != 'f'
		   || dest->wavFmtCkId[1] != 'm'
		   || dest->wavFmtCkId[2] != 't'
		   || dest->wavFmtCkId[3] != ' ') {
		printf("no fmt %c%c%c%c\n",
				dest->wavFmtCkId[0],
				dest->wavFmtCkId[1],
				dest->wavFmtCkId[2],
				dest->wavFmtCkId[3]);
		result++;
	}

	if (dest->wavFmtCkSize != 16) {
		printf("fmt size %d\n", dest->wavFmtCkSize);
		result++;
	}

	if (dest->wavFmtTag != 1) {
		printf("fmt tag %d\n", dest->wavFmtTag);
		result++;
	}
	
	if (dest->wavNumberChannels != 2) {
		printf("fmt channels %d\n", dest->wavNumberChannels);
		result++;
	}

	if (dest->wavSamplesPerSecond != 44100) {
		printf("fmt sps %d\n", dest->wavSamplesPerSecond);
		result++;
	}

	if (dest->wavBitsPerSample != 16) {
		printf("fmt bps %d\n", dest->wavBitsPerSample);
		result++;
	}

	if (       dest->wavDataCkId[0] != 'd'
			|| dest->wavDataCkId[1] != 'a'
			|| dest->wavDataCkId[2] != 't'
			|| dest->wavDataCkId[3] != 'a') {
		printf("wav data %c%c%c%c\n", 
				dest->wavDataCkId[0],
				dest->wavDataCkId[1],
				dest->wavDataCkId[2],
				dest->wavDataCkId[3]);
		result++;
	}
	return result;
}


void ednDrawEdnBmp(EdnPlatformState *ednPlatformState, f32 fMinX, f32 fMaxX, f32 fMinY, f32 fMaxY, EdnBmp bmp) {
	s32 minX = ednRoundFloatToUInt32(fMinX);
	if (minX < 0) minX = 0;

	s32 maxX = ednRoundFloatToUInt32(fMaxX);
	if (maxX > ednPlatformState->imageWidth) maxX = ednPlatformState->imageWidth;
	if (maxX > minX + bmp.bmpWidth) maxX = minX + bmp.bmpWidth;

	s32 minY = ednRoundFloatToUInt32(fMinY);
	if (minY < 0) minY = 0;

	s32 maxY = ednRoundFloatToUInt32(fMaxY);
	if (maxY > ednPlatformState->imageHeight) maxY = ednPlatformState->imageHeight;
	if (maxY > minY + bmp.bmpHeight) maxY = minY + bmp.bmpHeight;

	void *row = ednPlatformState->imageFrameData
					+ (minX * ednPlatformState->imageBytesPerPixel)
					+ (minY * ednPlatformState->imageFrameDataPitch);

	// the bmp data is (bottom left = 0), with (right = +x) and (up = +y)
	u32 *bmpRow = ((u32 *)bmp.bmpData) + ((bmp.bmpHeight - 1) * bmp.bmpWidth);
	EdnColor4f ednColor4f;
	for (s32 yIndex = minY; yIndex < maxY; yIndex++) {
		u32 *pixel = row;
		u32 *bmpPixel = bmpRow;
		for (s32 xIndex = minX; xIndex < maxX; xIndex++) {
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


s32 ednUpdateFrame(EdnPlatformState *ednPlatformState) {
	s32 result = 0;
	EdnGameState *ednGameState = (EdnGameState *)ednPlatformState->gamePermanentData;

	// clear to black
	{
		EdnColor4f clearColor;
		clearColor.a = 1.0f;
		clearColor.r = 1.0f;
		clearColor.g = 1.0f;
		clearColor.b = 0.0f;
		ednDrawRect(ednPlatformState, 0.0f, (f32)ednPlatformState->imageWidth, 0.0f, (f32)ednPlatformState->imageHeight, clearColor);

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

		u32 tileMap[9][16] = {
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

		for (s32 tileRow = 0; tileRow < 9; tileRow++) {
			for (s32 tileColumn = 0; tileColumn < 16; tileColumn++) {
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
				f32 tileWidth = ((f32) ednPlatformState->imageWidth) / 16.0f;
				f32 tileHeight = ((f32) ednPlatformState->imageHeight) / 9.0f;
				ednDrawRect(ednPlatformState,
						tileWidth * (f32)(tileColumn), tileWidth * (f32)(tileColumn + 1),
						tileHeight * (f32)(tileRow), tileHeight * (f32)(tileRow + 1),
						tileColor4f);
			}
		}

		// todo tks better assets/file io requests
		if (ednPlatformState->ednAssets[0].ednAssetStatus == LOADED) {
			EdnBmp bmp;
			parseEdnBmp(ednPlatformState->ednAssets[0].ednAssetData, &bmp);
			ednDrawEdnBmp(ednPlatformState, ednGameState->playerXPosition, ednGameState->playerXPosition + 50.0f,
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

		if (ednPlatformState->ednInput.isMPressed) {
			ednGameState->toneVolume = ednGameState->toneVolume - 10 < 0
				? 0 
				: ednGameState->toneVolume - 50;

		} else if (ednPlatformState->ednInput.isNPressed) {
			ednGameState->toneVolume = ednGameState->toneVolume + 10 > 32768
				? 32768
				: ednGameState->toneVolume + 50;
		}

#if 1
		for (s32 sampleIndex = 0; sampleIndex < ednPlatformState->audioFrameDataSize; sampleIndex++) {
			ednPlatformState->audioFrameData[sampleIndex] = 
				(s16)
				(ednGameState->toneVolume * sinf(2.0f * PI32 * (f32)ednGameState->runningSampleIndex / (f32)ednGameState->tonePeriod));
			ednGameState->runningSampleIndex += sampleIndex % 2;
		}
#else

		if (ednPlatformState->ednAssets[1].ednAssetStatus == LOADED) {
			EdnWav wav;
			s32 wavParseResult = parseEdnWav(ednPlatformState->ednAssets[1].ednAssetData, &wav);
			if (wavParseResult != 0) {
				result = wavParseResult;
			} else {

				for (s32 sampleIndex = 0; sampleIndex < ednPlatformState->audioFrameDataSize; sampleIndex++) {
					if (ednGameState->waveBytePosition > (wav.wavCkSize - 44 + (wav.wavCkSize % 2)))  {
						ednGameState->waveBytePosition = 0;
					}

					// todo multiply volume
					// ednPlatformState->audioFrameData[sampleIndex] = 
						// (s16) (ednGameState.toneVolume * (f32) (wavChunk.wavData + chunkCursor));
					ednPlatformState->audioFrameData[sampleIndex] = * (s16*)(wav.wavDataSamples + ednGameState->waveBytePosition++);
				}
			}



		} else {
			// todo tks better memory management too
			// todo tks maybe an asset find? or an asset reserve? an asset free? for these file ids
			ednPlatformState->ednAssets[1].ednAssetStatus = LOADING;
			ednStringCopy(ednPlatformState->ednAssets[1].fileName, "../assets/test.wav", EDN_ASSET_NAME_MAX);
			ednPlatformState->ednAssets[1].ednAssetData = ednPlatformState->gameTransientData + 50000;
			ednPlatformState->ednAssets[1].ednAssetDataByteCount = 800000;
			ednPlatformState->ednAssetCount = 2;
		}

#endif

	}

	return result;
}









