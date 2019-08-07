/*
 *   This program is distributed in the hope that it will be useful,
 *   I just don't bother with licenses.
 *   
 *  compile against OpenCV
 *   $	g++ tisim-recv.cpp -o tisim-recv `pkg-config opencv --libs` 
 *   $	./tisim-recv <port> <image-feed-file>
 * 
 * adapted from:
 * https://gist.github.com/Circuitsoft/1126411
 * https://github.com/chenxiaoqino/udp-image-streaming/
 */

#include "PracticalSocket.h" // For UDPSocket and SocketException
#include <stdlib.h>           // For atoi()
#include <sys/time.h>
#include <unistd.h>
#include "opencv2/opencv.hpp"
#include "config.h"

using namespace cv;
unsigned int frames_count;
struct timeval start, stop, timestamp;
int total_pack;

struct header{
	int pack_num;
	char info[INFO_LEN];
	} img_header;

int main(int argc, char * argv[]) {

    unsigned short servPort = atoi(argv[1]); // First arg:  local port

    namedWindow("recv", CV_WINDOW_AUTOSIZE);
    try {
        UDPSocket sock(servPort);

        char buffer[BUF_LEN]; // Buffer for echo string
        int recvMsgSize; // Size of received message
        string sourceAddress; // Address of datagram source
        unsigned short sourcePort; // Port of datagram source

        clock_t last_cycle = clock();


		gettimeofday(&start,NULL);
        while (1) {
            // Block until receive message from a client
            do {
                recvMsgSize = sock.recvFrom(buffer, BUF_LEN, sourceAddress, sourcePort);
            } while (recvMsgSize > sizeof(struct header));
            
            memcpy(&img_header, buffer, sizeof(struct header));
            total_pack = img_header.pack_num;
			printf("%s",img_header.info);
			
            char * longbuf = new char[PACK_SIZE * total_pack];
            for (int i = 0; i < total_pack; i++) {
                recvMsgSize = sock.recvFrom(buffer, BUF_LEN, sourceAddress, sourcePort);
                if (recvMsgSize != PACK_SIZE) {
                    cerr << "Received unexpected size pack:" << recvMsgSize << endl;
                    continue;
                }
                memcpy( & longbuf[i * PACK_SIZE], buffer, PACK_SIZE);
            }
 
            Mat rawData = Mat(1, PACK_SIZE * total_pack, CV_8UC1, longbuf);
            Mat frame = imdecode(rawData, CV_LOAD_IMAGE_COLOR);
            if (frame.size().width == 0) {
                cerr << "decode failure!" << endl;
                continue;
            }
            
            frames_count++;
			gettimeofday(&stop,NULL);
		
		
			// update fps counter every second
			if ((1e6*(stop.tv_sec-start.tv_sec) +stop.tv_usec -start.tv_usec) > 1e6){
				printf("recv fps: %d\n", frames_count);
				gettimeofday(&start,NULL);
				frames_count= 0;
				}
            
			// display image to screen
            //imshow("recv", frame);
            // waitKey(1);

            // save image to frame
            imwrite(argv[2], frame);
            usleep(50000);
            
            free(longbuf);
            

        }
    } catch (SocketException & e) {
        cerr << e.what() << endl;
        exit(1);
    }

    return 0;
}
