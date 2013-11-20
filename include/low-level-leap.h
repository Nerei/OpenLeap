#ifndef __LEAPWNAGE
/**************************************************
*  ERIC MCCANN WAS HERE AND TURNED THIS INSIDE OUT.
*  IT WAS ORIGINALY NiSimpleSkeleton.cpp FROM PRIMESENSE SAMPLES
*  now it's abstracted in a class, and runs in parallel with the opengl mainloop
**************************************************/
#define __LEAPWNAGE
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdarg.h>
#include <string>
#include <map>
#include <vector>
#include <queue>
#include <stack>

#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/function.hpp>
#include <iostream>

#include <libusb.h>

#ifdef NDEBUG
#define debug_printf(...)
#else
#define debug_printf(...) fprintf(stderr, __VA_ARGS__)
#endif

#define LEAP_VID 0xf182
#define LEAP_PID 0x0003

#define VFRAME_WIDTH  640
#define VFRAME_HEIGHT 240
#define VFRAME_SIZE   (VFRAME_WIDTH * VFRAME_HEIGHT)
#define VFRAME_INTERLEAVED_SIZE (2*VFRAME_SIZE)

#define UVC_STREAM_EOF                                  (1 << 1)

typedef struct camdata_s camdata_t;
struct camdata_s
{
  uint8_t left[VFRAME_SIZE];
  uint8_t right[VFRAME_SIZE];
  uint8_t interleaved[VFRAME_INTERLEAVED_SIZE];
};

namespace _leap
{
  typedef struct _ctx_s _ctx_t;
  struct _ctx_s
  {
    libusb_context       *libusb_ctx;
    libusb_device_handle *dev_handle;
    int quit;
  };
  typedef struct frame_s frame_t;
  struct frame_s
  {
    camdata_t data;
    uint32_t id;
    uint32_t state;
    uint32_t interleaved_pos;
    uint32_t left_pos;
    uint32_t right_pos;
  };
}

namespace leap
{
  class driver
  {
    private:
      boost::function<void(camdata_t*)> dataCallback;
      _leap::frame_t *current;
      _leap::_ctx_t _ctx_data;
      _leap::_ctx_t *_ctx;
      unsigned char data[16384];   
    public:
      driver(boost::function<void(camdata_t*)>);
      void spin();
      void shutdown();
    private:
      void init(); 
  };
}
#endif
