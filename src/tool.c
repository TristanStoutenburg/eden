// this c program can be used to build the source code
// to bootstrap this, run gcc tool.c -o tool
// todo tks we should use a #define on build to determine the platform?
// that way we know if we should point to eden_osx_main or eden_win_main, etc.
//
// then, use the help argument to see the menu

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** args) {
	// create an empty buffer which will store the command
	// just make one that's bigger than we'll really need
	char commandBuffer[256];

	if (argc == 2 && strcmp(args[1], "run") == 0) {
		// run the game
		strcpy(commandBuffer, "rm -f ../bin/loop.edn");
		strcat(commandBuffer, " && ./../bin/eden");

	} else if (argc == 3 && strcmp(args[1], "build") == 0 && strcmp(args[2], "game") == 0) {
		// recompile the eden shared library
		strcpy(commandBuffer, "gcc -g -c eden.c -o ../bin/eden.o");
		strcat(commandBuffer, " && gcc -g -shared ../bin/eden.o -o ../bin/eden.so");
		strcat(commandBuffer, " && rm ../bin/eden.o");

	} else if (argc == 3 && strcmp(args[1], "build") == 0 && strcmp(args[2], "platform") == 0) {
		strcpy(commandBuffer, "gcc -g osx_main_eden.c -o ../bin/eden ");
		strcat(commandBuffer, " -framework OpenGL -l glew -l sdl2 -l sdl2_image -l sdl2_ttf -l sdl2_mixer "); 
		strcat(commandBuffer, " && rm -f ../bin/loop.edn");

	} else if (argc == 3 && strcmp(args[1], "build") == 0 && strcmp(args[2], "tool") == 0) {
		strcpy(commandBuffer, "gcc -g tool.c -o tool");

	} else if (argc == 3 && strcmp(args[1], "build") == 0 && strcmp(args[2], "gamer") == 0) {
		strcpy(commandBuffer, "gcc -g gamer.c -o ./../bin/gamer");

	} else if (argc == 2 && strcmp(args[1], "gamer") == 0) {
		// run the gamer
		strcpy(commandBuffer, "./../bin/gamer");

	} else if (argc == 2 && strcmp(args[1], "debug") == 0) {
		// run the gamer
		strcpy(commandBuffer, "lldb ./../bin/eden");

	} else if (argc == 2 && strcmp(args[1], "help") == 0) {
		// show the help menu
		strcpy(commandBuffer, "echo 'tool options:\n");
		strcat(commandBuffer, "run - run the game\n");
		strcat(commandBuffer, "build game|platform|tool|gamer - recompile the game or the loop or the tool\n");
		strcat(commandBuffer, "gamer - run the game recompiler\n");
		strcat(commandBuffer, "debug - debug the game recompiler\n");
		strcat(commandBuffer, "help\n");
		strcat(commandBuffer, "'");


	} else {
		// display an invalid arguments prompt
		// note, the strcat may cause a buffer overflow
		// but let's just let the program crash
		// why churn for 20 minutes if someone accidentally pastes in something crazy
		strcpy(commandBuffer, "echo 'Invalid arguments. ");
		for (int i = 0; i < argc; i++) {
			strcat(commandBuffer, args[i]);
			strcat(commandBuffer, " ");
		}
		strcat(commandBuffer, "'");
	}
	
	// now that the command is in the buffer, execute the command
	system(commandBuffer);

	return 0;
}
