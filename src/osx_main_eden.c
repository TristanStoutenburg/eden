#include <errno.h>

#include <sys/stat.h> // file status
#include <fcntl.h> // file opening

#include <sys/mman.h> // memory mapping/allocating

#include <unistd.h> // file closing, reading, writing
#include <dlfcn.h> // shared library linking functions

#include <SDL2/SDL.h> // todo tks can I get off this? That would be nice... 

#include <x86intrin.h> // needed for for the counter srinsic, maybe move to an srinsics file

#include <stdarg.h> // varargs

#include "eden_platform.h"

#define kilobytes(Value) ((Value)*1024LL)
#define megabytes(Value) (kilobytes(Value)*1024LL)
#define gigabytes(Value) (megabytes(Value)*1024LL)
#define terabytes(Value) (gigabytes(Value)*1024LL)


// todo tks make the varagrs type safe? maybe these need to be thrown in a wrapper
// todo tks extract a strfmt(dest, max, message, ...) 
//	so this can be used to format the games error messages
//	also, maybe some sort of logger level printgs, that just print the message after each function call
#define PRINT_BUFFER_MAX 20000
void ednPrintf(const char *message, ...) {
	char printBuffer[PRINT_BUFFER_MAX];

	va_list args;
	s32 printBufferIndex = 0;
	s32 messageIndex = 0;

	va_start(args, message);
	while (message != NULL && message[messageIndex] != '\0') {
		if (message[messageIndex] == '%') {
			messageIndex++;
			if (message[messageIndex] == 'd') {
				messageIndex++;

				s32 s32Val = va_arg(args, s32);

				s32 cursor = printBufferIndex;
				if (s32Val < 0) {
					if (printBufferIndex + 1 > PRINT_BUFFER_MAX) break;
					printBuffer[printBufferIndex++] = '-';
				}
				s32 decimalPlace = 0;
				s32 remainder = s32Val;
				s32 divisor = 1000000000;
				while (divisor >= 1) {
					decimalPlace = remainder / divisor;
					remainder = remainder % divisor;
					if (decimalPlace != 0 && decimalPlace < 10) {
						if (printBufferIndex + 1 > PRINT_BUFFER_MAX) break;
						printBuffer[printBufferIndex++] = '0' + decimalPlace;
					}
					divisor /= 10;
				}

				if (cursor == printBufferIndex) {
					if (printBufferIndex + 1 > PRINT_BUFFER_MAX) break;
					printBuffer[printBufferIndex++] = '0';
				}


			} else if (message[messageIndex] == 's') {
				messageIndex++;

				char *subString = va_arg(args, char *);
				while (subString != NULL && *subString != '\0') {
					if (printBufferIndex + 1 > PRINT_BUFFER_MAX) break;
					printBuffer[printBufferIndex++] = *(subString++);
				}

			}
		} else {
			if (printBufferIndex + 1 > PRINT_BUFFER_MAX) break;
			printBuffer[printBufferIndex++] = message[messageIndex++];
		}
	}

	if (printBufferIndex < PRINT_BUFFER_MAX + 1) printBuffer[printBufferIndex++] = '\0';

	va_end(args);

	write(STDOUT_FILENO, printBuffer, printBufferIndex);
}

typedef struct {
    s32 size;
    s32 writeCursor;
    s32 playCursor;
    s16 *data;
} AudioBuffer;


void audioCallback(void *userdata, u8 *stream, s32 length) {
	// cast the stream to the signed, 16 bit segers that we're expecting
	s16 *dest = (s16 *)stream;
	s32 destLength = length / 2;
	bool isCollision = false;

	AudioBuffer *audioBuffer = (AudioBuffer *)userdata;
	for (s32 index = 0; index < destLength; index++) {
		if (((audioBuffer->playCursor + 1) % audioBuffer->size) == audioBuffer->writeCursor) { 
			dest[index] = 0; 
			isCollision = true;
		} else {
			dest[index] = audioBuffer->data[audioBuffer->playCursor];
			audioBuffer->playCursor = (audioBuffer->playCursor + 1) % audioBuffer->size;
		}
	}
	// todo tks figure out why this collision is occuring here every second or two
	// but only when the window is out of focus, crazy stuff is happening here
	// if (isCollision) { ednPrintf("Write and play audio head collided in callback\n"); }
}

