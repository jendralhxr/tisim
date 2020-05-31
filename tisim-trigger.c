#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#define PORT_TRIGGER 9999

// usage: ./tisim-trigger [addresses]

char command[256];
pid_t pid, pid_init, pid_current;

int main(int argc, char **argv){
	pid_init= getpid();
	printf("parent %d\n", pid);
	
	int i;
	for (i=1; i<argc; i++){
		pid= fork();
		//printf("pid%d %d\n", pid, i);
		if (pid==0){
			//printf("child%d pid%d %s\n", i, getpid(), argv[i]);
			pid_current= getppid();
			sprintf(command, "nc %s %d", argv[i], PORT_TRIGGER);
			if (pid_current==pid_init){
				printf("%s\n",command);
				system(command);
				}
			}
		}
	
	}
