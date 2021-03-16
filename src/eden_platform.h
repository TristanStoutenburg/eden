#ifndef EDEN_PLATFORM_H
#define EDEN_PLATFORM_H

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long  u64;

typedef          char   s8;
typedef          short  s16;
typedef          int    s32;
typedef          long   s64;

typedef          float  f32;
typedef          double f64;
typedef     long double f128;

#define short SHORT_USAGE_NOT_ALLOWED
#define int INT_USAGE_NOT_ALLOWED
#define long LONG_USAGE_NOT_ALLOWED
#define float FLOAT_USAGE_NOT_ALLOWED
#define double FLOAT_USAGE_NOT_ALLOWED
#define unsigned UNSIGNED_USAGE_NOT_ALLOWED
// #define printf PRINTF_USAGE_NOT_ALLOWED
#define fprintf FPRINTF_USAGE_NOT_ALLOWED
// todo figure out how to get redefine the macros without warnings?
// #define sprintf SPRINTF_USAGE_NOT_ALLOWED

// todo tks also disable int16_t 
#define int16_t INT_16_T_USAGE_NOT_ALLOWED


typedef struct {
	u32 isWPressed;
	u32 isAPressed;
	u32 isSPressed;
	u32 isDPressed;
	u32 isMPressed;
	u32 isNPressed;
} EdnInput;

typedef enum { EMPTY, LOADING, LOADED, UNLOADING, FILE_CLOSED } EdnAssetStatus;

#define EDN_ASSET_NAME_MAX 64
#define EDN_ASSET_MAX 200
typedef struct {
	char fileName[EDN_ASSET_NAME_MAX];
	u32 fileHandle;
	// the status and byte count is used for the game to ask the platform to load
	// the file so a chunk of memory, but the file read so the asset data pointer
	// while likely be smaller
	EdnAssetStatus ednAssetStatus;
	s64 ednAssetDataByteCount;
	// this points to somewhere inside of the memory that eden.c manages
	void* ednAssetData;
} EdnAsset;


typedef struct {

	void *baseData;
	s64 baseDataByteCount;

	void *platformData;
	s64 platformDataByteCount;

	u32 isRunning;

	u32 frameCount;
	f32 frameDurationMs;

	u32 imageWidth;
	u32 imageHeight;
    u32 imageBytesPerPixel;
	u32 imageFrameDataSize;
	u32 imageFrameDataPitch;
	void *imageFrameData;
	s64 imageFrameDataByteCount;

	u32 audioIsPlaying;
	u32 audioSamplesPerSecond;
	u32 audioBytesPerSample;
	u32 audioFrameDataSize;
	s16 *audioFrameData;
	s64 audioFrameDataByteCount;

	u32 ednAssetCount;
	EdnAsset ednAssets[EDN_ASSET_MAX];

	EdnInput ednInput;

	void *gameData;
	s64 gameDataByteCount;

	s64 gamePermanentDataByteCount;
	void *gamePermanentData;

	s64 gameTransientDataByteCount;
	void *gameTransientData;

	// todo tks better asset lib
	void *bmpData;

	// todo tks memory stuff??
	// todo tks file stuff? I'll probably want a request load asset or something like that, right?

} EdnPlatformState;

#endif
