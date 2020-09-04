/*
 $  g++ test.cpp `pkg-config opencv --libs` -lpthread
 $ ./test /dev/video1 FRAMERATE EXPOSURE(us)
 */

#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include "v4ldevice.cpp"
#include "config.h"

using namespace cv;
using namespace std;

pthread_t image_show;

char command[256]; // for parameter setting from shell
unsigned char* ImageBuffer = NULL;
	
int wKey = -1;

Mat raw_frame(FRAME_HEIGHT, FRAME_WIDTH, CV_8UC1,Scalar(0));// Grayscale
Mat gray_frame(FRAME_HEIGHT, FRAME_WIDTH, CV_8UC1,Scalar(0));// Grayscale

struct timespec start, stop, timestamp;

void* processDisplay(void *arg){
	printf("display enter\n");
	
	wKey=-1;
	while (wKey==-1){
		if (waitKey(1)==27) break;
		
		}
	wKey = 1;
	pthread_exit(NULL);
	printf("display exit\n");
}

int set_props(char *device, int exposure){
	sprintf(command, "v4l2-ctl -d %s -c exposure_auto=1", device);
	system(command);
	sprintf(command, "v4l2-ctl -d %s -c exposure_time_us=%d", device, exposure);
	system(command);
	//sprintf(command, "v4l2-ctl -d %s -c exposure_absolute=%d", device, EXPOSURE);
	//system(command);
	sprintf(command, "v4l2-ctl -d %s -c gain_auto=0", device);
	system(command);
	sprintf(command, "v4l2-ctl -d %s -c gain=%d", device, GAIN);
	system(command);
	sprintf(command, "v4l2-ctl -d %s -c brightness=%d", device, BRIGHTNESS);
	system(command);
	}

int main(int argc, char *argv[]){
	int thread_handler[1];
	thread_handler[0] = pthread_create(&image_show, NULL, &processDisplay, NULL);
	
	open_device(argv[1]);
    init_device(FRAME_WIDTH, FRAME_HEIGHT);

	// v4l2-ctl -d /dev/video1 --list-formats-ext
	if (setFPS(atoi(argv[2]))) printf("Framerate set to : %d\n", atoi(argv[2]));
	set_props(argv[1], atoi(argv[3]));
	start_capturing();

	unsigned int cycle=0;
	clock_gettime(CLOCK_MONOTONIC, &start);
	while(wKey == -1 )
    {
		clock_gettime(CLOCK_MONOTONIC, &timestamp);
        ImageBuffer = snapFrame();

        if( ImageBuffer != NULL )
        {
            memmove(raw_frame.data, ImageBuffer, sizeof(char)*FRAME_SIZE);
//			clock_gettime(CLOCK_MONOTONIC, &timestamp);
			cycle++;
			if (timestamp.tv_sec-start.tv_sec > 1){
				printf("fps: %f\n", cycle / (double) (timestamp.tv_sec-start.tv_sec + (timestamp.tv_nsec-start.tv_nsec)/1e9));
				clock_gettime(CLOCK_MONOTONIC, &start);
				cycle= 0;
				}
		}
        else
        {
            printf("No image buffer retrieved.\n");
            break;
        }
    }
	
	destroyAllWindows() ;
	stop_capturing();
    uninit_device();
    close_device();

    printf("Program ended\n");

    return 0;

}
