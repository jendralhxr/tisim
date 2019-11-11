#define DMK273

#ifdef DFK287
// image properties
#define FRAME_HEIGHT 480
#define FRAME_WIDTH 640
#define FRAME_SIZE 307200 // 640x480
#endif

#ifdef DMK273
#define FRAME_HEIGHT 1080
#define FRAME_WIDTH 1440
#define FRAME_SIZE 1555200
#endif

// camera setup
#define GAIN 10 // not really doing anything
#define FPS 6000 // maximum fps
#define EXPOSURE 2000 // us
#define FPS 600 // maximum fps
#define EXPOSURE 1500 // us
#define BRIGHTNESS 200

// UDP parameters
#define FRAME_INTERVAL (1000/30)
#define PACK_SIZE 4096 //udp pack size; note that OSX limits < 8100 bytes
#define ENCODE_QUALITY 90 // JPEG
#define BUF_LEN 65540 // Larger than maximum UDP packet size
#define INFO_LEN 256

