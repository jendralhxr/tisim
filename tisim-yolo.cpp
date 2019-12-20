/*
 *   This program is distributed in the hope that it will be useful,
 *   I just don't bother with licenses.
 *   
 *  compile against OpenCV
 *   $	g++ tisim-send.cpp -o tisim-send `pkg-config opencv --libs` 
 *   $	./tisim-yolo <camera-device> <receiver-host> <port>
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
#include <thread>
#include <fstream>

// darknet yolo detection w/ opencv
#define GPU
#define OPENCV
#include "yolo_v2_class.hpp"

#include "PracticalSocket.h"      // For UDPSocket and SocketException
#include "config.h" // camera parameters

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

long int ibuf[3]; // header
int total_pack;

string servAddress;
unsigned short servPort;
UDPSocket sock;
vector <int> compression_params;
vector <uchar> encoded;

std::vector<std::string> objects_names_from_file(std::string const filename) {
    std::ifstream file(filename);
    std::vector<std::string> file_lines;
    if (!file.is_open()) return file_lines;
    for(std::string line; getline(file, line);) file_lines.push_back(line);
    std::cout << "object names loaded \n";
    return file_lines;
}



//yolo
std::string  names_file = "coco.names";
std::string  cfg_file = "yolov3-tiny.cfg";
std::string  weights_file = "yolov3-tiny.weights";
//std::string  cfg_file = "yolov3/cfg/yolov3-spp-1cls.cfg";
//std::string  weights_file = "yolov3/weights/last1cls_1clscfg.pt";
vector <string> obj_names = objects_names_from_file(names_file); 
Detector detector(cfg_file, weights_file);
//float const thresh =  0.2;
            
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
	//fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_SRGGB10; // tegra
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
	sprintf(command, "v4l2-ctl -d %s -c white_balance_component_auto=0", device);
	system(command);
	sprintf(command, "v4l2-ctl -d %s -c white_balance_red_component=%d", device, HUE_RED);
	system(command);
	sprintf(command, "v4l2-ctl -d %s -c white_balance_blue_component=%d", device, HUE_BLUE);
	system(command);
	sprintf(command, "v4l2-ctl -d %s -c white_balance_green_component=%d", device, HUE_GREEN);
	system(command);
	}

void draw_boxes(cv::Mat mat_img, std::vector<bbox_t> result_vec, std::vector<std::string> obj_names,
    int current_det_fps = -1, int current_cap_fps = -1)
{
    int const colors[6][3] = { { 1,0,1 },{ 0,0,1 },{ 0,1,1 },{ 0,1,0 },{ 1,1,0 },{ 1,0,0 } };

    for (auto &i : result_vec) {
        cv::Scalar color = obj_id_to_color(i.obj_id);
        cv::rectangle(mat_img, cv::Rect(i.x, i.y, i.w, i.h), color, 2);
        if (obj_names.size() > i.obj_id) {
            std::string obj_name = obj_names[i.obj_id];
            if (i.track_id > 0) obj_name += " - " + std::to_string(i.track_id);
            cv::Size const text_size = getTextSize(obj_name, cv::FONT_HERSHEY_COMPLEX_SMALL, 1.2, 2, 0);
            //cv::Size const text_size = getTextSize(obj_name, cv::FONT_HERSHEY_COMPLEX_SMALL, 0, 0, 0);
            
            int max_width = (text_size.width > i.w + 2) ? text_size.width : (i.w + 2);
            max_width = std::max(max_width, (int)i.w + 2);
            //max_width = std::max(max_width, 283);
            std::string coords_3d;
            if (!std::isnan(i.z_3d)) {
                std::stringstream ss;
                ss << std::fixed << std::setprecision(2) << "x:" << i.x_3d << "m y:" << i.y_3d << "m z:" << i.z_3d << "m ";
                coords_3d = ss.str();
                cv::Size const text_size_3d = getTextSize(ss.str(), cv::FONT_HERSHEY_COMPLEX_SMALL, 0.8, 1, 0);
                int const max_width_3d = (text_size_3d.width > i.w + 2) ? text_size_3d.width : (i.w + 2);
                if (max_width_3d > max_width) max_width = max_width_3d;
            }

            cv::rectangle(mat_img, cv::Point2f(std::max((int)i.x - 1, 0), std::max((int)i.y - 35, 0)),
                cv::Point2f(std::min((int)i.x + max_width, mat_img.cols - 1), std::min((int)i.y, mat_img.rows - 1)),
                color, CV_FILLED, 8, 0);
            //putText(mat_img, obj_name, cv::Point2f(i.x, i.y - 16), cv::FONT_HERSHEY_COMPLEX_SMALL, 1.2, cv::Scalar(0, 0, 0), 2);
            putText(mat_img, obj_name, cv::Point2f(i.x, i.y - 16), cv::FONT_HERSHEY_COMPLEX_SMALL, 0.8, cv::Scalar(0, 0, 0), 2);
            if(!coords_3d.empty()) putText(mat_img, coords_3d, cv::Point2f(i.x, i.y-1), cv::FONT_HERSHEY_COMPLEX_SMALL, 0.8, cv::Scalar(0, 0, 0), 1);
        }
    }
    if (current_det_fps >= 0 && current_cap_fps >= 0) {
        std::string fps_str = "FPS detection: " + std::to_string(current_det_fps) + "   FPS capture: " + std::to_string(current_cap_fps);
        putText(mat_img, fps_str, cv::Point2f(10, 20), cv::FONT_HERSHEY_COMPLEX_SMALL, 1.2, cv::Scalar(50, 255, 0), 2);
    }
}

void show_console_result(std::vector<bbox_t> const result_vec, std::vector<std::string> const obj_names, int frame_id = -1) {
    std::cout << std::endl;
    for (auto &i : result_vec) {
        if (obj_names.size() > i.obj_id) std::cout << obj_names[i.obj_id] << " - ";
        std::cout << "obj_id = " << i.obj_id << ",  x = " << i.x << ", y = " << i.y
            << ", w = " << i.w << ", h = " << i.h
            << std::setprecision(3) << ", prob = " << i.prob << std::endl;
    }
}


template<typename T>
class send_one_replaceable_object_t {
    const bool sync;
    std::atomic<T *> a_ptr;
public:

    void send(T const& _obj) {
        T *new_ptr = new T;
        *new_ptr = _obj;
        if (sync) {
            while (a_ptr.load()) std::this_thread::sleep_for(std::chrono::milliseconds(3));
        }
        std::unique_ptr<T> old_ptr(a_ptr.exchange(new_ptr));
    }

    T receive() {
        std::unique_ptr<T> ptr;
        do {
            while(!a_ptr.load()) std::this_thread::sleep_for(std::chrono::milliseconds(3));
            ptr.reset(a_ptr.exchange(NULL));
        } while (!ptr);
        T obj = *ptr;
        return obj;
    }

    bool is_object_present() {
        return (a_ptr.load() != NULL);
    }

    send_one_replaceable_object_t(bool _sync) : sync(_sync), a_ptr(NULL)
    {}
};
	 
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
	// namedWindow( "Display window", WINDOW_AUTOSIZE );// Create a window for display.
	// imshow( "Display window", image );                   // Show our image inside it.
	// waitKey(1);                                          // Wait for a keystroke in the window

    // grey
	// memmove(raw.data, buffer, sizeof(char)*FRAME_SIZE);
	// imencode(".jpg", raw, encoded, compression_params);
	// imshow( "Display window", raw );                   
	return 0;
}
     
int send_image(){
	// color
	// here goes transmitting routine
	imencode(".jpg", image, encoded, compression_params);
	total_pack = 1 + (encoded.size() - 1) / PACK_SIZE;
    ibuf[0] = total_pack;
    gettimeofday(&timestamp, NULL);
    ibuf[1] = timestamp.tv_sec;
    ibuf[2] = timestamp.tv_usec;
    sock.sendTo(ibuf, sizeof(long int) *3, servAddress, servPort);
    //~ printf("sending header %ld\n",sizeof(long int) *3);
        
    for (int i = 0; i < total_pack; i++) {
		sock.sendTo( & encoded[i * PACK_SIZE], PACK_SIZE, servAddress, servPort);
		//~ printf("sending content %d/%d size:%d\n", i+1, total_pack, PACK_SIZE);
    }
	
	// waitKey(1);
	// waitKey(FRAME_INTERVAL);
	
    // char filename[20];
    // sprintf(filename, "/tmp/walo.jpg");
    // imwrite(filename, image);
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
	
	set_props(argv[1]); // setting camera properties
     
    compression_params.push_back(IMWRITE_JPEG_QUALITY);
    compression_params.push_back(ENCODE_QUALITY);
	auto obj_names = objects_names_from_file(names_file);
	
    
	try{
	servAddress = argv[2]; // First arg: server address
    servPort = Socket::resolveService(argv[3], "udp");
	printf("sending to %s:%d\n", servAddress.c_str(), servPort);
        
	gettimeofday(&start,NULL);
	while(1){
		if(capture_image(fd)) return 1;
		else {
			std::vector<bbox_t> result_vec = detector.detect(image);
			draw_boxes(image, result_vec, obj_names);
			//show_console_result(result_vec, obj_names);
			//cv::imshow("window name", image);
			//cv::waitKey(20);
			send_image();
			}
		
		frames_count++;
		gettimeofday(&stop,NULL);
		
		// update fps counter every second
		if ((1e6*(stop.tv_sec-start.tv_sec) +stop.tv_usec -start.tv_usec) > 1e6){
			printf("send fps: %d %ld\n", frames_count, stop.tv_sec);
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
