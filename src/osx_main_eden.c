#include "eden_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h> // file status
#include <sys/mman.h> // memory mapping

#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>
#include <OpenGL/GLU.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <time.h>

#include <x86intrin.h>
#include <dlfcn.h>

#define kilobytes(Value) ((Value)*1024LL)
#define megabytes(Value) (kilobytes(Value)*1024LL)
#define gigabytes(Value) (megabytes(Value)*1024LL)
#define terabytes(Value) (gigabytes(Value)*1024LL)

typedef struct {
    int size;
    int writeCursor;
    int playCursor;
    int16_t *data;
} AudioBuffer;

void audioCallback(void *userdata, uint8_t *stream, int length) {
	// cast the stream to the signed, 16 bit integers that we're expecting
	int16_t *dest = (int16_t *)stream;
	int destLength = length / 2;
	bool isCollision = false;

	AudioBuffer *audioBuffer = (AudioBuffer *)userdata;
	for (int index = 0; index < destLength; index++) {
		if (((audioBuffer->playCursor + 1) % audioBuffer->size) == audioBuffer->writeCursor) { 
			dest[index] = 0; 
			isCollision = true;
		} else {
			dest[index] = audioBuffer->data[audioBuffer->playCursor];
			audioBuffer->playCursor = (audioBuffer->playCursor + 1) % audioBuffer->size;
		}
	}
	// todo tks figure out why this collision is occuring here every second or two
	if (isCollision) { printf("Write and play audio head collided in callback\n"); }
}

