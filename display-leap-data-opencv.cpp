/*
 ** Author: Elina Lijouvni, Eric McCann
 ** License: GPL v3
 */

#include "low-level-leap.h"
#include <cv.h>
#include <highgui.h>
#include <map>
#include <queue>
#include <stack>

using namespace std;

#define NUM_BUFFERS 2

#define getIndex(x,y,ch) 3 * (x + 2 * VFRAME_WIDTH * y) + ch

#define setChannel(x,y,ch,co) f->frame->imageData[getIndex(x,y,ch)] = (unsigned char)co;

#define setPixel(x,y,cr,cg,cb) \
    setChannel(x,y,0,cb) \
    setChannel(x,y,1,cg) \
    setChannel(x,y,2,cr)

#define setTwoPixels(x,y,cr,cg,cb) \
    setPixel(x,2*y,cr,cg,cb) \
    setPixel(x,2*y+1,cr,cg,cb)

typedef struct ctx_s ctx_t;
struct ctx_s
{
  int quit;
};

typedef struct frame_s frame_t;
struct frame_s
{
  IplImage* frame;
  uint32_t id;
  uint32_t data_len;
  uint32_t state;
};

ctx_t ctx_data;
ctx_t *ctx = NULL;
frame_t *current = NULL, *next = NULL;

const CvSize cvs = cvSize(VFRAME_WIDTH * 2, 2 * VFRAME_HEIGHT);

void process_video_frame(ctx_t *ctx)
{
  int key;

  cvShowImage("mainWin", current->frame );
  key = cvWaitKey(1);
  if (key == 'q' || key == 0x1B)
    ctx->quit = 1;
}

void process_usb_frame(ctx_t *ctx, unsigned char *data, int size)
{
  int i,x,y;

  int bHeaderLen = data[0];
  int bmHeaderInfo = data[1];

  uint32_t dwPresentationTime = *( (uint32_t *) &data[2] );

  if (current == NULL)
  {
	    current = (frame_t *)malloc(sizeof(frame_t));
	    memset(current, 0, sizeof(frame_t));
      current->frame = cvCreateImage(cvs, IPL_DEPTH_8U, 3);
      current->id = dwPresentationTime;
  }
  if (next == NULL)
  {
      next = (frame_t *)malloc(sizeof(frame_t));
	    memset(next, 0, sizeof(frame_t));
      next->frame = cvCreateImage(cvs, IPL_DEPTH_8U, 3);
      next->id = dwPresentationTime;
  }
  if (next->id != dwPresentationTime)
  {
      next->id = dwPresentationTime;
      next->data_len = 0;
  }
  frame_t* f = next; //lazy reimplementation of more simple image handling
  
  //printf("frame time: %u\n", dwPresentationTime);

  for (x=0,y=0,i=bHeaderLen; i < size; i += 2) {
    x = f->data_len % VFRAME_WIDTH;
    y = (int)floor((1.0f * f->data_len) / (1.0f * VFRAME_WIDTH));
    setTwoPixels(x,y,0,data[i],data[i])
    setTwoPixels(x+VFRAME_WIDTH,y,data[i+1],0,0)
    if (++f->data_len > VFRAME_SIZE)
    {
      break;
    }
  }

  if (bmHeaderInfo & UVC_STREAM_EOF && f->data_len == VFRAME_SIZE) {
    frame_t *tmp = current; //swap buffers
    current = next;
    next = tmp;

    process_video_frame(ctx);
  }
}

void gotData(unsigned char* data, int usb_frame_size)
{
  process_usb_frame(ctx, data, usb_frame_size);
}

int main(int argc, char *argv[])
{
  memset(&ctx_data, 0, sizeof (ctx_data));
  ctx = &ctx_data;
  cvNamedWindow("mainWin", 0);
  cvResizeWindow("mainWin", 2 * VFRAME_WIDTH, 2 * VFRAME_HEIGHT);
  init();
  setDataCallback(&gotData);
  spin();
  if (current)
  {
    cvReleaseImage(&current->frame);
    free(current);
  }

  return (0);
}
