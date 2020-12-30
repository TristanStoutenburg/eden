#include "eden_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h> // file status

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

typedef struct {
    int size;
    int writeCursor;
    int playCursor;
    int16_t *data;
} AudioBuffer;

typedef struct {
	uint64_t performanceCountFrequency;
	uint64_t previousCounter;
	double msPerFrame;
	uint64_t previousCycle;
	double mcpf;
} PerformanceCounter;

void audioCallback(void *userdata, uint8_t *stream, int length) {
	// cast the stream to the signed, 16 bit integers that we're expecting
	int16_t *dest = (int16_t *)stream;
	int destLength = length / 2;

	AudioBuffer *audioBuffer = (AudioBuffer *)userdata;
	for (int index = 0; index < destLength; index++) {
		if (((audioBuffer->playCursor + 1) % audioBuffer->size) == audioBuffer->writeCursor) { 
			dest[index] = 0; 
		} else {
			dest[index] = audioBuffer->data[audioBuffer->playCursor];
			audioBuffer->playCursor = (audioBuffer->playCursor + 1) % audioBuffer->size;
		}
	}
}

int main(int argc, char** args) {
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) { fprintf(stderr, "SDL init. %s\n", SDL_GetError()); exit(1); }

	EdnPlatformState ednPlatformState;
	ednPlatformState.imageWidth = 1280;
	ednPlatformState.imageHeight = 720;
	ednPlatformState.imageBytesPerPixel = 4;
	ednPlatformState.imageFrameDataSize = ednPlatformState.imageWidth * ednPlatformState.imageHeight * ednPlatformState.imageBytesPerPixel;
	ednPlatformState.imageFrameDataPitch = ednPlatformState.imageWidth * ednPlatformState.imageBytesPerPixel;
	ednPlatformState.imageFrameData = malloc(ednPlatformState.imageFrameDataSize);

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
		ednPlatformState.audioFrameDataSize = 3200; // need an int round to do this
			// (int) (ednPlatformState.audioSamplesPerSecond * 2 * ednPlatformState.frameDurationMs / 1000.f);
		ednPlatformState.audioFrameData = malloc(sizeof(int16_t) * ednPlatformState.audioFrameDataSize);
		audioBuffer.size = ednPlatformState.audioFrameDataSize * 5; // todo tks I think 5 frames is enough room
		audioBuffer.data = malloc(sizeof(int16_t) * audioBuffer.size);
		audioBuffer.playCursor = 0;
		audioBuffer.writeCursor = ednPlatformState.audioFrameDataSize * 2; // todo tks one frame ahead..

		SDL_AudioSpec audioSettings = {0};
		audioSettings.freq = ednPlatformState.audioSamplesPerSecond;
		audioSettings.format = AUDIO_S16LSB;
		audioSettings.channels = 2;
		audioSettings.samples = ednPlatformState.audioFrameDataSize / 2; // one frame size...
		// todo tks why 512? this means that the callback is called every 512 samples read
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

	// weird gradient variables
	ednPlatformState.gameDataSize = 1000000; // just do 1 MB for now, improve this later
	ednPlatformState.gameData = malloc(ednPlatformState.gameDataSize);

	void *eden;
	int (*ednInit)(EdnPlatformState); 
	int (*ednUpdateFrame)(EdnPlatformState); 
	struct stat edenFileStat;
	time_t edenModificationTime;
	// game init
	{
		eden = dlopen("../bin/eden.so", RTLD_LAZY|RTLD_LOCAL);

		if (!eden) { fprintf(stderr, "open eden game lib"); exit(1); }

		// load the functions for the judas shared lib
		ednInit = (int (*)(EdnPlatformState))dlsym(eden, "ednInit");
		ednUpdateFrame = (int (*)(EdnPlatformState))dlsym(eden, "ednUpdateFrame");

		if (!ednUpdateFrame || !ednInit) { fprintf(stderr, "load functions for edn game lib"); exit(1); }

		stat("../bin/eden.so", &edenFileStat);
		edenModificationTime = edenFileStat.st_mtime;

		// todo tks better spot for this init?
		if (ednInit(ednPlatformState) != 0) { fprintf(stderr, "init eden call\n"); exit(1); }
	}

	PerformanceCounter performance = {0};
	{
		performance.performanceCountFrequency = SDL_GetPerformanceFrequency();
		performance.previousCounter = SDL_GetPerformanceCounter();
		performance.previousCycle = _rdtsc();
	}

	ednPlatformState.isRunning = true;
	SDL_Event event;
	const Uint8 *keyboardState;
	ednPlatformState.frameCount = 0;

	while (ednPlatformState.isRunning) {
		ednPlatformState.frameCount++;

		// events and keyboard input
		{
			while (SDL_PollEvent(&event)) {
				if (event.type == SDL_QUIT) {
					ednPlatformState.isRunning = false;
				}

				if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
					if (ednPlatformState.imageFrameData != NULL) {
						// todo tks something faster than malloc and free?
						free(ednPlatformState.imageFrameData);
					}

					if (texture != NULL) {
						SDL_DestroyTexture(texture);
					}

					ednPlatformState.imageWidth = event.window.data1;
					ednPlatformState.imageHeight = event.window.data2;
					ednPlatformState.imageFrameDataPitch = ednPlatformState.imageWidth * ednPlatformState.imageBytesPerPixel;
					ednPlatformState.imageFrameDataSize =
						ednPlatformState.imageWidth * ednPlatformState.imageHeight * ednPlatformState.imageBytesPerPixel;

					texture = SDL_CreateTexture(renderer,
							SDL_PIXELFORMAT_ARGB8888,
							SDL_TEXTUREACCESS_STREAMING,
							ednPlatformState.imageWidth, ednPlatformState.imageHeight);
					if (texture == NULL) {
						fprintf(stderr, "SDL create texture. %s\n", SDL_GetError());
						exit(1);
					}

					// todo tks something faster than malloc and free?
					ednPlatformState.imageFrameData = malloc(ednPlatformState.imageFrameDataSize);
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

					if (!ednUpdateFrame) { fprintf(stderr, "load functions for edn game lib. %s", dlerror()); exit(1); }
				}
			}
		}

		{
			if (ednUpdateFrame(ednPlatformState) != 0) { fprintf(stderr, "error updating frame\n"); exit(1); }
		}

		// update the sdl texture
		{
			SDL_UpdateTexture(texture, NULL, ednPlatformState.imageFrameData, ednPlatformState.imageFrameDataPitch);
			SDL_RenderCopy(renderer, texture, NULL, NULL);
			SDL_RenderPresent(renderer);
		}

		// update the audio
		for (int index = 0; index < ednPlatformState.audioFrameDataSize; index++) {
			if (((audioBuffer.writeCursor + 1) % audioBuffer.size) == audioBuffer.playCursor) { break; }
			audioBuffer.data[audioBuffer.writeCursor] = ednPlatformState.audioFrameData[index];
			audioBuffer.writeCursor = (audioBuffer.writeCursor + 1) % audioBuffer.size;
		}

		// lock in the framerate by waiting
		{
			// todo tks this is doing a LOT more than this needs to, right? I feel like this could be cleaned up
			performance.msPerFrame = 1000.0f 
				* (SDL_GetPerformanceCounter() - performance.previousCounter) 
				/ (double)performance.performanceCountFrequency;
			performance.mcpf = (double)(_rdtsc() -  performance.previousCycle) / (1000.0f * 1000.0f);

			if (performance.msPerFrame < ednPlatformState.frameDurationMs) {
				SDL_Delay((uint32_t)(ednPlatformState.frameDurationMs - performance.msPerFrame) - 2);
				do {
					performance.msPerFrame = 1000.0f 
						* (SDL_GetPerformanceCounter() - performance.previousCounter) 
						/ (double)performance.performanceCountFrequency;
					performance.mcpf = (double)(_rdtsc() -  performance.previousCycle) / (1000.0f * 1000.0f);
				} while (performance.msPerFrame < ednPlatformState.frameDurationMs);

			} else if (ednPlatformState.frameCount > 1) {
				printf("\n\n\nThis is bad, missed a frame\n\n\n");
				performance.msPerFrame = 1000.0f 
					* (SDL_GetPerformanceCounter() - performance.previousCounter) 
					/ (double)performance.performanceCountFrequency;
				performance.mcpf = (double)(_rdtsc() -  performance.previousCycle) / (1000.0f * 1000.0f);

			}

			performance.previousCounter = SDL_GetPerformanceCounter();
			performance.previousCycle = _rdtsc();

			printf("%.02fmspf, %.02fmcpf\n", performance.msPerFrame, performance.mcpf);
		}
	}

	return 0;
}