int main(int argc, char** args) {
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) { fprintf(stderr, "SDL init. %s\n", SDL_GetError()); exit(1); }

	EdnPlatformState ednPlatformState;

	// preallocate everything according to the max possible size...
	{
		// enough memory to display two 4K displays with 4 bytes per pixel
		ednPlatformState.imageFrameDataByteCount = 4 * 4096 * 2160 * 2;
		// enough audio for 10 seconds of 48000 LR samples with ints
		ednPlatformState.audioFrameDataByteCount = sizeof(int16_t) * 2 * 4800 * 10;
		ednPlatformState.platformDataByteCount = ednPlatformState.audioFrameDataByteCount;
		ednPlatformState.gamePermanentDataByteCount = megabytes(1);
		ednPlatformState.gameTransientDataByteCount = gigabytes(2);
		ednPlatformState.baseDataByteCount = 
			ednPlatformState.imageFrameDataByteCount
			+ ednPlatformState.audioFrameDataByteCount
			+ ednPlatformState.platformDataByteCount
			+ ednPlatformState.gamePermanentDataByteCount
			+ ednPlatformState.gameTransientDataByteCount;

		// todo tks internal vs game build
		void *baseAddress = (void *)terabytes(2);

		// allocate one big memory block here
		ednPlatformState.baseData = mmap(baseAddress, ednPlatformState.baseDataByteCount,
										 PROT_READ | PROT_WRITE,
										 MAP_ANON | MAP_PRIVATE,
										 -1, 0);

		if (ednPlatformState.baseData == MAP_FAILED) {
			fprintf(stderr, "failed to allocate the big chunk of memory");
			exit(1);
		}

		// todo tks fix this
		void *address = ednPlatformState.baseData;

		ednPlatformState.platformData = address;
		address += ednPlatformState.platformDataByteCount;

		ednPlatformState.imageFrameData = address; 
		address += ednPlatformState.imageFrameDataByteCount;

		ednPlatformState.audioFrameData = address; 
		address += ednPlatformState.audioFrameDataByteCount;

		ednPlatformState.gamePermanentData = address; 
		address += ednPlatformState.gamePermanentDataByteCount;

		ednPlatformState.gameTransientData = address; 
		address += ednPlatformState.gameTransientDataByteCount;
	}

	ednPlatformState.imageWidth = 1280;
	ednPlatformState.imageHeight = 720;
	ednPlatformState.imageBytesPerPixel = 4;
	ednPlatformState.imageFrameDataSize = ednPlatformState.imageWidth * ednPlatformState.imageHeight * ednPlatformState.imageBytesPerPixel;
	ednPlatformState.imageFrameDataPitch = ednPlatformState.imageWidth * ednPlatformState.imageBytesPerPixel;

	// init window
	SDL_Renderer *renderer;
	SDL_Texture *texture;
	{
		SDL_Window *window = SDL_CreateWindow("Eden", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
				ednPlatformState.imageWidth, ednPlatformState.imageHeight, SDL_WINDOW_RESIZABLE);

		if (window == NULL) { fprintf(stderr, "SDL create window. %s\n", SDL_GetError()); exit(1); }

		renderer = SDL_CreateRenderer(window, -1, 0);

		if (renderer == NULL) { fprintf(stderr, "SDL create renderer. %s\n", SDL_GetError()); exit(1); }

		texture = SDL_CreateTexture(
				renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, ednPlatformState.imageWidth, ednPlatformState.imageHeight);

		if (texture == NULL) { fprintf(stderr, "SDL create texture. %s\n", SDL_GetError()); exit(1); }
	}

	AudioBuffer audioBuffer;
	// Open our audio device:
	{
		// todo tks make this a little different?
		ednPlatformState.frameDurationMs = 1000.0f / 30.0f;
		ednPlatformState.audioSamplesPerSecond = 48000;
		// this is audio samples per second divided by 30
		ednPlatformState.audioFrameDataSize = 3200;
		audioBuffer.size = ednPlatformState.audioFrameDataSize * 5;
		audioBuffer.data = ednPlatformState.platformData;
		audioBuffer.playCursor = 0;
		// set the write cursor a little ahead of the play cursor
		audioBuffer.writeCursor = ednPlatformState.audioFrameDataSize * 2;

		SDL_AudioSpec audioSettings = {0};
		audioSettings.freq = ednPlatformState.audioSamplesPerSecond;
		audioSettings.format = AUDIO_S16LSB;
		audioSettings.channels = 2;
		audioSettings.samples = ednPlatformState.audioFrameDataSize / 2;
		audioSettings.callback = &audioCallback;
		audioSettings.userdata = &audioBuffer;
		SDL_OpenAudio(&audioSettings, 0);

		if (audioSettings.format != AUDIO_S16LSB) {
			printf("Oops! We didn't get AUDIO_S16LSB as our sample format!\n");
			SDL_CloseAudio();
		} else {
			printf("Initialised an Audio device at frequency %d Hz, %d Channels, buffer size %d\n",
				   audioSettings.freq, audioSettings.channels, audioSettings.samples);
			SDL_PauseAudio(0);
		}
	}

	void *eden;
	int (*ednInit)(EdnPlatformState); 
	int (*ednUpdateFrame)(EdnPlatformState); 
	char* (*ednGetError)(EdnPlatformState); 
	struct stat edenFileStat;
	time_t edenModificationTime;
	// game init
	{
		eden = dlopen("../bin/eden.so", RTLD_LAZY|RTLD_LOCAL);

		if (!eden) { fprintf(stderr, "open eden game lib"); exit(1); }

		// load the functions for the judas shared lib
		ednInit = (int (*)(EdnPlatformState))dlsym(eden, "ednInit");
		ednUpdateFrame = (int (*)(EdnPlatformState))dlsym(eden, "ednUpdateFrame");
		ednGetError = (char* (*)(EdnPlatformState))dlsym(eden, "ednGetError");

		if (!ednUpdateFrame || !ednInit || !ednGetError) { fprintf(stderr, "load functions for edn game lib"); exit(1); }

		stat("../bin/eden.so", &edenFileStat);
		edenModificationTime = edenFileStat.st_mtime;

		// todo tks better spot for this init?
		if (ednInit(ednPlatformState) != 0) { fprintf(stderr, "init eden call. %s\n", ednGetError(ednPlatformState)); exit(1); }
	}

	// performance variables
	uint64_t performanceCountFrequency = SDL_GetPerformanceFrequency();
	uint64_t previousCounter = SDL_GetPerformanceCounter();
	double msPerFrame = 0;
	uint64_t previousCycle = _rdtsc();
	double mcpf = 0;

	// event variables
	SDL_Event event;
	const Uint8 *keyboardState;

	ednPlatformState.frameCount = 0;
	ednPlatformState.isRunning = true;

	while (ednPlatformState.isRunning) {
		ednPlatformState.frameCount++;

		// events and keyboard input
		{
			while (SDL_PollEvent(&event)) {
				if (event.type == SDL_QUIT) {
					ednPlatformState.isRunning = false;
				}

				if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
					// if (ednPlatformState.imageFrameData != NULL) {
						// todo tks something faster than malloc and free?
						// free(ednPlatformState.imageFrameData);
					// }

					if (texture != NULL) {
						SDL_DestroyTexture(texture);
					}

					ednPlatformState.imageWidth = event.window.data1;
					ednPlatformState.imageHeight = event.window.data2;
					ednPlatformState.imageFrameDataPitch = ednPlatformState.imageWidth * ednPlatformState.imageBytesPerPixel;
					ednPlatformState.imageFrameDataSize =
						ednPlatformState.imageWidth * ednPlatformState.imageHeight * ednPlatformState.imageBytesPerPixel;

					if (ednPlatformState.imageFrameDataSize > ednPlatformState.imageFrameDataByteCount) {
						fprintf(stderr, "display is too big\n");
						exit(1);
					}

					texture = SDL_CreateTexture(renderer,
							SDL_PIXELFORMAT_ARGB8888,
							SDL_TEXTUREACCESS_STREAMING,
							ednPlatformState.imageWidth, ednPlatformState.imageHeight);
					if (texture == NULL) {
						fprintf(stderr, "SDL create texture. %s\n", SDL_GetError());
						exit(1);
					}

					// todo tks something faster than malloc and free?
					// ednPlatformState.imageFrameData = malloc(ednPlatformState.imageFrameDataSize);
				}
			}

			keyboardState = SDL_GetKeyboardState(NULL); 
			ednPlatformState.isRunning = ednPlatformState.isRunning && keyboardState[SDL_SCANCODE_ESCAPE] != 1;
			ednPlatformState.isWPressed = keyboardState[SDL_SCANCODE_W] == 1; 
			ednPlatformState.isAPressed = keyboardState[SDL_SCANCODE_A] == 1; 
			ednPlatformState.isSPressed = keyboardState[SDL_SCANCODE_S] == 1; 
			ednPlatformState.isDPressed = keyboardState[SDL_SCANCODE_D] == 1; 
		} 

		// hot reload the eden library
		{
			if (ednPlatformState.frameCount % 30 == 0) { // only check every 30 frames
				stat("../bin/eden.so", &edenFileStat);
				if (edenModificationTime < edenFileStat.st_mtime) {
					edenModificationTime = edenFileStat.st_mtime;

					if (dlclose(eden) != 0) { fprintf(stderr, "close game lib. %s", dlerror()); exit(1); }

					eden = dlopen("../bin/eden.so", RTLD_LAZY|RTLD_LOCAL);

					if (!eden) { fprintf(stderr, "open eden game lib. %s", dlerror()); exit(1); }

					// load the functions for the judas shared lib
					ednUpdateFrame = (int (*)(EdnPlatformState))dlsym(eden, "ednUpdateFrame");
					ednGetError = (char* (*)(EdnPlatformState))dlsym(eden, "ednGetError");

					if (!ednUpdateFrame || !ednGetError) { fprintf(stderr, "load functions for edn game lib. %s\n", dlerror()); exit(1); }
				}
			}
		}

		// have eden update the frame
		{
			if (ednUpdateFrame(ednPlatformState) != 0) { fprintf(stderr, "error updating frame. %s\n", ednGetError(ednPlatformState)); exit(1); }
		}

		// update the sdl texture
		{
			SDL_UpdateTexture(texture, NULL, ednPlatformState.imageFrameData, ednPlatformState.imageFrameDataPitch);
			SDL_RenderCopy(renderer, texture, NULL, NULL);
			SDL_RenderPresent(renderer);
		}

		// update the audio
		for (int index = 0; index < ednPlatformState.audioFrameDataSize; index++) {
			if (((audioBuffer.writeCursor + 1) % audioBuffer.size) == audioBuffer.playCursor) {
				printf("Write and play audio head collided while copying\n");
				break;
			}
			audioBuffer.data[audioBuffer.writeCursor] = ednPlatformState.audioFrameData[index];
			audioBuffer.writeCursor = (audioBuffer.writeCursor + 1) % audioBuffer.size;
		}

		// lock in the framerate by waiting, and output the performance
		{
			msPerFrame = 1000.0f * (SDL_GetPerformanceCounter() - previousCounter) / (double)performanceCountFrequency;
			mcpf = (double)(_rdtsc() -  previousCycle) / (1000.0f * 1000.0f);

			if (msPerFrame < ednPlatformState.frameDurationMs) {
				SDL_Delay((uint32_t)(ednPlatformState.frameDurationMs - msPerFrame) - 2);
				do {
					msPerFrame = 1000.0f * (SDL_GetPerformanceCounter() - previousCounter) / (double)performanceCountFrequency;
					mcpf = (double)(_rdtsc() -  previousCycle) / (1000.0f * 1000.0f);
				} while (msPerFrame < ednPlatformState.frameDurationMs);

			} else if (ednPlatformState.frameCount > 1) {
				printf("\n\n\nThis is bad, missed a frame\n\n\n");
				msPerFrame = 1000.0f * (SDL_GetPerformanceCounter() - previousCounter) / (double)performanceCountFrequency;
				mcpf = (double)(_rdtsc() -  previousCycle) / (1000.0f * 1000.0f);
			}

			previousCounter = SDL_GetPerformanceCounter();
			previousCycle = _rdtsc();

			printf("%.02fmspf, %.02fmcpf\n", msPerFrame, mcpf);
		}
	}

	return 0;
}
