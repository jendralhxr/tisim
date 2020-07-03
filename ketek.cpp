/*
 *   g++ ketek.cpp `pkg-config opencv --libs` -ldarknet -o ketek
 */

#include <string.h>
#include <vector>
#include <sys/time.h>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <fstream>

// darknet yolo detection w/ opencv
#define GPU
#define OPENCV
#include "yolo_v2_class.hpp"

using namespace cv;
using namespace std;	

struct timeval start, stop, timestamp;
unsigned int frames_count;

void draw_boxes(cv::Mat mat_img, std::vector<bbox_t> result_vec, std::vector<std::string> obj_names,
    int current_det_fps = -1, int current_cap_fps = -1){
    int const colors[6][3] = { { 1,0,1 },{ 0,0,1 },{ 0,1,1 },{ 0,1,0 },{ 1,1,0 },{ 1,0,0 } };

    for (auto &i : result_vec) {
        if ((i.obj_id!=2) && (i.obj_id!=5) && (i.obj_id!=7)) continue; // car, bus, truck
        cv::Scalar color = obj_id_to_color(i.obj_id);
        cv::rectangle(mat_img, cv::Rect(i.x, i.y, i.w, i.h), color, 2);
        if (obj_names.size() > i.obj_id) {
            std::string obj_name = obj_names[i.obj_id];
            if (i.track_id > 0) obj_name += " - " + std::to_string(i.track_id);
            cv::Size const text_size = getTextSize(obj_name, cv::FONT_HERSHEY_COMPLEX_SMALL, 1.2, 2, 0);
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
            putText(mat_img, obj_name, cv::Point2f(i.x, i.y - 16), cv::FONT_HERSHEY_COMPLEX_SMALL, 1.2, cv::Scalar(0, 0, 0), 2);
            if(!coords_3d.empty()) putText(mat_img, coords_3d, cv::Point2f(i.x, i.y-1), cv::FONT_HERSHEY_COMPLEX_SMALL, 0.8, cv::Scalar(0, 0, 0), 1);
        }
    }
    if (current_det_fps >= 0 && current_cap_fps >= 0) {
        std::string fps_str = "FPS detection: " + std::to_string(current_det_fps) + "   FPS capture: " + std::to_string(current_cap_fps);
        putText(mat_img, fps_str, cv::Point2f(10, 20), cv::FONT_HERSHEY_COMPLEX_SMALL, 1.2, cv::Scalar(50, 255, 0), 2);
    }
}

void show_console_result(std::vector<bbox_t> const result_vec, std::vector<std::string> const obj_names, int frame_id = -1) {
    std::cout << frames_count << ";";
    for (auto &i : result_vec) {
		if ((i.obj_id!=2) && (i.obj_id!=5) && (i.obj_id!=7)) continue; // car, bus, truck
        if (obj_names.size() > i.obj_id) std::cout << obj_names[i.obj_id] << "," << i.x << "," << i.y
            << "," << i.w << "," << i.h << std::setprecision(3) << "," << i.prob << ";";
		}
    std::cout << std::endl;
}

std::vector<std::string> objects_names_from_file(std::string const filename) {
    std::ifstream file(filename);
    std::vector<std::string> file_lines;
    if (!file.is_open()) return file_lines;
    for(std::string line; getline(file, line);) file_lines.push_back(line);
    std::cout << "object names loaded \n";
    return file_lines;
}

cv::Mat image;
	
int main(int argc, char **argv){
	gettimeofday(&start,NULL);
	VideoCapture cap(argv[1]); 
	
	
	//yolo
	std::string cfg_file = argv[2];
	std::string weights_file = argv[3];
	std::vector <std::string> obj_names = objects_names_from_file("coco.names"); 
	Detector detector(cfg_file, weights_file);
	float const thresh =  0.2;
	
	cap >> image;
	VideoWriter writer(argv[4], VideoWriter::fourcc('M','P','4','V'), 30, Size(608,608), true);
	
	while(1){
		cap >> image;
		if (image.empty()) break;
		
		vector<bbox_t> result_vec = detector.detect(image);
		draw_boxes(image, result_vec, obj_names);
		show_console_result(result_vec, obj_names);
		//imshow("detection", image);
		//cv::waitKey(1);
		
		writer.write(image);

		frames_count++;
		gettimeofday(&stop,NULL);
		
		/*
		// update fps counter every second
		if ((1e6*(stop.tv_sec-start.tv_sec) +stop.tv_usec -start.tv_usec) > 1e6){
			//printf("fps: %d %ld\n", frames_count, stop.tv_sec);
			gettimeofday(&start,NULL);
			frames_count= 0;
			}
		*/
		}
		
	cap.release();
	writer.release();
		
	return 0;
	}	


