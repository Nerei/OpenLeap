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
#include <string>
#include <map>
#include <vector>

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

void setDataCallback(boost::function<void(camdata_t*)>);
void init();
void spin();
void shutdown();
#endif
