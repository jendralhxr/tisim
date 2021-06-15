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

using namespace std;
using namespace cv;

unsigned long int framenum;
Mat image;

struct v4l2_buffer buf;
struct timeval timeout;
struct timeval start, stop, timestamp;
unsigned int frames_count;

std::vector<std::string> objects_names_from_file(std::string const filename) {
    std::ifstream file(filename);
    std::vector<std::string> file_lines;
    if (!file.is_open()) return file_lines;
    for(std::string line; getline(file, line);) file_lines.push_back(line);
    std::cout << "object names loaded \n";
    return file_lines;
}

//yolo
std::string  names_file = "drone.names";
std::string  cfg_file = "yolo-drone.cfg";
std::string  weights_file = "yolo-drone.weights";
vector <string> obj_names = objects_names_from_file(names_file); 
Detector detector(cfg_file, weights_file);
float thresh =  0.01;
 
void draw_boxes(cv::Mat mat_img, std::vector<bbox_t> result_vec, std::vector<std::string> obj_names,
    int current_det_fps = -1, int current_cap_fps = -1){
    int const colors[6][3] = { { 1,0,1 },{ 0,0,1 },{ 0,1,1 },{ 0,1,0 },{ 1,1,0 },{ 1,0,0 } };

    for (auto &i : result_vec) {
        cv::Scalar color = obj_id_to_color(i.obj_id);
        cv::rectangle(mat_img, cv::Rect(i.x, i.y, i.w, i.h), color, 2);
        putText(mat_img, std::to_string(i.prob), cv::Point2f(i.x, i.y + 16), cv::FONT_HERSHEY_COMPLEX_SMALL, 0.8, cv::Scalar(0, 0, 0), 2);
		}
    }


void show_console_result(std::vector<bbox_t> const result_vec, std::vector<std::string> const obj_names, int frame_id = -1) {
    for (auto &i : result_vec) {
        if (obj_names.size() > i.obj_id) std::cout << obj_names[i.obj_id] << " - ";
        std::cout << "framenum= " << framenum << ",  x= " << i.x << ", y= " << i.y
            << ", w= " << i.w << ", h= " << i.h
            << std::setprecision(3) << ", prob= " << i.prob << std::endl;
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
	
int main(int argc, char **argv){
	cout << "file :" << argv[1] << endl;
	VideoCapture cap(argv[1]);
	
	int width = int(cap.get(CAP_PROP_FRAME_WIDTH));
	int height = int(cap.get(CAP_PROP_FRAME_HEIGHT));
	
	thresh = atof(argv[2]);
	VideoWriter output(argv[3], cap.get(CAP_PROP_FOURCC), cap.get(CAP_PROP_FPS), Size(width, height));
	
	if(!cap.isOpened()){
	    cout << "Error opening video stream or file" << endl;
	    return -1;
	  }
	Mat frame;
	std::vector<bbox_t> result_vec;
	
	gettimeofday(&start,NULL);
	while(1){
		cap >> image;
		
		if (image.empty()) break;
		
		framenum++;
		//image = frame;
		result_vec = detector.detect(image, thresh, true);  // true
		draw_boxes(image, result_vec, obj_names);
		show_console_result(result_vec, obj_names);
		//cv::imshow("detection", image);
		//cv::waitKey(1);
		output.write(image);
		
		frames_count++;
		gettimeofday(&stop,NULL);
		
		// update fps counter every second
		if ((1e6*(stop.tv_sec-start.tv_sec) +stop.tv_usec -start.tv_usec) > 1e6){
			printf("fps: %d %ld\n", frames_count, stop.tv_sec);
			gettimeofday(&start,NULL);
			frames_count= 0;
			}
		}
	
	cap.release();
	output.release();
	
	return 0;
}