s32 main(s32 argc, char** args) {
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) { 
		ednPrintf("SDL init. %s\n", SDL_GetError());
		exit(1);
	}

	EdnPlatformState ednPlatformState;

	AudioBuffer audioBuffer;

	// todo tks work in progress, maybe I should move all the memory pointers so a struct
	s64 platformAudioBufferByteCount;
	s64 platformAssetFileByteCount;
	void *platformAssetFileData;
	// s64 platformLoopFileByteCount;
	// void *platformLoopFileData;

	// preallocate everything with more than enough space
	{
		// enough memory to display two 4K displays with 4 bytes per pixel
		ednPlatformState.imageFrameDataByteCount = 4 * 4096 * 2160 * 2;
		// enough audio for 10 seconds of 48000 LR samples with ss
		ednPlatformState.audioFrameDataByteCount = sizeof(s16) * 2 * 4800 * 10;
		ednPlatformState.gamePermanentDataByteCount = megabytes(1);
		// ednPlatformState.gameTransientDataByteCount = gigabytes(2);
		ednPlatformState.gameTransientDataByteCount = gigabytes(1);
		ednPlatformState.gameDataByteCount = ednPlatformState.gamePermanentDataByteCount + ednPlatformState.gameTransientDataByteCount;

		// todo tks this should split so files and the stuff for the audio ring buffer
		platformAudioBufferByteCount = ednPlatformState.audioFrameDataByteCount;
		platformAssetFileByteCount = megabytes(100);
		// platformLoopFileByteCount = ednPlatformState.gamePermanentDataByteCount + ednPlatformState.gameTransientDataByteCount;

		ednPlatformState.platformDataByteCount = platformAudioBufferByteCount 
			+ platformAssetFileByteCount;
			// + platformLoopFileByteCount;

		ednPlatformState.baseDataByteCount = 
			ednPlatformState.imageFrameDataByteCount
			+ ednPlatformState.audioFrameDataByteCount
			+ ednPlatformState.platformDataByteCount
			+ ednPlatformState.gamePermanentDataByteCount
			+ ednPlatformState.gameTransientDataByteCount;

		// todo tks only do this for debug/devloper builds
		void *baseAddress = (void *)terabytes(2);

		// allocate one big memory block here
		ednPlatformState.baseData = mmap(baseAddress, ednPlatformState.baseDataByteCount,
										 PROT_READ | PROT_WRITE,
										 MAP_ANON | MAP_PRIVATE,
										 -1, 0);
		// todo consider calling mprotect

		if (ednPlatformState.baseData == MAP_FAILED) { ednPrintf("failed to allocate the big chunk of memory\n"); exit(1); }

		void *address = ednPlatformState.baseData;

		ednPlatformState.platformData = address;
		address += ednPlatformState.platformDataByteCount;

		{
			void *subAddress = ednPlatformState.platformData;

			audioBuffer.data = subAddress;
			subAddress += platformAudioBufferByteCount;

			platformAssetFileData = subAddress;
			subAddress += platformAssetFileByteCount;

			// platformLoopFileData = subAddress;
			// subAddress += platformLoopFileByteCount;

			if (subAddress != address) {
				ednPrintf("subdividing memory calculation failed\n");
				exit(1);
			}
		}

		ednPlatformState.imageFrameData = address; 
		address += ednPlatformState.imageFrameDataByteCount;

		ednPlatformState.audioFrameData = address; 
		address += ednPlatformState.audioFrameDataByteCount;

		ednPlatformState.gameData = address;
		address += ednPlatformState.gameDataByteCount;
		{
			void *subAddress = ednPlatformState.gameData;

			ednPlatformState.gamePermanentData = subAddress; 
			subAddress += ednPlatformState.gamePermanentDataByteCount;

			ednPlatformState.gameTransientData = subAddress; 
			subAddress += ednPlatformState.gameTransientDataByteCount;

			if (subAddress != address) {
				ednPrintf("subdividing game memeory calculation failed\n");
				exit(1);
			}
		}
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

		if (window == NULL) { 
			ednPrintf("SDL create window. %s\n", SDL_GetError());
			exit(1);
		}

		renderer = SDL_CreateRenderer(window, -1, 0);

		if (renderer == NULL) {
			ednPrintf("SDL create renderer. %s\n", SDL_GetError());
			exit(1);
		}

		texture = SDL_CreateTexture(
				renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, ednPlatformState.imageWidth, ednPlatformState.imageHeight);

		if (texture == NULL) {
			ednPrintf("SDL create texture. %s\n", SDL_GetError());
			exit(1);
		}
	}

	// Open our audio device:
	{
		ednPlatformState.frameDurationMs = 1000.0f / 30.0f;
		ednPlatformState.audioSamplesPerSecond = 48000;
		ednPlatformState.audioFrameDataSize = 3200; // this is audio samples per second divided by 30
		audioBuffer.size = ednPlatformState.audioFrameDataSize * 5;
		audioBuffer.playCursor = 0;
		audioBuffer.writeCursor = ednPlatformState.audioFrameDataSize * 4; // set the write cursor a little ahead of the play cursor

		SDL_AudioSpec audioSettings = {0};
		audioSettings.freq = ednPlatformState.audioSamplesPerSecond;
		audioSettings.format = AUDIO_S16LSB;
		audioSettings.channels = 2;
		audioSettings.samples = ednPlatformState.audioFrameDataSize / 2;
		audioSettings.callback = &audioCallback;
		audioSettings.userdata = &audioBuffer;
		SDL_OpenAudio(&audioSettings, 0);

		if (audioSettings.format != AUDIO_S16LSB) {
			ednPrintf("Oops! We didn't get AUDIO_S16LSB as our sample format!\n");
			SDL_CloseAudio();
		} else {
			// ednPrintf("Initialised an Audio device at frequency %d Hz, %d Channels, buffer size %d\n",
				   // audioSettings.freq, audioSettings.channels, audioSettings.samples);
			SDL_PauseAudio(0);
		}
	}

	void *eden;
	s32 (*ednInit)(EdnPlatformState*); 
	s32 (*ednUpdateFrame)(EdnPlatformState*); 
	char* (*ednGetError)(EdnPlatformState*); 
	struct stat edenFileStat;
	time_t edenModificationTime;
	// game init
	{
		eden = dlopen("../bin/eden.so", RTLD_LAZY|RTLD_LOCAL);

		if (!eden) { ednPrintf("open eden game lib\n"); exit(1); }

		// load the functions for the judas shared lib
		ednInit = (s32 (*)(EdnPlatformState*))dlsym(eden, "ednInit");
		ednUpdateFrame = (s32 (*)(EdnPlatformState*))dlsym(eden, "ednUpdateFrame");
		ednGetError = (char* (*)(EdnPlatformState*))dlsym(eden, "ednGetError");

		if (!ednUpdateFrame || !ednInit || !ednGetError) { ednPrintf("load functions for edn game lib\n"); exit(1); }

		stat("../bin/eden.so", &edenFileStat);
		edenModificationTime = edenFileStat.st_mtime;

		// todo tks better spot for this init?
		if (ednInit(&ednPlatformState) != 0) {
			ednPrintf("init eden call. %s\n", ednGetError(&ednPlatformState));
			exit(1);
		}
	}

	// todo tks debug code, this isn't used for anything right now
	struct stat loopFileStat;
	s32 loopFileHandle;
	char *loopFileName = "../bin/loop.edn";
	
	// performance variables
	u64 performanceCountFrequency = SDL_GetPerformanceFrequency();
	u64 previousCounter = SDL_GetPerformanceCounter();
	f32 msPerFrame = 0;
	u64 previousCycle = _rdtsc();
	f32 mcpf = 0;

	// event variables
	SDL_Event event;
	const Uint8 *keyboardState;

	ednPlatformState.frameCount = 0;
	ednPlatformState.isRunning = true;

	bool isLoopMode = false;
	bool isRecordMode = false;

