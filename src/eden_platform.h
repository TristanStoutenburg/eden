#ifndef EDEN_PLATFORM_H
#define EDEN_PLATFORM_H

#include <stdbool.h>
#include <stdlib.h>

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

	bool isWPressed;
	bool isAPressed;
	bool isSPressed;
	bool isDPressed;

	long gamePermanentDataByteCount;
	void *gamePermanentData;

	long gameTransientDataByteCount;
	void *gameTransientData;

	// todo tks memory stuff??
	// todo tks file stuff?

} EdnPlatformState;

#endif
