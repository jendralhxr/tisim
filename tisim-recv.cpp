/*
 *   This program is distributed in the hope that it will be useful,
 *   I just don't bother with licenses.
 *   
 *  compile against OpenCV
 *   $	g++ tisim-recv.cpp -o tisim-recv `pkg-config opencv --libs` 
 *   $	./tisim-recv <port>
 * 
 * adapted from:
 * https://gist.github.com/Circuitsoft/1126411
 * https://github.com/chenxiaoqino/udp-image-streaming/
 */

#include "PracticalSocket.h" // For UDPSocket and SocketException
#include <iostream>          // For cout and cerr
#include <cstdlib>           // For atoi()
#include <sys/time.h>
#include "opencv2/opencv.hpp"
#include "config.h"

using namespace cv;
unsigned int frames_count;
struct timeval start, stop, timestamp;

int main(int argc, char * argv[]) {

    if (argc != 2) { // Test for correct number of parameters
        cerr << "Usage: " << argv[0] << " <Server Port>" << endl;
        exit(1);
    }

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
            } while (recvMsgSize > sizeof(int));
            int total_pack = ((int * ) buffer)[0];

  //          cout << "expecting length of packs:" << total_pack << endl;
            char * longbuf = new char[PACK_SIZE * total_pack];
            for (int i = 0; i < total_pack; i++) {
                recvMsgSize = sock.recvFrom(buffer, BUF_LEN, sourceAddress, sourcePort);
                if (recvMsgSize != PACK_SIZE) {
                    cerr << "Received unexpected size pack:" << recvMsgSize << endl;
                    continue;
                }
                memcpy( & longbuf[i * PACK_SIZE], buffer, PACK_SIZE);
            }

//            cout << "Received packet from " << sourceAddress << ":" << sourcePort << endl;
 
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
            
            imshow("recv", frame);
            free(longbuf);
            waitKey(1);


        }
    } catch (SocketException & e) {
        cerr << e.what() << endl;
        exit(1);
    }

    return 0;
}
