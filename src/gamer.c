#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h> // file status
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

int main(int argc, char** args) {
	struct stat gameFileStat;
	time_t gameModificationTime;

	stat("eden.c", &gameFileStat);
	system("./tool build game");
	gameModificationTime = gameFileStat.st_mtime;
	while (true) {
		stat("eden.c", &gameFileStat);
		if (gameModificationTime < gameFileStat.st_mtime) {
			gameModificationTime = gameFileStat.st_mtime;
			system("./tool build game");
		}
		sleep(1);
	}
	return 0;
}
