#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <SDL2/SDL_opengl.h>
#include <OpenGL/GLU.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>

typedef struct
{
    int size;
    int writeCursor;
    int playCursor;
    void *data;
} AudioRingBuffer;

AudioRingBuffer audioRingBuffer;

void
SDLAudioCallback(void *UserData, Uint8 *AudioData, int Length)
{
    AudioRingBuffer *ringBuffer = (AudioRingBuffer *)UserData;

    int region1Size = Length;
    int region2Size = 0;
    if (ringBuffer->playCursor + Length > ringBuffer->size)
    {
        region1Size = ringBuffer->size - ringBuffer->playCursor;
        region2Size = Length - region1Size;
    }
    memcpy(AudioData, (uint8_t *)(ringBuffer->data) + ringBuffer->playCursor, region1Size);
    memcpy(&AudioData[region1Size], ringBuffer->data, region2Size);
    ringBuffer->playCursor = (ringBuffer->playCursor + Length) % ringBuffer->size;
    ringBuffer->writeCursor = (ringBuffer->playCursor + 2048) % ringBuffer->size;
}

void ednAudioCallback(void *ednData, Uint8 *sdlData, int length) {
	memset(sdlData, 0, length); 
}

int main(int argc, char** args) {
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
		fprintf(stderr, "SDL init. %s\n", SDL_GetError());
		exit(1);
	}

	// todo this could be moved into it's own struct and helper method
	// along with the texture, I don't know if this is a great idea
	int screenWidth = 1280;
	int screenHeight = 720;
    #define BYTES_PER_PIXEL 4
	void *bitmap = malloc(screenWidth * screenHeight * BYTES_PER_PIXEL);
	int bitmapPitch = screenWidth * BYTES_PER_PIXEL;

	// init window
	SDL_Renderer *renderer;
	SDL_Texture *texture;
	{
		SDL_Window *window = SDL_CreateWindow(
				"Eden", // window title
				SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, // position
				screenWidth, screenHeight,
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
				screenWidth, screenHeight
				);
		if (texture == NULL) {
			fprintf(stderr, "SDL create texture. %s\n", SDL_GetError());
			exit(1);
		}
	}

	// NOTE: Sound test
	int samplesPerSecond = 48000;
	int toneHz = 256;
	int16_t toneVolume = 3000;
	uint32_t runningSampleIndex = 0;
	int squareWavePeriod = samplesPerSecond / toneHz;
	int halfSquareWavePeriod = squareWavePeriod / 2;
	int bytesPerSample = sizeof(int16_t) * 2;
	int secondaryBufferSize = samplesPerSecond * bytesPerSample;
	// Open our audio device:
	{
		SDL_AudioSpec audioSettings = {0};

		audioSettings.freq = samplesPerSecond;
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

		if (audioSettings.format != AUDIO_S16LSB)
		{
			printf("Oops! We didn't get AUDIO_S16LSB as our sample format!\n");
			SDL_CloseAudio();
		}

	}
	bool soundIsPlaying = false;

	// weird gradient variables
	int blueOffset = 0;
	int greenOffset = 0;

	bool isRunning = true;

	SDL_Event event;
	const Uint8 *keyboardState;
	while (isRunning) {

		while (SDL_PollEvent(&event)) {

			if (event.type == SDL_QUIT) {
				isRunning = false;

			} else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
				if (bitmap != NULL) {
					free(bitmap);
				}
				if (texture != NULL) {
					SDL_DestroyTexture(texture);
				}

				screenWidth = event.window.data1;
				screenHeight = event.window.data2;
				bitmapPitch = screenWidth * BYTES_PER_PIXEL;

				texture = SDL_CreateTexture(
						renderer,
						SDL_PIXELFORMAT_ARGB8888,
						SDL_TEXTUREACCESS_STREAMING,
						screenWidth, screenHeight
						);
				if (texture == NULL) {
					fprintf(stderr, "SDL create texture. %s\n", SDL_GetError());
					exit(1);
				}

				// todo tks something faster than malloc and free?
				bitmap = malloc(screenHeight * screenWidth * BYTES_PER_PIXEL);
			}

		}

		keyboardState = SDL_GetKeyboardState(NULL); 
		if (keyboardState[SDL_SCANCODE_ESCAPE] == 1) {
			isRunning = false; 
		} 
		if (keyboardState[SDL_SCANCODE_W] == 1) {
			greenOffset += 1;
		} 
		if (keyboardState[SDL_SCANCODE_A] == 1) {
			blueOffset += 1;
		} 
		if (keyboardState[SDL_SCANCODE_S] == 1) {
			greenOffset -= 1;
		} 
		if (keyboardState[SDL_SCANCODE_D] == 1) {
			blueOffset -= 1;
		}

		// render weird grandient
		{
			uint8_t *row = (uint8_t *)bitmap;

			for (int y = 0; y < screenHeight; y++) {
				uint32_t *pixel = (uint32_t *)row;
				for (int x = 0; x < screenWidth; x++) {
					uint8_t blue = x + blueOffset;
					uint8_t green = y + greenOffset;
					*pixel++ = ((green << 8) | (blue));
				}
				row += bitmapPitch;
			}
		}

		// update the sdl texture
		{
			SDL_UpdateTexture(
					texture,
					NULL, // do the whole texture
					bitmap,
					bitmapPitch
					);

			SDL_RenderCopy(
					renderer, texture,
					NULL, NULL // source and destintaion rectangles
					);

			SDL_RenderPresent(renderer);
		}

		// update the audio
		{
			SDL_LockAudio();
				int byteToLock = runningSampleIndex*bytesPerSample % secondaryBufferSize;
				int bytesToWrite;
				if(byteToLock == audioRingBuffer.playCursor) {
					bytesToWrite = secondaryBufferSize;
				} else if(byteToLock > audioRingBuffer.playCursor) {
					bytesToWrite = (secondaryBufferSize - byteToLock);
					bytesToWrite += audioRingBuffer.playCursor;
				} else {
					bytesToWrite = audioRingBuffer.playCursor - byteToLock;
				}
				// TODO(casey): More strenuous test!
				// TODO(casey): Switch to a sine wave
				void *region1 = (uint8_t*)audioRingBuffer.data + byteToLock;
				int region1Size = bytesToWrite;
				if (region1Size + byteToLock > secondaryBufferSize) region1Size = secondaryBufferSize - byteToLock;
				void *region2 = audioRingBuffer.data;
				int region2Size = bytesToWrite - region1Size;
			SDL_UnlockAudio();

			int region1SampleCount = region1Size/bytesPerSample;
			int16_t *sampleOut = (int16_t *)region1;
			for(int sampleIndex = 0; sampleIndex < region1SampleCount; ++sampleIndex) {
				int16_t sampleValue = ((runningSampleIndex++ / halfSquareWavePeriod) % 2) ? toneVolume : -toneVolume;
				*sampleOut++ = sampleValue;
				*sampleOut++ = sampleValue;
			}

			int region2SampleCount = region2Size/bytesPerSample;
			sampleOut = (int16_t *)region2;
			for (int sampleIndex = 0; sampleIndex < region2SampleCount; ++sampleIndex) {
				int16_t sampleValue = ((runningSampleIndex++ / halfSquareWavePeriod) % 2) ? toneVolume : -toneVolume;
				*sampleOut++ = sampleValue;
				*sampleOut++ = sampleValue;
			}

			if (!soundIsPlaying) {
				SDL_PauseAudio(0);
				soundIsPlaying = true;
			}
		}
	}

	SDL_Quit();

	return 0;
}
