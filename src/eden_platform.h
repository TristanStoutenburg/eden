#ifndef EDEN_PLATFORM_H
#define EDEN_PLATFORM_H

#include <stdbool.h>

typedef struct {

	bool isRunning;

	int frameCount;
	double frameDurationMs;

	int imageWidth;
	int imageHeight;
    int imageBytesPerPixel;
	int imageFrameDataSize;
	int imageFrameDataPitch;
	void *imageFrameData;

	bool audioIsPlaying;
	int audioSamplesPerSecond;
	int audioBytesPerSample;
	int audioFrameDataSize;
	void *audioFrameData;

	bool isWPressed;
	bool isAPressed;
	bool isSPressed;
	bool isDPressed;

	int gameDataSize;
	void *gameData;

	// todo tks memory stuff??
	// todo tks file stuff?

} EdnPlatformState;

#endif
