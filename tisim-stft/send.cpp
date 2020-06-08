/*
  ./imagingsource_v4l2 /dev/video1 FRAMERATE EXPOSURE(us) SERVER-IP SERVER-PORT
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
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include "v4ldevice.cpp"
#include "stft_cuda.h"
#include "config.h"

using namespace cv;
using namespace std;

pthread_t image_show;
pthread_t image_GPU;
pthread_t image_network;

struct sockaddr_in si_other;
int sockfd, i, slen=sizeof(si_other);
long int netbuf[4];
    
typedef float TYPE;
char command[256]; // for parameter setting from shell

bool fps;
int wKey = -1;

Mat raw_frame(FRAME_HEIGHT, FRAME_WIDTH, CV_8UC1,Scalar(0));// Grayscale
Mat gray_frame(FRAME_HEIGHT, FRAME_WIDTH, CV_8UC1,Scalar(0));// Grayscale
Mat color_frame(FRAME_HEIGHT, FRAME_WIDTH, CV_8UC3);// Color image
Mat	Color_map(FRAME_HEIGHT, FRAME_WIDTH, CV_8UC3);
Mat Phaseimg_cpu1, Phase_diff_img_cpu1;
Mat peakfreq_temp;

unsigned int object_count;

/*
・Input image (VGA, JPEG compressed (unsigned char))
・Peak frequency image (VGA, JPEG compressed (unsigned char))
・gravity position (pairs 32-bit float)
・number of objects (8-bit integer)
・frequency response (arrays of 32-bit float).  
*/

struct dataset_image{
	unsigned char image_input[FRAME_SIZE];
	unsigned char image_peak[FRAME_SIZE*3];
} imagepacket, *imagepacket_ptr;

struct dataset_object{
	unsigned char object_num; // 1 to 10
	float centroid_x;
	float centroid_y;
	float freq[FREQ_BIN];
} object[OBJECT_COUNT_MAX], *object_ptr;

char *helper_ptr;

struct timespec start, stop, timestamp;

void* processDisplay(void *arg){
	printf("display enter\n");
	//namedWindow("Frequency_Image",CV_WINDOW_AUTOSIZE);//CV_WINDOW_OPENGL | CV_WINDOW_AUTOSIZE);//CV_WINDOW_OPENGL);
//	moveWindow("Frequency_Image",10,660);
	//~ namedWindow("Phase_img",CV_WINDOW_OPENGL);
	//~ moveWindow("Phase_img",500,10);
	//~ namedWindow("Phase_diff_img",CV_WINDOW_OPENGL);
	//~ moveWindow("Phase_diff_img",500,660);
	//~ Phaseimg_cpu1.create(FRAME_HEIGHT, FRAME_WIDTH, CV_8UC3);
	//~ Phase_diff_img_cpu1.create(FRAME_HEIGHT, FRAME_WIDTH, CV_8UC3);

//	namedWindow("iMAGING sOURCE",1); //CV_WINDOW_OPENGL | CV_WINDOW_AUTOSIZE); //
//	moveWindow("iMAGING sOURCE", 10, 10);
	while (wKey==-1)
	{
	//Phaseimg_cpu1 = convert_colormap(Phaseimg_cpu1);
	//Phase_diff_img_cpu1 = convert_colormap(Phase_diff_img_cpu1);
	
	//imshow("Phase_img", Phaseimg_cpu1);
	//imshow("Phase_diff_img", Phase_diff_img_cpu1);
	
	Color_map = convert_colormap(Color_map);
	imshow("Frequency_Image", Color_map);
	imshow("Input img", raw_frame); //color_frame);
	
	if (waitKey(1)==27) break;
	}
	wKey = 1;
	pthread_exit(NULL);
	printf("display exit\n");
}

//----------------------------------------------//
//			Initialize	GPU Processing			//
//----------------------------------------------//
void* processGPU(void *arg)
{
	printf("gpu enter\n");
	while(1) {ImageRead();
	if(wKey==-1)pthread_exit(NULL);
	}
	printf("gpu exit\n");
}

