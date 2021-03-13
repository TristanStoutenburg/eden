#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h> // file status
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
1
int main(int argc, char** args) {
	struct stat gameFileStat;
	time_t gameModificationTime;

	stat("eden.c", &gameFileStat);
	system("./tool build game");
	gameModificationTime = gameFileStat.st_mtime;
	while (1) {
		stat("eden.c", &gameFileStat);
		if (gameModificationTime < gameFileStat.st_mtime) {
			gameModificationTime = gameFileStat.st_mtime;
			system("./tool build game");
			printf("rebuilt eden %s", asctime(gmtime(&(gameFileStat.st_mtime))));
		}
		sleep(1);
	}
	return 0;
}
