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

#include <math.h> // todo tks use built in function

#include <x86intrin.h>
#include <dlfcn.h>

#define PI32 3.14159265329
typedef struct {
    int size;
    int writeCursor;
    int playCursor;
    void *data;
} AudioRingBuffer;

void SDLAudioCallback(void *userData, Uint8 *audioData, int length) {
    AudioRingBuffer *ringBuffer = (AudioRingBuffer *)userData;
    int region1Size = length;
    int region2Size = 0;
    if (ringBuffer->playCursor + length > ringBuffer->size) {
        region1Size = ringBuffer->size - ringBuffer->playCursor;
        region2Size = length - region1Size;
    }
    memcpy(audioData, (uint8_t *)(ringBuffer->data) + ringBuffer->playCursor, region1Size);
    memcpy(&audioData[region1Size], ringBuffer->data, region2Size);
    ringBuffer->playCursor = (ringBuffer->playCursor + length) % ringBuffer->size;
    ringBuffer->writeCursor = (ringBuffer->playCursor + 2048) % ringBuffer->size;
}

int main(int argc, char** args) {
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
		fprintf(stderr, "SDL init. %s\n", SDL_GetError());
		exit(1);
	}

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
		SDL_Window *window = SDL_CreateWindow(
				"Eden", // window title
				SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, // position
				ednPlatformState.imageWidth, ednPlatformState.imageHeight,
				SDL_WINDOW_RESIZABLE // window flags, can be ored together
				);

		if (window == NULL) {
			fprintf(stderr, "SDL create window. %s\n", SDL_GetError());
			exit(1);
		}

		renderer = SDL_CreateRenderer(
				window,
				-1, // auto-detect the driver
				0 // no special flags
				);

		if (renderer == NULL) {
			fprintf(stderr, "SDL create renderer. %s\n", SDL_GetError());
			exit(1);
		}

		texture = SDL_CreateTexture(
				renderer,
				SDL_PIXELFORMAT_ARGB8888,
				SDL_TEXTUREACCESS_STREAMING,
				ednPlatformState.imageWidth, ednPlatformState.imageHeight
				);
		if (texture == NULL) {
			fprintf(stderr, "SDL create texture. %s\n", SDL_GetError());
			exit(1);
		}
	}

	// todo tks make this a little different?
	ednPlatformState.frameDurationMs = 33.33f;

	ednPlatformState.audioSamplesPerSecond = 48000;
	ednPlatformState.audioBytesPerSample = sizeof(int16_t) * 2;
	ednPlatformState.audioFrameDataSize =
		(int) (ednPlatformState.audioSamplesPerSecond * ednPlatformState.audioBytesPerSample * ednPlatformState.frameDurationMs / 1000.f);
	ednPlatformState.audioFrameData = malloc(ednPlatformState.audioFrameDataSize);

	AudioRingBuffer audioRingBuffer;
	// NOTE: Sound test
	int runningSampleIndex = 0;
	int toneVolume = 3000;
	int tonePeriod = ednPlatformState.audioSamplesPerSecond / 256;

	int secondaryBufferSize = ednPlatformState.audioSamplesPerSecond * ednPlatformState.audioBytesPerSample;
	// Open our audio device:
	{
		SDL_AudioSpec audioSettings = {0};

		audioSettings.freq = ednPlatformState.audioSamplesPerSecond;
		audioSettings.format = AUDIO_S16LSB;
		audioSettings.channels = 2;
		audioSettings.samples = 1024;
		audioSettings.callback = &SDLAudioCallback;
		audioSettings.userdata = &audioRingBuffer;

		audioRingBuffer.size = secondaryBufferSize;
		audioRingBuffer.data = malloc(secondaryBufferSize);
		audioRingBuffer.playCursor = audioRingBuffer.writeCursor = 0;

		SDL_OpenAudio(&audioSettings, 0);

		printf("Initialised an Audio device at frequency %d Hz, %d Channels, buffer size %d\n",
			   audioSettings.freq, audioSettings.channels, audioSettings.samples);

		if (audioSettings.format != AUDIO_S16LSB) {
			printf("Oops! We didn't get AUDIO_S16LSB as our sample format!\n");
			SDL_CloseAudio();
		}

	}

	// weird gradient variables
	ednPlatformState.gameDataSize = 1000000; // just do 1 MB for now, improve this later
	ednPlatformState.gameData = malloc(ednPlatformState.gameDataSize);

	void *eden;
	void (*ednUpdateFrame)(EdnPlatformState); 
	struct stat edenFileStat;
	time_t edenModificationTime;
	// game init
	{
		eden = dlopen("../bin/eden.so", RTLD_LAZY|RTLD_LOCAL);
		if (!eden) {
			fprintf(stderr, "open eden game lib");
			exit(1);
		}

		// load the functions for the judas shared lib
		ednUpdateFrame = (void (*)(EdnPlatformState))dlsym(eden, "ednUpdateFrame");
		if (!ednUpdateFrame) {
			fprintf(stderr, "load functions for edn game lib");
			exit(1);
		}

		stat("../bin/eden.so", &edenFileStat);
		edenModificationTime = edenFileStat.st_mtime;
	}

	uint64_t performanceCountFrequency = SDL_GetPerformanceFrequency();
	uint64_t previousCounter = SDL_GetPerformanceCounter();
	uint64_t currentCounter;
	uint64_t elapsedCounter;
	double msPerFrame;
	double fps;

	uint32_t audioWriteHead = 0;
	uint32_t audioPlayHead = 0;

	uint64_t previousCycle = _rdtsc();
	uint64_t currentCycle;
	uint64_t elapsedCycles;
	double mcpf;

	clock_t startClock, endClock;
	double elapsedClockTime;
	startClock = clock();
	endClock = clock();
	elapsedClockTime = ((double)(endClock - startClock)) / CLOCKS_PER_SEC;

	bool isRunning = true;
	SDL_Event event;
	const Uint8 *keyboardState;
	ednPlatformState.frameCount = 0;
	while (isRunning) {
		ednPlatformState.frameCount++;

		while (SDL_PollEvent(&event)) {

			if (event.type == SDL_QUIT) {
				isRunning &= false;

			} else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
				if (ednPlatformState.imageFrameData != NULL) {
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

				texture = SDL_CreateTexture(
						renderer,
						SDL_PIXELFORMAT_ARGB8888,
						SDL_TEXTUREACCESS_STREAMING,
						ednPlatformState.imageWidth, ednPlatformState.imageHeight
						);
				if (texture == NULL) {
					fprintf(stderr, "SDL create texture. %s\n", SDL_GetError());
					exit(1);
				}

				// todo tks something faster than malloc and free?
				ednPlatformState.imageFrameData = malloc(ednPlatformState.imageFrameDataSize);
			}

		}

		// get the game input
		keyboardState = SDL_GetKeyboardState(NULL); 
		isRunning &= keyboardState[SDL_SCANCODE_ESCAPE] != 1;
		ednPlatformState.isWPressed = keyboardState[SDL_SCANCODE_W] == 1; 
		ednPlatformState.isAPressed = keyboardState[SDL_SCANCODE_A] == 1; 
		ednPlatformState.isSPressed = keyboardState[SDL_SCANCODE_S] == 1; 
		ednPlatformState.isDPressed = keyboardState[SDL_SCANCODE_D] == 1; 

		// hot reload the eden library
		{
			if (ednPlatformState.frameCount % 30 == 0) { // only check once a second
				stat("../bin/eden.so", &edenFileStat);
				if (edenModificationTime < edenFileStat.st_mtime) {
					edenModificationTime = edenFileStat.st_mtime;

					if (dlclose(eden) != 0) {
						fprintf(stderr, "close game lib. %s", dlerror());
					}

					eden = dlopen("../bin/eden.so", RTLD_LAZY|RTLD_LOCAL);
					if (!eden) {
						fprintf(stderr, "open eden game lib. %s", dlerror());
						exit(1);
					}

					// load the functions for the judas shared lib
					ednUpdateFrame = (void (*)(EdnPlatformState))dlsym(eden, "ednUpdateFrame");
					if (!ednUpdateFrame) {
						fprintf(stderr, "load functions for edn game lib. %s", dlerror());
						exit(1);
					}
				}
			}
		}

		ednUpdateFrame(ednPlatformState);

		// update the sdl texture
		{
			SDL_UpdateTexture(
					texture,
					NULL, // do the whole texture
					ednPlatformState.imageFrameData,
					ednPlatformState.imageFrameDataPitch);

			SDL_RenderCopy(
					renderer, texture,
					NULL, NULL); // source and destintaion rectangles

			SDL_RenderPresent(renderer);

		}

		// update the audio
		{
			SDL_LockAudio();
				int byteToLock = runningSampleIndex * ednPlatformState.audioBytesPerSample % secondaryBufferSize;
				int bytesToWrite = 0;
				// todo tks we could check to make sure that the play cursor is different
				if (byteToLock > audioRingBuffer.playCursor) {
					bytesToWrite = secondaryBufferSize - byteToLock;
					bytesToWrite += audioRingBuffer.playCursor;
				} else {
					bytesToWrite = audioRingBuffer.playCursor - byteToLock;
				}
				// TODO(casey): More strenuous test!
				// TODO(casey): Switch to a sine wave
				void *region1 = (uint8_t*)audioRingBuffer.data + byteToLock;
				int region1Size = bytesToWrite;
				if (region1Size + byteToLock > secondaryBufferSize) {
					region1Size = secondaryBufferSize - byteToLock;
				}
				void *region2 = audioRingBuffer.data;
				int region2Size = bytesToWrite - region1Size;
			SDL_UnlockAudio();

			int region1SampleCount = region1Size/ednPlatformState.audioBytesPerSample;
			int16_t *sampleOut = (int16_t *)region1;
			for (int sampleIndex = 0; sampleIndex < region1SampleCount; ++sampleIndex) {
				float t = 2.0f * PI32 * (float)runningSampleIndex / (float)tonePeriod;
				float sinValue = sinf(t);
				runningSampleIndex++;
				int16_t sampleValue = (int16_t) (sinValue * toneVolume);
				*sampleOut++ = sampleValue;
				*sampleOut++ = sampleValue;
			}

			int region2SampleCount = region2Size/ednPlatformState.audioBytesPerSample;
			sampleOut = (int16_t *)region2;
			for (int sampleIndex = 0; sampleIndex < region2SampleCount; ++sampleIndex) {
				float t = 2.0f * PI32 * (float)runningSampleIndex / (float)tonePeriod;
				float sinValue = sinf(t);
				runningSampleIndex++;
				int16_t sampleValue = (int16_t) (sinValue * toneVolume);
				*sampleOut++ = sampleValue;
				*sampleOut++ = sampleValue;
			}

			if (!ednPlatformState.audioIsPlaying) {
				SDL_PauseAudio(0);
				ednPlatformState.audioIsPlaying = true;
			}
		}

		// performance counter work...
		{
			currentCounter = SDL_GetPerformanceCounter();
			elapsedCounter = currentCounter - previousCounter;
			msPerFrame = (1000.0f * (double)elapsedCounter) / (double)performanceCountFrequency;

			if (msPerFrame < ednPlatformState.frameDurationMs) {
				SDL_Delay((uint32_t)(ednPlatformState.frameDurationMs - msPerFrame) - 2);
				while (msPerFrame < ednPlatformState.frameDurationMs) {
					currentCounter = SDL_GetPerformanceCounter();
					elapsedCounter = currentCounter - previousCounter;
					msPerFrame = (1000.0f * (double)elapsedCounter) / (double)performanceCountFrequency;
					// todo tks for some reason this isn't perfect.. fix this later, ok?
				}

			} else {
				printf("\n\n\nThis is bad, missed a frame\n\n\n");
			}

			startClock = endClock;

			fps = (double)performanceCountFrequency / (double)elapsedCounter;
			currentCycle = _rdtsc();
			elapsedCycles = currentCycle - previousCycle;
			mcpf = (double)elapsedCycles / (1000.0f * 1000.0f);

			printf("%.02fmspf, %.02fmcpf, %.02ffps\n", msPerFrame, mcpf, fps);
			
			previousCounter = currentCounter;
			previousCycle = currentCycle;
		}
	}

	SDL_Quit();

	return 0;
}
