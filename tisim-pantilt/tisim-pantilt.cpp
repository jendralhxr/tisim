/*
 * $ g++ susa.cpp  `pkg-config opencv --libs` -lspi_adc  -lpthread
 * $ sudo ./a.out /dev/video1 FRAMERATE EXPOSURE MOTORGAIN
 * */

#include <iostream>
#include <math.h>
#include <opencv2/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "config.h"
#include "gpio.h"
#include "spi_ad.h"
#include "v4ldevice.cpp"

using namespace cv;
using namespace std;

pthread_t thread_image;
pthread_t thread_fps;
pthread_t thread_pantilt;
pthread_t thread_centroid;

 
typedef float TYPE;
char command[256]; // for parameter setting from shell
bool fps;
int ret = 1;
double pos_volt[2];

unsigned char* ImageBuffer = NULL;
unsigned char val_max;
int val_max_x, val_max_y;
static	spi_setting		dac;
static	spi_setting		adc;
uint16_t	data;
double voltage_incr;
unsigned int cycle=0;
	
Mat raw_frame(FRAME_HEIGHT, FRAME_WIDTH, CV_8UC1,Scalar(0));// Grayscale

 
void* processDisplay(void *arg){
	while (1){
		imshow("camera", raw_frame);
		waitKey(1);
		}
	}

void* processFPSCounter(void *arg){
	struct timespec start, stop, timestamp;
	clock_gettime(CLOCK_MONOTONIC, &start);
	
	while(1){
		clock_gettime(CLOCK_MONOTONIC, &timestamp);
		if (timestamp.tv_sec-start.tv_sec > 2){
			// performance hit from 360 to 240 with double framerate counter
			//printf("fps: %f\n", cycle / (double) (timestamp.tv_sec-start.tv_sec + (timestamp.tv_nsec-start.tv_nsec)/1e9));
			printf("fps: %d\n", cycle /2 );
			clock_gettime(CLOCK_MONOTONIC, &start);
			cycle= 0;
			}
		}
	}
	
void* processPanTilt(void *arg){
	// home
	data = cal_digital_pm10(2.5, 0);
	pos_volt[0]= 0;
	pos_volt[1]= 0;
	ret = spi_transfer_da(dac, DAC_REG, DAC_A, data, NULL);
	ret = spi_transfer_da(dac, DAC_REG, DAC_B, data, NULL);
	
	while(1){
		// AD5752: DAC A 
		data = cal_digital_pm10(2.5, pos_volt[0]);	
		ret = spi_transfer_da(dac, DAC_REG, DAC_A, data, NULL);
		if(ret == RET_ERR)	fprintf(stderr, "Error: set DAC_A to 0x%04X\n", data);
		// fprintf(stdout, "DAC_A-Analog output: %gV (0x%04X)\n", pos_volt[0], data);

		// AD5752: DAC B
		data = cal_digital_pm10(2.5, pos_volt[1]);
		ret = spi_transfer_da(dac, DAC_REG, DAC_B, data, NULL);
		if(ret == RET_ERR)	fprintf(stderr, "Error: set DAC_B to 0x%04X\n", data);
		// fprintf(stdout, "DAC_B-Analog output: %gV (0x%04X)\n", pos_volt[1], data);
		}
	}	

void* processCentroid(void *arg){
	while(1){
		// finding the brighest spot
		val_max= 0;
		for (int i=0; i<FRAME_SIZE; i++){
			//printf("%d ",raw_frame.data[i]);
			if ( raw_frame.data[i] > val_max){
				val_max= raw_frame.data[i];
				val_max_x= i % FRAME_WIDTH;
				val_max_y= i / FRAME_WIDTH;
				}
			}
				
		if (val_max_x > (FRAME_WIDTH >> 1) + 8) pos_volt[0] -= voltage_incr;
		else if (val_max_x < (FRAME_WIDTH >> 1) - 8) pos_volt[0] += voltage_incr;
		if (val_max_y > (FRAME_HEIGHT >> 1) + 8) pos_volt[1] -= voltage_incr;
		else if (val_max_y < (FRAME_HEIGHT >> 1) - 8) pos_volt[1] += voltage_incr;
		//printf("target (%d, %d) as %.4fV %.4fV\n", val_max_x, val_max_y, pos_volt[0], pos_volt[1]);
		}
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
	
	// camera initialization
	open_device(argv[1]);
	init_device(FRAME_WIDTH, FRAME_HEIGHT);
	// v4l2-ctl -d /dev/video1 --list-formats-ext
	if (setFPS(atoi(argv[2]))) printf("Framerate set to : %d\n", atoi(argv[2]));
	set_props(argv[1], atoi(argv[3]));
	start_capturing();

	// ADC and DAC initialization
	/* INITIALIZE */
	char dacDev[256], adcDev[256];
	strncpy(dacDev, "/dev/spidev3.0", sizeof(char) * 256);
	strncpy(adcDev, "/dev/spidev3.1", sizeof(char) * 256);
	dac.fd		= -1;
	dac.mode	= SPI_MODE_1;
	dac.bits	= 8;
	dac.speed	= 10000000;		// 10MHz
	adc.fd		= -1;
	adc.mode	= SPI_MODE_1;
	adc.bits	= 16;
	adc.speed	= 10000000;		// 10MHz
	/* GPIO */
	set_gpio_export();
	set_gpio_init();
	/* SPI */
	ret = spi_open(&dac, dacDev); if(ret == RET_ERR) return 1;
	ret = spi_open(&adc, adcDev); if(ret == RET_ERR) return 1;
	ret = spi_transfer_da(dac, OUT_REG, DAC_AB, OUT_PN10, NULL);				// output range ±10V
	if(ret == RET_ERR)	return(2);
	ret = spi_transfer_da(dac, POW_REG, 0x00, (POW_PU_A | POW_PU_B), NULL);		// DAC A and DAC B are in normal operating mode
	if(ret == RET_ERR)	return(2);
	ret = spi_transfer_da(dac, CTRL_REG, CTRL_LOAD, 0, NULL);					// load
	if(ret == RET_ERR)	return(2);
	
	voltage_incr= atof(argv[4]);
	
	// the threads
	int thread_handler[4];
	thread_handler[0] = pthread_create(&thread_image, NULL, &processDisplay, NULL);
	thread_handler[1] = pthread_create(&thread_pantilt, NULL, &processPanTilt, NULL);
	thread_handler[2] = pthread_create(&thread_fps, NULL, &processFPSCounter, NULL);
	thread_handler[3] = pthread_create(&thread_centroid, NULL, &processCentroid, NULL);
	
	// the routine
	while(1){
		ImageBuffer = snapFrame();
		if( ImageBuffer != NULL ){
			memmove(raw_frame.data, ImageBuffer, sizeof(char)*FRAME_SIZE);
			cycle++; // fps counter
	}
	else{
		printf("No image buffer retrieved.\n");
		break;
		}
	}
	
	destroyAllWindows() ;
	stop_capturing();
	uninit_device();
	close_device();
	spi_close(&dac);
	spi_close(&adc);
	set_gpio_unexport();
	return 0;
	}