#define INPUT_LOOP_MAX 15000
	s32 inputLoopIndex = 0;
	s32 inputLoopSize = 0;
	s32 isLPressed = 0;
	s32 isKPressed = 0;
	EdnInput inputLoop[INPUT_LOOP_MAX];

	while (ednPlatformState.isRunning) {
		ednPlatformState.frameCount++;

		// events and keyboard input
		{
			while (SDL_PollEvent(&event)) {
				if (event.type == SDL_QUIT) {
					ednPlatformState.isRunning = false;
				}

				if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
					// todo tks troubleshoot why resizing stalls, is the texture memory the problem?
					if (texture != NULL) {
						SDL_DestroyTexture(texture);
					}

					ednPlatformState.imageWidth = event.window.data1;
					ednPlatformState.imageHeight = event.window.data2;
					ednPlatformState.imageFrameDataPitch = ednPlatformState.imageWidth * ednPlatformState.imageBytesPerPixel;
					ednPlatformState.imageFrameDataSize =
						ednPlatformState.imageWidth * ednPlatformState.imageHeight * ednPlatformState.imageBytesPerPixel;

					if (ednPlatformState.imageFrameDataSize > ednPlatformState.imageFrameDataByteCount) {
						ednPrintf("display is too big\n"); exit(1);
					}

					texture = SDL_CreateTexture(renderer,
							SDL_PIXELFORMAT_ARGB8888,
							SDL_TEXTUREACCESS_STREAMING,
							ednPlatformState.imageWidth, ednPlatformState.imageHeight);
					if (texture == NULL) {
						ednPrintf("SDL create texture. %s\n", SDL_GetError());
						exit(1);
					}
				}
			}

			keyboardState = SDL_GetKeyboardState(NULL); 
			ednPlatformState.isRunning = ednPlatformState.isRunning && keyboardState[SDL_SCANCODE_ESCAPE] != 1;
			ednPlatformState.ednInput.isWPressed = keyboardState[SDL_SCANCODE_W] == 1 ? ednPlatformState.ednInput.isWPressed + 1 : 0; 
			ednPlatformState.ednInput.isAPressed = keyboardState[SDL_SCANCODE_A] == 1 ? ednPlatformState.ednInput.isAPressed + 1 : 0; 
			ednPlatformState.ednInput.isSPressed = keyboardState[SDL_SCANCODE_S] == 1 ? ednPlatformState.ednInput.isSPressed + 1 : 0; 
			ednPlatformState.ednInput.isDPressed = keyboardState[SDL_SCANCODE_D] == 1 ? ednPlatformState.ednInput.isDPressed + 1 : 0; 
			ednPlatformState.ednInput.isMPressed = keyboardState[SDL_SCANCODE_M] == 1 ? ednPlatformState.ednInput.isMPressed + 1 : 0; 
			ednPlatformState.ednInput.isNPressed = keyboardState[SDL_SCANCODE_N] == 1 ? ednPlatformState.ednInput.isNPressed + 1 : 0; 

			// used for loop recording, don't pass to the game
			isLPressed = keyboardState[SDL_SCANCODE_L] == 1;
			isKPressed = keyboardState[SDL_SCANCODE_K] == 1;
		} 

		// hot reload the eden library
		{
			if (ednPlatformState.frameCount % 30 == 0) { // only check every 30 frames
				stat("../bin/eden.so", &edenFileStat);
				if (edenModificationTime < edenFileStat.st_mtime) {
					edenModificationTime = edenFileStat.st_mtime;

					if (dlclose(eden) != 0) {
						ednPrintf("close game lib. %s\n", dlerror());
						exit(1);
					}

					eden = dlopen("../bin/eden.so", RTLD_LAZY|RTLD_LOCAL);

					if (!eden) {
						ednPrintf("open eden game lib. %s\n", dlerror());
						exit(1);
					}

					// load the functions for the judas shared lib
					ednUpdateFrame = (s32 (*)(EdnPlatformState*))dlsym(eden, "ednUpdateFrame");
					ednGetError = (char* (*)(EdnPlatformState*))dlsym(eden, "ednGetError");

					if (!ednUpdateFrame || !ednGetError) {
						ednPrintf("load functions for edn game lib. %s\n", dlerror());
						exit(1); }
				}
			}
		}

		// loop recording and playback
		{
			if (!isRecordMode && !isLoopMode && isLPressed) {
				isRecordMode = true;
				isLoopMode = false;
				inputLoopIndex = 0;
				inputLoopSize = 0;
				//save game data to the loop file

				loopFileHandle = open(loopFileName,
					O_WRONLY | O_CREAT | O_TRUNC,
					S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

				// loopFileHandle = open(loopFileName, O_WRONLY | O_CREAT | O_TRUNC);
				if (loopFileHandle < 0) {
					ednPrintf("loop file descriptor %s\n", strerror(errno));
					exit(1);
				}

				u32 bytesToWrite = ednPlatformState.gameDataByteCount;
				char *nextByteLocation = ednPlatformState.gameData;
				while (bytesToWrite > 0) {


					// todo tks this isn't working, I'm just too tired to understand why
					// maybe I'm doing something silly with the memory, like it maybe doesn't like
					// that the memory is unused or partially used or something like that
					ssize_t bytesWritten = write(loopFileHandle, nextByteLocation, bytesToWrite);
					if (bytesWritten == -1) {
						ednPrintf("write loop file %s\n", strerror(errno));
						exit(1);
					}
					bytesToWrite -= bytesWritten;
					nextByteLocation += bytesWritten;
				}

				if (close(loopFileHandle) != 0) {
					ednPrintf("close loop file %s\n", strerror(errno));
					exit(1);
				}

			} else if (isRecordMode && !isLPressed) {
				isLoopMode = false;
				if (inputLoopSize == INPUT_LOOP_MAX) {
					isRecordMode = false;
					isLoopMode = true;
					inputLoopIndex = 0;

				} else {
					inputLoop[inputLoopIndex].isWPressed = ednPlatformState.ednInput.isWPressed;
					inputLoop[inputLoopIndex].isAPressed = ednPlatformState.ednInput.isAPressed;
					inputLoop[inputLoopIndex].isSPressed = ednPlatformState.ednInput.isSPressed;
					inputLoop[inputLoopIndex].isDPressed = ednPlatformState.ednInput.isDPressed;

					inputLoopIndex++;
					inputLoopSize++;
				}

			} else if (isRecordMode && isLPressed) {
				isRecordMode = false;
				isLoopMode = true;
				inputLoopIndex = 0;

			} else if (isLoopMode && !isKPressed) {
				if (inputLoopIndex == 0) {
					// open the file, load game memory
					loopFileHandle = open(loopFileName, O_RDONLY);

					if (fstat(loopFileHandle, &loopFileStat) == -1) {
						ednPrintf("loop file stat\n");
						exit(1);
					}

					if (loopFileStat.st_size != ednPlatformState.gameDataByteCount) {
						ednPrintf("loop file expected %d but found %d\n",
								ednPlatformState.gameDataByteCount,
								loopFileStat.st_size);
						exit(1);
					}

					u32 bytesToRead = loopFileStat.st_size;
					void *nextByteLocation = ednPlatformState.gameData;
					while (bytesToRead > 0) {
						ssize_t bytesRead = read(loopFileHandle, nextByteLocation, bytesToRead);
						if (bytesRead == -1) {
							close(loopFileHandle);
							break;
						}
						bytesToRead -= bytesRead;
						nextByteLocation += bytesRead;
					}

					if (close(loopFileHandle) != 0) {
						ednPrintf("close loop file\n");
						exit(1);
					}
				}

				ednPlatformState.ednInput.isWPressed = inputLoop[inputLoopIndex].isWPressed;
				ednPlatformState.ednInput.isAPressed = inputLoop[inputLoopIndex].isAPressed;
				ednPlatformState.ednInput.isSPressed = inputLoop[inputLoopIndex].isSPressed;
				ednPlatformState.ednInput.isDPressed = inputLoop[inputLoopIndex].isDPressed;

				inputLoopIndex = (inputLoopIndex + 1) % inputLoopSize;

			} else if (isLoopMode && isKPressed) {
				isLoopMode = false;
				isRecordMode = false;
				inputLoopIndex = 0;
				inputLoopSize = 0;
				if (unlink(loopFileName) != 0) {
					ednPrintf("unlinking loop file %s\n", strerror(errno));
					exit(1);
				}
			}

			if (!ednPlatformState.isRunning && (isLoopMode || isRecordMode)) {
				if (unlink(loopFileName) != 0) {
					ednPrintf("unlinking loop file %s\n", strerror(errno));
					exit(1);
				}
			}
		}
		

		// have eden update the frame
		{
			if (ednUpdateFrame(&ednPlatformState) != 0) {
				ednPrintf("error updating frame. %s\n", ednGetError(&ednPlatformState));
				exit(1); }
		}


		// update the sdl texture
		{
			SDL_UpdateTexture(texture, NULL, ednPlatformState.imageFrameData, ednPlatformState.imageFrameDataPitch);
			SDL_RenderCopy(renderer, texture, NULL, NULL);
			SDL_RenderPresent(renderer);
		}

		// update the audio
		for (s32 index = 0; index < ednPlatformState.audioFrameDataSize; index++) {
			if (((audioBuffer.writeCursor + 1) % audioBuffer.size) == audioBuffer.playCursor) {
				// ednPrintf("Write and play audio head collided while copying\n");
				break;
			}
			audioBuffer.data[audioBuffer.writeCursor] = ednPlatformState.audioFrameData[index];
			audioBuffer.writeCursor = (audioBuffer.writeCursor + 1) % audioBuffer.size;
		}

		// load and close asset files
		{
			for (s32 assetIndex = 0; assetIndex < ednPlatformState.ednAssetCount; assetIndex++) {
				EdnAsset *ednAsset= &ednPlatformState.ednAssets[assetIndex];
				if (ednAsset->ednAssetStatus == LOADING) {
					ednAsset->fileHandle = open(ednAsset->fileName, O_RDONLY);

					struct stat assetFileStat;
					fstat(ednAsset->fileHandle, &assetFileStat); // Don't forget to check for an error return in f code
					ssize_t bytesRead = read(ednAsset->fileHandle, ednAsset->ednAssetData, assetFileStat.st_size);
					if (bytesRead == -1) {
						// todo do the looping thing to always get the full file
						close(ednAsset->fileHandle);
						exit(1);
					}
					ednAsset->ednAssetStatus = LOADED;

				} else if (ednAsset->ednAssetStatus == UNLOADING) {
					close(ednAsset->fileHandle);
					ednAsset->ednAssetStatus = FILE_CLOSED;
				}
			}

		}

		// lock in the framerate by waiting, and output the performance
		{
			msPerFrame = 1000.0f * (SDL_GetPerformanceCounter() - previousCounter) / (f32)performanceCountFrequency;
			mcpf = (f32)(_rdtsc() -  previousCycle) / (1000.0f * 1000.0f);

			if (msPerFrame < ednPlatformState.frameDurationMs) {
				SDL_Delay((u32)(ednPlatformState.frameDurationMs - msPerFrame) - 2);
				do {
					msPerFrame = 1000.0f * (SDL_GetPerformanceCounter() - previousCounter) / (f32)performanceCountFrequency;
					mcpf = (f32)(_rdtsc() -  previousCycle) / (1000.0f * 1000.0f);
				} while (msPerFrame < ednPlatformState.frameDurationMs);

			} else if (ednPlatformState.frameCount > 1) {
				// todo tks fix this bad boy right here
				// ednPrintf("\n\n\nThis is bad, missed a frame\n\n\n");
				msPerFrame = 1000.0f * (SDL_GetPerformanceCounter() - previousCounter) / (f32)performanceCountFrequency;
				mcpf = (f32)(_rdtsc() -  previousCycle) / (1000.0f * 1000.0f);
				// ednPrintf("%.02fmspf, %.02fmcpf\n", msPerFrame, mcpf);
			}

			previousCounter = SDL_GetPerformanceCounter();
			previousCycle = _rdtsc();

#if 0
			// todo print f32
			// ednPrintf("%.02fmspf, %.02fmcpf\n", msPerFrame, mcpf);
#endif
		}
	}

	return 0;
}
