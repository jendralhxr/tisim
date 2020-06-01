/*
 *   This program is distributed in the hope that it will be useful,
 *   I just don't bother with licenses.
 *   
 *  compile against OpenCV
 *   $	g++ tisim-capture.cpp -o tisim-send `pkg-config opencv --libs` 
 *   $	./tisim-send <camera-device> 
 *  
 */

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>
#include "config.h"
#include "v4ldevice.cpp"

using namespace std;
using namespace cv;

char command[256];
unsigned char* buffer = NULL;
    
Mat raw = Mat(FRAME_HEIGHT, FRAME_WIDTH, CV_8UC1);
Mat image= Mat(FRAME_HEIGHT, FRAME_WIDTH, CV_8UC3);

struct timeval timeout;
struct timeval start, stop, timestamp;
unsigned int frames_count;
 
int print_caps(int fd){
	struct v4l2_capability caps = {};
	if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &caps))	{
		perror("Querying Capabilities");
		return 1;
		}
 
	printf( "Driver Caps:\n"
			"  Driver: \"%s\"\n"
			"  Card: \"%s\"\n"
			"  Bus: \"%s\"\n"
			"  Version: %d.%d\n"
			"  Capabilities: %08x\n",
			caps.driver,
			caps.card,
			caps.bus_info,
			(caps.version>>16)&&0xff,
			(caps.version>>24)&&0xff,
			caps.capabilities);
 
 
	struct v4l2_cropcap cropcap = {0};
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == xioctl (fd, VIDIOC_CROPCAP, &cropcap)){
		perror("Querying Cropping Capabilities");
		return 1;
	}

	struct v4l2_fmtdesc fmtdesc = {0};
	fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	char fourcc[5] = {0};
	char c, e;
	printf("  FMT : CE Desc\n--------------------\n");
	while (0 == xioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc)){
		strncpy(fourcc, (char *)&fmtdesc.pixelformat, 4);
		c = fmtdesc.flags & 1? 'C' : ' ';
		e = fmtdesc.flags & 2? 'E' : ' ';
		printf("  %s: %c%c %s\n", fourcc, c, e, fmtdesc.description);
		fmtdesc.index++;
		}
      
    struct v4l2_format fmt = {0};
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = FRAME_WIDTH;
	fmt.fmt.pix.height = FRAME_HEIGHT;
	//fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SRGGB8; // color
	//fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SRGGB10; // tegra
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_GREY; // greyscale

	fmt.fmt.pix.field = V4L2_FIELD_NONE;

	
	if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt)){
		perror("Setting Pixel Format");
		return 1;
		}
 
	strncpy(fourcc, (char *)&fmt.fmt.pix.pixelformat, 4);
	printf( "Selected Camera Mode:\n"
			"  Width: %d\n"
			"  Height: %d\n"
			"  PixFmt: %s\n"
			"  Field: %d\n",
			fmt.fmt.pix.width,
			fmt.fmt.pix.height,
			fourcc,
			fmt.fmt.pix.field);
	return 0;
	}
 
int set_props(char *device){
	sprintf(command, "v4l2-ctl -d %s -c exposure_auto=1", device);
	system(command);
	sprintf(command, "v4l2-ctl -d %s -c exposure_time_us=%d", device, EXPOSURE);
	system(command);
	//sprintf(command, "v4l2-ctl -d %s -c exposure_absolute=%d", device, EXPOSURE);
	//system(command);
	sprintf(command, "v4l2-ctl -d %s -c gain_auto=0", device);
	system(command);
	sprintf(command, "v4l2-ctl -d %s -c gain=%d", device, GAIN);
	system(command);
	sprintf(command, "v4l2-ctl -d %s -c brightness=%d", device, BRIGHTNESS);
	system(command);
	sprintf(command, "v4l2-ctl -d %s -p %d", device, 480);
	system(command);
	}
     
int main(int argc, char **argv){
	open_device(argv[1]);
	set_props(argv[1]);
    init_device(FRAME_WIDTH, FRAME_HEIGHT);
	setFPS(FPS);
	start_capturing();
	
	gettimeofday(&start,NULL);
	while(1){
		frames_count++;
		gettimeofday(&stop,NULL);
		
		buffer = snapFrame();
		if( buffer != NULL ){ 
            //color
			//memcpy(raw.imageData, buffer, sizeof(char)*FRAME_SIZE);
			memmove(raw.data, buffer, sizeof(char)*FRAME_SIZE);
			cvtColor(raw, image, COLOR_BayerBG2BGR);
			imshow( "Display window", image );
		
			// grey
			//memmove(raw.data, buffer, sizeof(char)*FRAME_SIZE);
			//imshow( "Display window", raw );                   
			waitKey(1);
			}
        else {
			printf("No image buffer retrieved.\n");
            break;
			}
		
		// update fps counter every second
		if ((1e6*(stop.tv_sec-start.tv_sec) +stop.tv_usec -start.tv_usec) > 1e6){
			printf("fps: %d\n", frames_count);
			//printf("fps: %f\n", frames_count/ (double)((stop.tv_sec - start.tv_sec) - (stop.tv_usec - start.tv_usec)/1e6 ));
	
			gettimeofday(&start,NULL);
			frames_count= 0;
			}
		}
		
	close(fd);
	return 0;
}
