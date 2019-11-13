/*
 *   This program is distributed in the hope that it will be useful,
 *   I just don't bother with licenses.
 *   
 *  compile against OpenCV
 *   $	g++ tisim-global.cpp -o tisim-global `pkg-config opencv --libs` 
 *   $	./tisim-global <camera-device> <exposure> <number of frames>  <save directory> <receiver-host> <receiver-port>
 * 
 * adapted from:
 * https://gist.github.com/Circuitsoft/1126411
 * https://github.com/chenxiaoqino/udp-image-streaming/
 * https://jameshfisher.com/2017/04/05/set_socket_nonblocking/
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include "PracticalSocket.h"      // For UDPSocket and SocketException
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>
#include "config.h" // camera parameters

using namespace std;
using namespace cv;

char command[256];
char hostname[256];
char filename[256];
    
uint8_t *buffer;
uint8_t **buffer_list;

fd_set fds;
int fd; // camera file descriptor

Mat raw = Mat(FRAME_HEIGHT, FRAME_WIDTH, CV_8UC1);
Mat image= Mat(FRAME_HEIGHT, FRAME_WIDTH, CV_8UC3);

struct v4l2_buffer buf;
struct timeval timeout;
struct timeval *timestamp;
unsigned int frames_count;

long int ibuf[3]; // header
int total_pack;

string servAddress;
unsigned short servPort;
UDPSocket sock;
#define PORT_TRIGGER 9999

// image for viewing prior to capture
vector < int > compression_params;
vector < uchar > encoded;
            
int wait;

int guard(int n, char * err) { if (n == -1) { perror(err); exit(1); } return n; }


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
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SRGGB8; // color
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
	sprintf(command, "v4l2-ctl -d %s -c white_balance_component_auto=0", device);
	system(command);
	sprintf(command, "v4l2-ctl -d %s -c white_balance_red_component=%d", device, HUE_RED);
	system(command);
	sprintf(command, "v4l2-ctl -d %s -c white_balance_blue_component=%d", device, HUE_BLUE);
	system(command);
	sprintf(command, "v4l2-ctl -d %s -c white_balance_green_component=%d", device, HUE_GREEN);
	system(command);
	}
	
	
int capture_image(int fd, int num){
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

	 FD_ZERO(&fds);
    FD_SET(fd, &fds);
   /* timeout can go away atm 
    timeout.tv_sec = 2;
    int r = select(fd+1, &fds, NULL, NULL, &timeout);
    if(-1 == r) // timout after 2 secs
    {
        perror("Waiting for Frame, timeout");
        return 1;
    }
 */
    if(-1 == xioctl(fd, VIDIOC_DQBUF, &buf))
    {
        perror("Retrieving Frame");
        return 1;
    }
    
    if (wait){
	    memmove(raw.data, buffer, sizeof(char)*FRAME_SIZE);
		}
    else memmove(buffer_list[num], buffer, sizeof(char)*FRAME_SIZE);
    return 0;
}
     
int main(int argc, char **argv){
	gethostname(hostname, 256);
	// printf("host: %s", hostname);
	servAddress = argv[5]; // server address
    servPort = Socket::resolveService(argv[6], "udp");
	printf("sending to %s:%d\n", servAddress.c_str(), servPort);
	
	// for trigger, TCP
	int listen_socket_fd = guard(socket(AF_INET, SOCK_STREAM, 0), "could not create TCP listening socket");
	int flags = guard(fcntl(listen_socket_fd, F_GETFL), "could not get flags on TCP listening socket");
	guard(fcntl(listen_socket_fd, F_SETFL, flags | O_NONBLOCK), "could not set TCP listening socket to be non-blocking");
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT_TRIGGER);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	guard(bind(listen_socket_fd, (struct sockaddr *) &addr, sizeof(addr)), "could not bind");
	guard(listen(listen_socket_fd, 100), "could not listen");
	
	// device memory buffer map
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
	
	
    int framenum, framenum_max;
	framenum_max= atoi(argv[3]);
	printf("frames to save to %s: %d\n", argv[4], framenum_max);
    buffer_list= (uint8_t**) malloc(sizeof(uint8_t*) * framenum_max);		 
	timestamp= (struct timeval*) malloc(sizeof(struct timeval) * framenum_max);
	for (int framenum= 0; framenum<framenum_max; framenum++){
		buffer_list[framenum]= (uint8_t*) malloc(sizeof(uint8_t)*FRAME_SIZE);		 
		}
        
	set_props(argv[1], atoi(argv[2]));
    
    wait=1;
    
    // wait for trigger, display
    while (wait){
		if(capture_image(fd, 0)) return 1;
		cvtColor(raw, image, CV_BayerBG2BGR);
		imencode(".jpg", image, encoded, compression_params);
		total_pack = 1 + (encoded.size() - 1) / PACK_SIZE;
	    ibuf[0] = total_pack;
	    gettimeofday(&(timestamp[0]), NULL);
		ibuf[1] = timestamp[0].tv_sec;
	    ibuf[2] = timestamp[0].tv_usec;
	    //printf("sending to %s:%d wait%ld\n", servAddress.c_str(), servPort, timestamp[0].tv_sec);
		sock.sendTo(ibuf, sizeof(long int) *3, servAddress, servPort); // sending header
		for (int i = 0; i < total_pack; i++) {
			sock.sendTo( & encoded[i * PACK_SIZE], PACK_SIZE, servAddress, servPort);
			// printf("sending content %d/%d size:%d\n", i+1, total_pack, PACK_SIZE);
			}

	    // waiting for trigger
		int client_socket_fd = accept(listen_socket_fd, NULL, NULL);
	    if (client_socket_fd == -1){
		    if (errno == EWOULDBLOCK){
		        //printf("No pending connections; sleeping.\n");
		        //sleep(1);
			    } 
			else {
			    perror("error when accepting connection");
		        exit(1);
				}
			} 
		else {
		    char msg[] = "grab!\x4\x3";
		    close(client_socket_fd);
		    printf("start recording\n");
			wait=0;
			break;
		    }
	 	}
	    
    // capture   
	for (int framenum= 0; framenum<framenum_max; framenum++){
		gettimeofday(&(timestamp[framenum]), NULL);
		if(capture_image(fd, framenum)) return 1;
		}
	
	// saving
	for (int framenum= 0; framenum<framenum_max; framenum++){
		memmove(raw.data, buffer_list[framenum], sizeof(char)*FRAME_SIZE);
		cvtColor(raw, image, CV_BayerBG2BGR);
		sprintf(filename, "%s/%s-%ld%06ld.tif", argv[4], hostname, timestamp[framenum].tv_sec, timestamp[framenum].tv_usec);
		printf("saving: %s\n", filename); 
		imwrite(filename, image);
	}
		
	close(fd);
	return 0;
}
