// filename eden.c

#ifndef EDEN_PLATFORM_H
#define EDEN_PLATFORM_H

#include <stdbool.h>
#include <stdlib.h>

typedef struct {
	int isWPressed;
	int isAPressed;
	int isSPressed;
	int isDPressed;
	int isMPressed;
} EdnInput;

typedef enum { BMP } EdnAssetType;

typedef struct {
	EdnAssetType ednAssetType;
	long ednAssetDataByteCount;
	char ednAssetName[64];
	void* ednAssetData;
} EdnAsset;

typedef struct {

	void *baseData;
	long baseDataByteCount;

	void *platformData;
	long platformDataByteCount;

	bool isRunning;

	int frameCount;
	double frameDurationMs;

	int imageWidth;
	int imageHeight;
    int imageBytesPerPixel;
	int imageFrameDataSize;
	int imageFrameDataPitch;
	void *imageFrameData;
	long imageFrameDataByteCount;

	bool audioIsPlaying;
	int audioSamplesPerSecond;
	int audioBytesPerSample;
	int audioFrameDataSize;
	int16_t *audioFrameData;
	long audioFrameDataByteCount;

	int ednAssetCount;
	EdnAsset* ednAssets;

	EdnInput ednInput;

	void *gameData;
	long gameDataByteCount;

	long gamePermanentDataByteCount;
	void *gamePermanentData;

	long gameTransientDataByteCount;
	void *gameTransientData;

	// todo tks memory stuff??
	// todo tks file stuff? I'll probably want a request load asset or something like that, right?jkjkjkj

} EdnPlatformState;

#endif
