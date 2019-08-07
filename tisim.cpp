/* sample code for ImagingSource camera 
   compile against OpenCV
     $	g++ tisim.cpp -o tisim `pkg-config opencv --libs` 
     $	./tisim /dev/video1

adapted from https://gist.github.com/Circuitsoft/1126411
*/

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>

#define FRAME_HEIGHT 480
#define FRAME_WIDTH 640
#define FRAME_SIZE 307200 // 640x480

#define GAIN 10 // not really doing anything
#define FPS 600 // maximum fps
#define EXPOSURE 2000 // us
#define BRIGHTNESS 200

using namespace cv;

char command[256];
uint8_t *buffer;
fd_set fds;
int fd; // camera file descriptor

Mat raw16 = Mat(480, 640, CV_16UC1);
Mat raw = Mat(480, 640, CV_8UC1);
Mat image= Mat(480, 640, CV_8UC3);
Mat image16= Mat(480, 640, CV_16UC3);

struct v4l2_buffer buf;
struct timeval timeout;
struct timeval start, stop, *timestamp;
unsigned int frames_count;
        
static int xioctl(int fd, int request, void *arg){
	int r;
	do r = ioctl (fd, request, arg);
	while (-1 == r && EINTR == errno);
	return r;
}
 
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
	fmt.fmt.pix.width = 640;
	fmt.fmt.pix.height = 480;
	//fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SRGGB8; // color
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SRGGB10; // tegra
	//fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_GREY; // greyscale

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
 
int init_mmap(int fd){
    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
 
    if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req))
    {
        perror("Requesting Buffer");
        return 1;
    }
 
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if(-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
    {
        perror("Querying Buffer");
        return 1;
    }
 
    buffer = (unsigned char*) mmap (NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    printf("Length: %d\nAddress: %p\n", buf.length, buffer);
    printf("Image Length: %d\n", buf.bytesused);
 
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
	}
	 
int capture_image(int fd){
    if(-1 == xioctl(fd, VIDIOC_QBUF, &buf))
    {
        perror("Query Buffer");
        return 1;
    }
 
    if(-1 == xioctl(fd, VIDIOC_STREAMON, &buf.type))
    {
        perror("Start Capture");
        return 1;
    }

	// timing from camera
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    
    timeout.tv_sec = 2;
    int r = select(fd+1, &fds, NULL, NULL, &timeout);
    if(-1 == r) // timout after 2 secs
    {
        perror("Waiting for Frame, timeout");
        return 1;
    }
 
    if(-1 == xioctl(fd, VIDIOC_DQBUF, &buf))
    {
        perror("Retrieving Frame");
        return 1;
    }
    
    //tegra camera
    //memmove(raw16.data, buffer, sizeof(char)*FRAME_SIZE*2);
    //raw = raw16.clone();
    //raw.convertTo(raw, CV_8UC1, 0.0625);
    //cvtColor(raw, image, CV_BayerBG2BGR);
    //imshow( "Display window", image );                   // Show our image inside it.
	    
    //color
    memmove(raw.data, buffer, sizeof(char)*FRAME_SIZE);
    cvtColor(raw, image, CV_BayerBG2BGR);
    imshow( "Display window", image );                   // Show our image inside it.
	
    // grey
    //memmove(raw.data, buffer, sizeof(char)*FRAME_SIZE);
    //imshow( "Display window", raw );                   
	
	
    waitKey(1);                                          // Wait for a keystroke in the window
    
    //char filename[20];
    //sprintf(filename, "image%d.jpg",i);
    //imwrite(filename, image);
    return 0;
}
     
int main(int argc, char **argv){

	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = 0;
 
	fd = open(argv[1], O_RDWR);
	if (fd == -1){
		perror("Opening video device");
		return 1;
	}
	if(print_caps(fd)){
		perror("Fail to access camera properties");
		return 1;
		}
	if(init_mmap(fd)){
		perror("Fail to initialize memory");
		return 1;
		}  
	
	set_props(argv[1]);

	gettimeofday(&start,NULL);
	while(1){
		if(capture_image(fd)) return 1;
		frames_count++;
		gettimeofday(&stop,NULL);
		
		// update fps counter every second
		if ((1e6*(stop.tv_sec-start.tv_sec) +stop.tv_usec -start.tv_usec) > 1e6){
			printf("fps: %d\n", frames_count);
			gettimeofday(&start,NULL);
			frames_count= 0;
			}
		}
	
	close(fd);
	return 0;
}
 
