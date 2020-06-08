/*
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
} imagepacket, *imagepacket_ptr;

struct dataset_object{
	unsigned char object_num; // 1 to 10
	float centroid_x;
	float centroid_y;
	float freq[FREQ_BIN];
} object[OBJECT_COUNT_MAX], *object_ptr;

char *helper_ptr;

void* processNetworkisplay(void *arg){
	struct timespec timestamp;
	int *port;
	port= (int*) arg;
	printf("started on port %d\n",*port);
	unsigned int len;
	long netbuf[BUF_LEN];
	int sequence, num;
	
	struct sockaddr_in si_me, si_other;
    int s, i;
    unsigned int slen=sizeof(si_other);
    char buf[BUF_LEN];
  
    if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1) perror("socket");
    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(*port);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr *) &si_me, sizeof(si_me))==-1) perror("bind");
    
    
    while (1){
		repeat:
		len = recvfrom(s, buf, BUF_LEN, 0, (struct sockaddr *) &si_other, &slen);
		printf("from %s:%d len, %d byte %d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port), len, sizeof(long int)*2);
		switch (len){
			case (sizeof(long int)*3): goto timeheader; break;
			case (sizeof(long int)*2): goto sequence; break;
			default: goto repeat; break;
			}
		
		timeheader: // receiver timestamp
		memmove(netbuf, buf, sizeof(sizeof(long int)*3));
		timestamp.tv_sec= netbuf[1];
		timestamp.tv_nsec= netbuf[2];
		//printf("timestamp: %ld %ld\n", timestamp.tv_sec, timestamp.tv_nsec);
		goto repeat;
		
		sequence:
		memmove(netbuf, buf, sizeof(sizeof(long int)*2));
		sequence= netbuf[1];
		//printf("image seq: %d\n", sequence);
    	if (netbuf[0]==0) goto image_seq;
		else {
			num= netbuf[0];
			goto object_seq;
			}
		
		image_seq:
		imagepacket_ptr = &imagepacket;
		helper_ptr= (char*) imagepacket_ptr;
		len = recvfrom(s, buf, BUF_LEN, 0, (struct sockaddr *) &si_other, &slen);
		mempcpy(& helper_ptr[sequence*PACK_SIZE], buf, PACK_SIZE);
		//printf("image seq: %d\n", sequence);
    	goto repeat;
		
		object_seq:
		object_ptr = &(object[num]);
		helper_ptr= (char*) object_ptr;
		len = recvfrom(s, buf, BUF_LEN, 0, (struct sockaddr *) &si_other, &slen);
		mempcpy(& helper_ptr[sequence*PACK_SIZE], buf, PACK_SIZE);
		printf("object seq: %d\n", sequence);
		goto repeat;
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
