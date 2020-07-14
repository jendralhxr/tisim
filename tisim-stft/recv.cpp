/*
	g++ recv.cpp -lpthread `pkg-config opencv --libs` -o recvr
	./recver PORTS..
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
#include "config.h"

using namespace cv;

unsigned int object_count, threads_count;
int *thread_handler;
pthread_t *netdisplay;
int *PORTS; // open ports for senders
char *frameupdate;
	
/*ω
・Input image (VGA, JPEG compressed (unsigned char))
・Peak frequency image (VGA, JPEG compressed (unsigned char))
・gravity position (pairs 32-bit float)
・number of objects (8-bit integer)
・frequency response (arrays of 32-bit float).  
*/

struct dataset_image{
	unsigned char image_input[FRAME_SIZE];
	unsigned char image_peak[FRAME_SIZE*3];
};

struct dataset_object{
	unsigned char object_num; // 1 to 10
	float centroid_x;
	float centroid_y;
	float freq[FREQ_BIN];
};

struct sending_buffer{
	short int id; 
	short int sequence;
	char data[PACK_SIZE];
	} recv_buffer;

char *helper_ptr;

void* processNetworkisplay(void *arg){
	struct timespec timestamp, start, timestamp_recv, start_recv;
	int *port;
	port= (int*) arg;
	printf("started on port %d\n",*port);
	unsigned int len, frames_count;
	unsigned long int netbuf[BUF_LEN]; // for header
	char buf[BUF_LEN+8]; // for content
	int sequence, num, total_pack;
	
	struct sockaddr_in si_me, si_other;
    int s;
    unsigned int slen=sizeof(si_other);
    
    struct dataset_image imagepacket, *imagepacket_ptr;
    struct dataset_object object[OBJECT_COUNT_MAX], *object_ptr;
    
    if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1) perror("socket");
    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(*port);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr *) &si_me, sizeof(si_me))==-1) perror("bind");
    
    clock_gettime(CLOCK_MONOTONIC, &start_recv);
    while (1){
		len = recvfrom(s, buf, sizeof(struct sending_buffer), 0, (struct sockaddr *) &si_other, &slen);
		//printf("from %s:%d len, %d byte\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port), len);
		switch (len){
			case (32):  // timestamp header, sorted out
				memcpy(netbuf, buf, 32);
				total_pack= netbuf[0];
				timestamp.tv_sec= netbuf[1];
				timestamp.tv_nsec= netbuf[2];
				// fps counter
				clock_gettime(CLOCK_MONOTONIC, &timestamp_recv);
    			if (timestamp_recv.tv_sec-start_recv.tv_sec > 2){
					printf("port %d, fps: %f\n", *port, frames_count / (double) (timestamp_recv.tv_sec-start_recv.tv_sec + (timestamp_recv.tv_nsec-start_recv.tv_nsec)/1e9));
					frames_count= 0;
					clock_gettime(CLOCK_MONOTONIC, &start_recv);
					}
				else frames_count++;
				break;
			case (4100):  
				memcpy(&recv_buffer, buf, sizeof(struct sending_buffer));
				//printf("id %d, sequence: %d/%d\n", recv_buffer.id, recv_buffer.sequence, total_pack);
				if (recv_buffer.id==0){ // image
	//				printf("image %d/%d size:%d\n", recv_buffer.sequence, total_pack, sizeof(struct sending_buffer));
					imagepacket_ptr = &imagepacket;
					helper_ptr= (char*) imagepacket_ptr;
					mempcpy(& helper_ptr[recv_buffer.sequence*PACK_SIZE], recv_buffer.data, PACK_SIZE);
					}
				else{ // object centroid, freq, etc
					object_ptr = &(object[recv_buffer.id-1]);
					helper_ptr= (char*) object_ptr;
					mempcpy(& helper_ptr[recv_buffer.sequence*PACK_SIZE], buf, PACK_SIZE);			
			//		printf("object %d %d/%d size:%d\n", recv_buffer.id, recv_buffer.sequence, total_pack, sizeof(struct sending_buffer));
					}
				break;
			default: 
				printf("invalid length %d\n", len);
				break;
			}
		}
	}


/*
・Input image (VGA, JPEG compressed (unsigned char))
・Peak frequency image (VGA, JPEG compressed (unsigned char))
・gravity position (pairs 32-bit float)
・number of objects (8-bit integer)
・frequency response (arrays of 32-bit float).  
*/


int main(int argc, char *argv[]){
	threads_count= argc -1 ;
	thread_handler = (int*) malloc(sizeof(int) * threads_count);
	netdisplay = (pthread_t*) malloc(sizeof(pthread_t) * threads_count);
	
	PORTS = (int*) malloc(sizeof(int)*threads_count);
	frameupdate = (char*) malloc(sizeof(char)*threads_count);
	
	for (int i=0; i<threads_count; i++){
		// init network stuff
		PORTS[i]=  atoi(argv[i+1]);
		thread_handler[i] = pthread_create(&(netdisplay[i]), NULL, &processNetworkisplay, &(PORTS[i]));
		}
		
	while (1){
		
		}
		
    return 0;

}
