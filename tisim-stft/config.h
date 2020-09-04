// DFK287
#define FRAME_HEIGHT 480
#define FRAME_WIDTH 640
#define FRAME_SIZE 307200 // 640x480
#define FRAME_RATE 480
#define FREQ_BIN 32
#define OBJECT_COUNT_MAX 10

/*
// DMK273
#define FRAME_HEIGHT 1080
#define FRAME_WIDTH 1440
#define FRAME_SIZE 1555200
*/

// camera setup
#define GAIN 20 // not really doing anything
#define FPS 480 // maximum fps
#define EXPOSURE 1000 // us
#define BRIGHTNESS 200
#define HUE_RED 60
#define HUE_GREEN 50
#define HUE_BLUE 90


// UDP parameters
#define FRAME_INTERVAL (1000/30)
#define PACK_SIZE 4096 //udp pack size; note that OSX limits < 8100 bytes
#define ENCODE_QUALITY 90 // JPEG
#define BUF_LEN 65540 // Larger than maximum UDP packet size

