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

unsigned int objects_count;

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
	float blob_size;
	float freq[FREQ_BIN];
} object[OBJECT_COUNT_MAX], *object_ptr;

struct sending_buffer{
	short int id; 
	short int sequence;
	char data[PACK_SIZE];
	} sendbuf, *sendbuf_ptr;
	
char *helper_ptr, plainbuf[8000];

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
	//imshow("Frequency_Image", Color_map);
	//imshow("Input img", raw_frame); //color_frame);
	
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

void* cvblob(){
	float moment_x[OBJECT_COUNT_MAX], moment_y[OBJECT_COUNT_MAX], mass[OBJECT_COUNT_MAX];
	float moment_x_temp, moment_y_temp, mass_temp;
	
	SimpleBlobDetector::Params pDefaultBLOB;
    // This is default parameters for SimpleBlobDetector
    pDefaultBLOB.thresholdStep = 10;
    pDefaultBLOB.minThreshold = 20;
    pDefaultBLOB.maxThreshold = 220;
    pDefaultBLOB.minRepeatability = 2;
    pDefaultBLOB.minDistBetweenBlobs = 10;
    pDefaultBLOB.filterByColor = false;
    pDefaultBLOB.blobColor = 0;
    pDefaultBLOB.filterByArea = false;
    pDefaultBLOB.minArea = 25;
    pDefaultBLOB.maxArea = 5000;
    pDefaultBLOB.filterByCircularity = false;
    pDefaultBLOB.minCircularity = 0.9f;
    pDefaultBLOB.maxCircularity = (float)1e37;
    pDefaultBLOB.filterByInertia = false;
    pDefaultBLOB.minInertiaRatio = 0.1f;
    pDefaultBLOB.maxInertiaRatio = (float)1e37;
    pDefaultBLOB.filterByConvexity = false;
    pDefaultBLOB.minConvexity = 0.95f;
    pDefaultBLOB.maxConvexity = (float)1e37;
	
	// Detect blobs.
	vector<KeyPoint> keypoints;
	Ptr<SimpleBlobDetector> sbd = SimpleBlobDetector::create(pDefaultBLOB);
	sbd->detect(Color_map, keypoints, Mat());
    
	objects_count=0; 
	for (std::vector<KeyPoint>::iterator it = keypoints.begin(); it != keypoints.end(); ++it){
		object[objects_count].centroid_x = it->pt.x;
		object[objects_count].centroid_y = it->pt.y;
		object[objects_count].blob_size = it->size;
		//moment_x[n]= it->pt.x;
		//moment_y[n]= it->pt.y;
		//mass[n]= it->size;
		objects_count++;
		} 
	
	/* // little sorting
	for (int i=n-1; i>=0; i--){
		for (int j=i-1; j>=0; j--){
			if (moment_x[i] > moment_x[j]){
				moment_x_temp= moment_x[i];
				moment_y_temp= moment_y[i];
				mass_temp= mass[i];
				moment_x[i]= moment_x[j];
				moment_y[i]= moment_y[j];
				mass[i]= mass[j];
				moment_x[j]= moment_x_temp;
				moment_y[j]= moment_y_temp;
				mass[j]= mass_temp;
				}
			}
		} */
	}

void* processNetwork(void *arg){
	int total_pack;
	int sentlen;
	
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
    char mbuf[8];
    memcpy(mbuf, netbuf, 32);
    sentlen= sendto(sockfd, mbuf, 32, 0, (struct sockaddr *) &si_other, slen);
    
    //sending content, image
    imagepacket_ptr = &imagepacket;
    helper_ptr= (char*) imagepacket_ptr;
	for (int i=0; i<total_pack; i++) {
		sendbuf.id= 0;
		sendbuf.sequence= i;
		memmove(sendbuf.data, & helper_ptr[i*PACK_SIZE], PACK_SIZE);
		//printf("sending %u image %u/%d size:%lu\n", sendbuf.id, sendbuf.sequence, total_pack, sizeof(struct sending_buffer));
		sendto(sockfd, &sendbuf, sizeof(struct sending_buffer), 0, (struct sockaddr *) &si_other, slen);
        }
	
	total_pack = 1 + sizeof(struct dataset_object) / PACK_SIZE;
	for (int num=0; num<objects_count; num++){
		object_ptr = &(object[num]);
		helper_ptr= (char*) object_ptr;
		//sending content, object
		for (int i=0; i<total_pack; i++) {
			sendbuf.id= num+1;
			sendbuf.sequence= i;
			memmove(sendbuf.data, & helper_ptr[i*PACK_SIZE], PACK_SIZE);
			sendto(sockfd, &sendbuf, sizeof(struct sending_buffer), 0, (struct sockaddr *) &si_other, slen);
			//printf("sending %d object %d/%d size:%lu\n", sendbuf.id, sendbuf.sequence, total_pack, sizeof(struct sending_buffer));
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
		clock_gettime(CLOCK_MONOTONIC, &timestamp);
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
			cvblob();
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
