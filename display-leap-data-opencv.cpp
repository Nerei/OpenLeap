/*
 ** Author: Elina Lijouvni, Eric McCann
 ** License: GPL v3
 */

#include "low-level-leap.h"
#include <cv.h>
#include <highgui.h>

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

const CvSize cvs = cvSize(VFRAME_WIDTH * 2, 2 * VFRAME_HEIGHT);

IplImage *img;

void process_video_frame()
{
  int key;

  cvShowImage("mainWin", img);
  key = cvWaitKey(1);
  if (key == 'q' || key == 0x1B)
    shutdown();
}

void gotData(camdata_t *data)
{
  int pos=0;
  for(int y=0;y<VFRAME_HEIGHT;y++)
  {
    for(int tworows=0;tworows<2;tworows++)
      for (int x=0; x<VFRAME_WIDTH*2; x++)
      {
         if (x < VFRAME_WIDTH)
         {
            img->imageData[pos++] = data->left[VFRAME_WIDTH * y + (x%VFRAME_WIDTH)]; 
            img->imageData[pos++] = data->left[VFRAME_WIDTH * y + (x%VFRAME_WIDTH)];
            img->imageData[pos++] = data->left[VFRAME_WIDTH * y + (x%VFRAME_WIDTH)];
         }
         else
         {
            img->imageData[pos++] = data->right[VFRAME_WIDTH * y + (x%VFRAME_WIDTH)]; 
            img->imageData[pos++] = data->right[VFRAME_WIDTH * y + (x%VFRAME_WIDTH)];
            img->imageData[pos++] = data->right[VFRAME_WIDTH * y + (x%VFRAME_WIDTH)];
         }
      }
  }
  process_video_frame();
}

int main(int argc, char *argv[])
{
  cvNamedWindow("mainWin", 0);
  cvResizeWindow("mainWin", 2 * VFRAME_WIDTH, 2 * VFRAME_HEIGHT);
  img = cvCreateImage(cvs, IPL_DEPTH_8U, 3);
  init();
  setDataCallback(&gotData);
  spin();
  cvReleaseImage(&img);
  return (0);
}
