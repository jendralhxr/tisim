/*
 *   This program is distributed in the hope that it will be useful,
 *   I just don't bother with licenses.
 *   
 *  compile against OpenCV
 *   $	g++ tisim-send.cpp -o tisim-send `pkg-config opencv --libs` 
 *   $	./tisim-send <camera-device> <receiver-host> <port>
 * 
 * adapted from:
 * https://gist.github.com/Circuitsoft/1126411
 * https://github.com/chenxiaoqino/udp-image-streaming/
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

#include "PracticalSocket.h"      // For UDPSocket and SocketException
#include "config.h"

using namespace std;
using namespace cv;

char command[256];
uint8_t *buffer;
fd_set fds;
int fd; // camera file descriptor

Mat raw16 = Mat(480, 640, CV_16UC1);
Mat image16= Mat(480, 640, CV_16UC3);
Mat raw = Mat(480, 640, CV_8UC1);
Mat image= Mat(480, 640, CV_8UC3);

struct v4l2_buffer buf;
struct timeval timeout;
struct timeval start, stop, timestamp;
unsigned int frames_count;

struct header{
	int pack_num;
	char info[INFO_LEN];
	} img_header;

string servAddress;
unsigned short servPort;
UDPSocket sock;
vector < int > compression_params;
vector < uchar > encoded;
            
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
    //imshow( "Display window", image );                   // Show our image inside it.
	
    // grey
    //memmove(raw.data, buffer, sizeof(char)*FRAME_SIZE);
    //imshow( "Display window", raw );                   
	
	// here goes transmitting routine
	imencode(".jpg", image, encoded, compression_params);
	int total_pack = 1 + (encoded.size() - 1) / PACK_SIZE;
    int ibuf[1];
    ibuf[0] = total_pack;
    
    gettimeofday(&timestamp, NULL);
    
    img_header.pack_num= total_pack;
    sprintf(img_header.info, "timestamp %ld %ld\n",timestamp.tv_sec, timestamp.tv_usec);
    
    //sock.sendTo(&img_header, sizeof(struct header), servAddress, 8080);
    //printf("sending header %ld\n",sizeof(struct header));
    
    sock.sendTo(ibuf, sizeof(int), servAddress, servPort);
    printf("sending header %ld\n",sizeof(int));
        
    for (int i = 0; i < total_pack; i++) {
		sock.sendTo( & encoded[i * PACK_SIZE], PACK_SIZE, servAddress, servPort);
		printf("sending content %d/%d size:%d\n", i+1, total_pack, PACK_SIZE);
    }
	
//    waitKey(1);
//    waitKey(FRAME_INTERVAL);
	
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
     
    compression_params.push_back(CV_IMWRITE_JPEG_QUALITY);
    compression_params.push_back(ENCODE_QUALITY);

	try{
	servAddress = argv[2]; // First arg: server address
    servPort = Socket::resolveService(argv[3], "udp");
	printf("sending to %s:%d\n", servAddress.c_str(), servPort);

        
	gettimeofday(&start,NULL);
	while(1){
		if(capture_image(fd)) return 1;
		frames_count++;
		gettimeofday(&stop,NULL);
		
		// update fps counter every second
		if ((1e6*(stop.tv_sec-start.tv_sec) +stop.tv_usec -start.tv_usec) > 1e6){
			printf("send fps: %d\n", frames_count);
			gettimeofday(&start,NULL);
			frames_count= 0;
			}
		}
	}
	catch (SocketException & e) {
        cerr << e.what() << endl;
        exit(1);
    }
		
	close(fd);
	return 0;


}
