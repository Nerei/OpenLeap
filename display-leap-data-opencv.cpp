/*
 ** Author: Elina Lijouvni, Eric McCann
 ** License: GPL v3
 */

#include "low-level-leap.h"
#include <cv.h>
#include <highgui.h>
#include <map>
#include <queue>

using namespace std;

#define NUM_BUFFERS 2

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
frame_t *current = NULL;
uint32_t oldest = 0;

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
  int i;

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
	  f = (frame_t *)malloc(sizeof(frame_t));
	  memset(f, 0, sizeof(frame_t));
	  f->frame = cvCreateImage( cvSize(VFRAME_WIDTH, 2 * VFRAME_HEIGHT), IPL_DEPTH_8U, 3);
	  frame[dwPresentationTime] = f;
	  pending.push_back(dwPresentationTime);
	  sort(pending.begin(),pending.end());	  
      f->id = dwPresentationTime;
  }
  
  //printf("frame time: %u\n", dwPresentationTime);

  for (i = bHeaderLen; i < size ; i += 2) {
    if (f->data_len >= VFRAME_SIZE)
    {
      cvReleaseImage(&f->frame);
      frame.erase(f->id);
      pending.erase(remove(pending.begin(), pending.end(), f->id), pending.end());
      free(f);
      break ;
    }

    CvScalar s;
    s.val[2] = data[i];
    s.val[1] = data[i+1];
    s.val[0] = 0;
    int x = f->data_len % VFRAME_WIDTH;
    int y = (int)floor((1.0f * f->data_len) / (1.0f * VFRAME_WIDTH));
    cvSet2D(f->frame, 2 * y,     x, s);
    cvSet2D(f->frame, 2 * y + 1, x, s);
    f->data_len++;
  }

  if (dwPresentationTime != f->id && f->id > 0) {
    printf("mixed frame TS: (id=%i, dwPresentationTime=%i) -- dropping frame\n", f->id, dwPresentationTime);
    cvReleaseImage(&f->frame);
    frame.erase(f->id);
    pending.erase(remove(pending.begin(), pending.end(), f->id), pending.end());
    free(f);
    return ;
  }
  if (bmHeaderInfo & UVC_STREAM_EOF) {
    //printf("End-of-Frame.  Got %i\n", f->data_len);
    if (f->data_len != VFRAME_SIZE) {
      printf("wrong frame size got %i expected %i\n", f->data_len, VFRAME_SIZE);
      cvReleaseImage(&f->frame);
      frame.erase(f->id);
      pending.erase(remove(pending.begin(), pending.end(), f->id), pending.end());
      free(f);
      return ;
    }

    if (current!=NULL) {
    	cvReleaseImage(&current->frame);
    	free(current);
    }
    current = frame[dwPresentationTime];
    pending.erase(remove(pending.begin(), pending.end(), dwPresentationTime), pending.end());
    frame.erase(dwPresentationTime);
    sort(pending.begin(),pending.end());
    if (pending.size() > 10) {
        printf("Oh noez! There are %d pending frames in the queue!\n",(int)pending.size());
/*        for(vector<uint32_t>::const_iterator it = pending.begin(); it!=pending.begin()+5;it++)
        {
            cvReleaseImage(&frame[*it]->frame);
            free(frame[*it]);
            frame.erase(*it);
        }
        pending.erase(pending.begin(), pending.begin()+5);*/
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
  cvResizeWindow("mainWin", VFRAME_WIDTH, VFRAME_HEIGHT * 2);
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

  return (0);
}
