#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <string.h>
#include <winsock2.h>

#pragma comment(lib, "Ws2_32.lib")

using namespace cv;
using namespace std;

#define FRAME_HEIGHT 480
#define FRAME_WIDTH 640
#define FRAME_SIZE 307200 // 640x480
#define FRAME_RATE 480
#define FREQ_BIN 32
#define OBJECT_COUNT_MAX 10
#define FRAME_INTERVAL (1000/30)
#define PACK_SIZE 4096 //udp pack size; note that OSX limits < 8100 bytes
#define ENCODE_QUALITY 90 // JPEG
#define BUF_LEN 65540 // Larger than maximum UDP packet size

unsigned int object_count, threads_count;
int PORTS[4]; // open ports for senders

struct dataset_image {
    unsigned char image_input[FRAME_SIZE];
    unsigned char image_peak[FRAME_SIZE * 3];
};

struct dataset_object {
    unsigned char object_num; // 1 to 10
    float centroid_x;
    float centroid_y;
    float freq[FREQ_BIN];
};

struct sending_buffer {
    short int id;
    short int sequence;
    char data[PACK_SIZE];
} recv_buffer;

char* helper_ptr;

DWORD WINAPI processNetworkDisplay(void* param) {
    //void* processNetworkisplay(void* arg) {
    struct timespec timestamp;
    int* porta;
    porta = (int*)param;
    printf("started on port %d\n", *porta);
    unsigned long int netbuf[BUF_LEN]; // for header
    char buf[BUF_LEN + 8]; // for content
    int total_pack;

    struct dataset_image imagepacket, * imagepacket_ptr;
    struct dataset_object object[OBJECT_COUNT_MAX], * object_ptr;

    WSADATA       wsaData;
    SOCKET        ReceivingSocket;
    SOCKADDR_IN   ReceiverAddr;
    int           Port = *porta;
    char          ReceiveBuf[BUF_LEN];
    int           BufLength = BUF_LEN;
    SOCKADDR_IN   SenderAddr;
    int           SenderAddrSize = sizeof(SenderAddr);
    int           ByteReceived = 5;

    // socket startup routines
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    ReceivingSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    while (1) {
        //len = recvfrom(s, buf, sizeof(struct sending_buffer), 0, (struct sockaddr*)&si_other, &slen);
        ByteReceived = recvfrom(ReceivingSocket, ReceiveBuf, BufLength, 0, (SOCKADDR*)&SenderAddr, &SenderAddrSize);

        switch (ByteReceived) {
        case (32):  // timestamp header, sorted out
            memcpy(netbuf, buf, 32);
            total_pack = netbuf[0];
            timestamp.tv_sec = netbuf[1];
            timestamp.tv_nsec = netbuf[2];
            // fps counter
            break;
        case (4100):
            memcpy(&recv_buffer, buf, sizeof(struct sending_buffer));
            //printf("id %d, sequence: %d/%d\n", recv_buffer.id, recv_buffer.sequence, total_pack);
            if (recv_buffer.id == 0) { // image
//				printf("image %d/%d size:%d\n", recv_buffer.sequence, total_pack, sizeof(struct sending_buffer));
                imagepacket_ptr = &imagepacket;
                helper_ptr = (char*)imagepacket_ptr;
                memcpy(&helper_ptr[recv_buffer.sequence * PACK_SIZE], recv_buffer.data, PACK_SIZE);
            }
            else { // object centroid, freq, etc
                object_ptr = &(object[recv_buffer.id - 1]);
                helper_ptr = (char*)object_ptr;
                memcpy(&helper_ptr[recv_buffer.sequence * PACK_SIZE], buf, PACK_SIZE);
                //		printf("object %d %d/%d size:%d\n", recv_buffer.id, recv_buffer.sequence, total_pack, sizeof(struct sending_buffer));
            }
            break;
        default:
            printf("invalid length %d\n", ByteReceived);
            break;
        }
    }

    return(0);
}

int main()
{
    PORTS[0] = 9000;
    PORTS[1] = 9001;
    PORTS[2] = 9002;
    PORTS[3] = 9003;

    for (int i = 0; i < 4; i++) {
        HANDLE thdHandle = CreateThread(NULL, 0, processNetworkDisplay, &(PORTS[i]), 0, NULL);
    }

    while (1) {}
    //getchar();
    return 0;
}