/*
・Input image (VGA, JPEG compressed (unsigned char))
・Peak frequency image (VGA, JPEG compressed (unsigned char))

・gravity position (pairs 32-bit float)
・number of objects (8-bit integer)
・frequency response (arrays of 32-bit float).  
*/


void* processNetwork(void *arg){
	int total_pack;
	while(1){
	
	// acquring data from other places, put it in the struct
	// image
	memmove(imagepacket.image_input, raw_frame.data, FRAME_SIZE);
	memmove(imagepacket.image_peak, Color_map.data, FRAME_SIZE*3);
	// objects
	// ..
	// ..
	
	// sending header, timestamp
	total_pack = 1 + sizeof(struct dataset_image) / PACK_SIZE;
    netbuf[0] = total_pack;
	netbuf[1] = timestamp.tv_sec;
    netbuf[2] = timestamp.tv_nsec;
    sendto(sockfd, netbuf, sizeof(long int)*3, 0, (struct sockaddr *) &si_other, slen);
    printf("timestamp %ld, totalpack %d\n",timestamp.tv_sec, total_pack);
    
	//sending content, image
	imagepacket_ptr = &imagepacket;
	helper_ptr= (char*) imagepacket_ptr;
	for (int i=0; i<total_pack; i++) {
		netbuf[0] = 0; // 0 means image package
		netbuf[1] = i; // sequence number of the package
		// sequence
		printf("image sequence: %d/%d %d\n", i, total_pack, sizeof(long int)*2);
		sendto(sockfd, netbuf, sizeof(long int)*2, 0, (struct sockaddr *) &si_other, slen);
    	// cotent
		printf("image content: %d/%d %d\n", i, total_pack, PACK_SIZE);
		sendto(sockfd, & helper_ptr[i*PACK_SIZE], PACK_SIZE, 0, (struct sockaddr *) &si_other, slen);
        }
	
	object_count=2;
	total_pack = 1 + sizeof(struct dataset_object) / PACK_SIZE;
	for (int num=0; num<object_count; num++){
		object_ptr = &(object[num]);
		helper_ptr= (char*) object_ptr;
		//sending content, object
		for (int i=0; i<total_pack; i++) {
			netbuf[0] = num+1;
			netbuf[1] = i;
			// sequence
			printf("object %d sequence: %d/%d %d\n", num, i, total_pack, sizeof(long int)*2);
			sendto(sockfd, netbuf, sizeof(long int)*2, 0, (struct sockaddr *) &si_other, slen);
			// cotent
    		printf("object %d content: %d/%d %d\n", num, i, total_pack, PACK_SIZE);
			sendto(sockfd, & helper_ptr[i*PACK_SIZE], PACK_SIZE, 0, (struct sockaddr *) &si_other, slen);
			//printf("sending content %d/%d size:%d\n", i+1, total_pack, PACK_SIZE);
			}
		}
	
	}
	printf("net exit");
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
	// init network
	if ((sockfd=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1) perror("socket");
	memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(atoi(argv[5]));
    if (inet_aton(argv[4], &si_other.sin_addr)==0){
		fprintf(stderr, "inet_aton() failed\n");
        exit(1);
        }
	
    initial_parameter(); // color me STFT
    unsigned char* ImageBuffer = NULL;
	unsigned char *image_;
	
	int thread_handler[3];
	thread_handler[0] = pthread_create(&image_show, NULL, &processDisplay, NULL);
	thread_handler[1] = pthread_create(&image_GPU, NULL, &processGPU, NULL);
	thread_handler[2] = pthread_create(&image_network, NULL, &processNetwork, NULL);

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
		
        ImageBuffer = snapFrame();

        if( ImageBuffer != NULL )
        {
            memmove(raw_frame.data, ImageBuffer, sizeof(char)*FRAME_SIZE);
			clock_gettime(CLOCK_MONOTONIC, &timestamp);
			cycle++;
			if (timestamp.tv_sec-start.tv_sec > 2){
				printf("fps: %f\n", cycle / (double) (timestamp.tv_sec-start.tv_sec + (timestamp.tv_nsec-start.tv_nsec)/1e9));
				clock_gettime(CLOCK_MONOTONIC, &start);
				cycle= 0;
				}
			
			image_ = raw_frame.data;
			Image_grab(image_) ;
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
