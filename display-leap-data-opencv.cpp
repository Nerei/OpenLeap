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
map<uint32_t, frame_t*> frame;
vector<uint32_t> pending;
stack<frame_t *> recycling;
frame_t *current = NULL;

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

  frame_t* f = NULL;

  for(vector<uint32_t>::const_iterator it = pending.begin(); it!=pending.end();it++)
	  if ((uint32_t)(*it) == dwPresentationTime)
	  {
		  f = frame[dwPresentationTime];
		  break;
	  }
  if (f == NULL)
  {
      if (!recycling.empty()) {
	    f = recycling.top();
	    recycling.pop();
        f->data_len = 0;
        f->state = 0;
      } else {
	    f = (frame_t *)malloc(sizeof(frame_t));
	    memset(f, 0, sizeof(frame_t));
        f->frame = cvCreateImage(cvs, IPL_DEPTH_8U, 3);
      }
      frame[dwPresentationTime] = f;
      pending.push_back(dwPresentationTime);
      sort(pending.begin(),pending.end());
      f->id = dwPresentationTime;
  }
  
  //printf("frame time: %u\n", dwPresentationTime);

  for (x=0,y=0,i=bHeaderLen; i < size; i += 2) {
    if (f->data_len >= VFRAME_SIZE)
    {
      break;
    }
    x = f->data_len % VFRAME_WIDTH;
    y = (int)floor((1.0f * f->data_len) / (1.0f * VFRAME_WIDTH));
    setTwoPixels(x,y,0,data[i],data[i])
    setTwoPixels(x+VFRAME_WIDTH,y,data[i+1],0,0)
    f->data_len++;
  }

  if (bmHeaderInfo & UVC_STREAM_EOF) {
    //printf("End-of-Frame.  Got %i\n", f->data_len);
    if (f->data_len != VFRAME_SIZE) {
      printf("wrong frame size got %i expected %i\n", f->data_len, VFRAME_SIZE);
      recycling.push(f);
      frame.erase(f->id);
      pending.erase(remove(pending.begin(), pending.end(), f->id), pending.end());
      return ;
    }

    if (current!=NULL) {
      recycling.push(current);
    }
    current = frame[dwPresentationTime];
    pending.erase(remove(pending.begin(), pending.end(), dwPresentationTime), pending.end());
    frame.erase(dwPresentationTime);
    sort(pending.begin(),pending.end());
    if (pending.size() > 10) {
        printf("Oh noez! There are %d pending frames in the queue!\n",(int)pending.size());
        for(vector<uint32_t>::const_iterator it = pending.begin(); it!=pending.begin()+5;it++)
        {
          recycling.push(frame[*it]);
          frame.erase(*it);
        }
        reverse(pending.begin(), pending.end());
        pending.resize(5);
        reverse(pending.begin(), pending.end());
    }

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
  for(std::vector<uint32_t>::const_iterator it = pending.begin(); it != pending.end(); it++)
  {
	frame_t *tmp = frame[*it];
	cvReleaseImage(&tmp->frame);
	frame.erase((uint32_t)*it);
	free(tmp);
  }
  while(!recycling.empty()) {
    frame_t *tmp = recycling.top();
    recycling.pop();
    cvReleaseImage(&tmp->frame);
    free(tmp);
  }

  return (0);
}
