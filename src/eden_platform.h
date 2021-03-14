#ifndef EDEN_PLATFORM_H
#define EDEN_PLATFORM_H

typedef unsigned short uint16;
typedef unsigned int   uint32;
typedef unsigned long  uint64;

typedef          short  int16;
typedef          int    int32;
typedef          long   int64;

typedef          float  real32;
typedef          double real64;

#define short SHORT_USAGE_NOT_ALLOWED
#define int INT_USAGE_NOT_ALLOWED
#define long LONG_USAGE_NOT_ALLOWED
#define float FLOAT_USAGE_NOT_ALLOWED
#define double FLOAT_USAGE_NOT_ALLOWED
#define unsigned UNSIGNED_USAGE_NOT_ALLOWED
#define printf PRINTF_USAGE_NOT_ALLOWED
#define fprintf FPRINTF_USAGE_NOT_ALLOWED
// todo figure out how to get redefine the macros without warnings?
// #define sprintf SPRINTF_USAGE_NOT_ALLOWED

typedef struct {
	uint32 isWPressed;
	uint32 isAPressed;
	uint32 isSPressed;
	uint32 isDPressed;
	uint32 isMPressed;
} EdnInput;

typedef enum { EMPTY, LOADING, LOADED, UNLOADING, FILE_CLOSED } EdnAssetStatus;

#define EDN_ASSET_NAME_MAX 64
#define EDN_ASSET_MAX 200
typedef struct {
	char fileName[EDN_ASSET_NAME_MAX];
	uint32 fileHandle;
	// the status and byte count is used for the game to ask the platform to load
	// the file into a chunk of memory, but the file read into the asset data pointer
	// while likely be smaller
	EdnAssetStatus ednAssetStatus;
	int64 ednAssetDataByteCount;
	// this points to somewhere inside of the memory that eden.c manages
	void* ednAssetData;
} EdnAsset;


typedef struct {

	void *baseData;
	int64 baseDataByteCount;

	void *platformData;
	int64 platformDataByteCount;

	uint32 isRunning;

	uint32 frameCount;
	real32 frameDurationMs;

	uint32 imageWidth;
	uint32 imageHeight;
    uint32 imageBytesPerPixel;
	uint32 imageFrameDataSize;
	uint32 imageFrameDataPitch;
	void *imageFrameData;
	int64 imageFrameDataByteCount;

	uint32 audioIsPlaying;
	uint32 audioSamplesPerSecond;
	uint32 audioBytesPerSample;
	uint32 audioFrameDataSize;
	int16 *audioFrameData;
	int64 audioFrameDataByteCount;

	uint32 ednAssetCount;
	EdnAsset ednAssets[EDN_ASSET_MAX];

	EdnInput ednInput;

	void *gameData;
	int64 gameDataByteCount;

	int64 gamePermanentDataByteCount;
	void *gamePermanentData;

	int64 gameTransientDataByteCount;
	void *gameTransientData;

	// todo tks better asset lib
	void *bmpData;

	// todo tks memory stuff??
	// todo tks file stuff? I'll probably want a request load asset or something like that, right?

} EdnPlatformState;

#endif
