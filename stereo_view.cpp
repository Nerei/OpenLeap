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
 #include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

typedef struct stereoframe_s stereoframe_t; 
struct stereoframe_s{
    IplImage * left;
    IplImage * right;
    CvMat *cv_image_depth;
    IplImage *cv_image_depth_aux;
    CvStereoBMState *stereo_state; /* Block Matching State */

};

stereoframe_t stereo;

const CvSize cvs = cvSize(VFRAME_WIDTH, VFRAME_HEIGHT);

void setPixels(IplImage *currentFrame, int x, int y, int data) {
    int position = (x + VFRAME_WIDTH  * y) * 3;
    currentFrame->imageData[position] = data;
    currentFrame->imageData[position + 1] = data;
    currentFrame->imageData[position + 2] = data;
}

void init_stereo_frame(stereoframe_t *frame){
    memset(frame,0,sizeof(stereoframe_t));
    frame->stereo_state =  cvCreateStereoBMState(CV_STEREO_BM_BASIC, 64);
    frame->cv_image_depth = cvCreateMat (  VFRAME_HEIGHT,VFRAME_WIDTH, CV_16S);
    frame->cv_image_depth_aux = cvCreateImage (cvs, IPL_DEPTH_8U, 1);
    frame->left = cvCreateImage (cvs, IPL_DEPTH_8U, 3);
    frame->right = cvCreateImage (cvs, IPL_DEPTH_8U, 3);

    frame->stereo_state->preFilterSize      = 5;
    frame->stereo_state->preFilterCap       = 61;
    frame->stereo_state->SADWindowSize      = 9;
    frame->stereo_state->minDisparity       = -39;
    frame->stereo_state->numberOfDisparities    = 112;
    frame->stereo_state->textureThreshold   = 0;
    frame->stereo_state->uniquenessRatio    = 10;
    frame->stereo_state->speckleWindowSize  = 0;
    frame->stereo_state->speckleRange       = 8;
    frame->stereo_state->textureThreshold   = 507;
    frame->stereo_state->disp12MaxDiff      = 1;
}

static void computeStereoBM ( stereoframe_t *data ) {
    int i, j, aux;
    uchar *ptr_dst;

    IplImage * newLeft = cvCreateImage (cvs, IPL_DEPTH_8U, 1);
    cvCvtColor(data->left, newLeft, CV_RGB2GRAY);
    IplImage * newRight = cvCreateImage (cvs, IPL_DEPTH_8U, 1);
    cvCvtColor(data->right, newRight, CV_RGB2GRAY);

    cvFindStereoCorrespondenceBM (
        newLeft,
        newRight,
        data->cv_image_depth,
        data->stereo_state
    );

    //Normalize the result so we can display it
    cvNormalize( data->cv_image_depth, data->cv_image_depth, 0, 256, CV_MINMAX, NULL );
    for ( i = 0; i < data->cv_image_depth->rows; i++)
    {
        aux = data->cv_image_depth->cols * i;
        ptr_dst = (uchar*)(data->cv_image_depth_aux->imageData + i*data->cv_image_depth_aux->widthStep);
        for ( j = 0; j < data->cv_image_depth->cols; j++ )
        {
            //((float*)(mat->data.ptr + mat->step*i))[j]
            ptr_dst[j] = (uchar)((short int*)(data->cv_image_depth->data.ptr + data->cv_image_depth->step*i))[j];
            ptr_dst[j+1] = (uchar)((short int*)(data->cv_image_depth->data.ptr + data->cv_image_depth->step*i))[j];
            ptr_dst[j+2] = (uchar)((short int*)(data->cv_image_depth->data.ptr + data->cv_image_depth->step*i))[j];
        }
    }
}

void process_video_frame() {
    int key;

    // cvShowImage(DEPTH_WINDOW, frame);
    IplImage * newDepth = cvCreateImage (cvs, IPL_DEPTH_8U, 1);
    cvConvert(stereo.cv_image_depth, newDepth);

    cvShowImage("Depth Image", newDepth );
    cvShowImage("Left Image", stereo.left );
    cvShowImage("Right Image", stereo.right );
    // cvShowImage("mainWin", current->frame );
    // imshow("mainWin", *(stereo.cv_image_depth));
    
    key = cvWaitKey(1);

    if (key > 0) {

        printf("key: %d\n", key);


        switch(key) {

            case 1048689: // q
                shutdown();
                break;

            case 1048695: // w
                printf("preFilterSize: %d\n", stereo.stereo_state->preFilterSize);
                if (stereo.stereo_state->preFilterSize < 255)
                    stereo.stereo_state->preFilterSize += 2;
                break;

            case 1048691: // s
                printf("preFilterSize: %d\n", stereo.stereo_state->preFilterSize);
                if (stereo.stereo_state->preFilterSize > 5)
                    stereo.stereo_state->preFilterSize -= 2;
                break;

            case 1048677: // e
                printf("preFilterCap: %d\n", stereo.stereo_state->preFilterCap);
                if (stereo.stereo_state->preFilterCap < 63)
                    stereo.stereo_state->preFilterCap++;
                break;

            case 1048676: // d
                printf("preFilterCap: %d\n", stereo.stereo_state->preFilterCap);
                if (stereo.stereo_state->preFilterCap > 1)
                    stereo.stereo_state->preFilterCap--;
                break;

            case 1048690: // r
                printf("SADWindowSize: %d\n", stereo.stereo_state->SADWindowSize);
                if (stereo.stereo_state->SADWindowSize < 255)
                    stereo.stereo_state->SADWindowSize += 2;
                break;

            case 1048678: // f
                printf("SADWindowSize: %d\n", stereo.stereo_state->SADWindowSize);
                if (stereo.stereo_state->SADWindowSize > 5)
                    stereo.stereo_state->SADWindowSize -= 2;
                break;

            case 1048692: // t
                printf("minDisparity: %d\n", stereo.stereo_state->minDisparity);
                stereo.stereo_state->minDisparity += 1;
                break;

            case 1048679: // g
                printf("minDisparity: %d\n", stereo.stereo_state->minDisparity);
                stereo.stereo_state->minDisparity -= 1;
                break;
            case 1113938: // up
                printf("numberOfDisparities: %d\n", stereo.stereo_state->numberOfDisparities);
                    stereo.stereo_state->numberOfDisparities += 16;
                break;
            case 1113940: // down
                printf("numberOfDisparities: %d\n", stereo.stereo_state->numberOfDisparities);
                if (stereo.stereo_state->numberOfDisparities > 16)
                    stereo.stereo_state->numberOfDisparities -= 16;
                break;
        }
    }
}

void gotData(camdata_t *data) {
    int i, x, y;

    for (x = 0, y = 0, i = 0; i < VFRAME_SIZE; x = i % VFRAME_WIDTH,y=(int) floor((1.0f * i) / (1.0f * VFRAME_WIDTH)),i++) {
        setPixels(stereo.left, x, y, data->left[i]);
        setPixels(stereo.right, x, y, data->right[i]);
    }
    computeStereoBM(&stereo);
    process_video_frame();
}

int main(int argc, char *argv[])
{   
    namedWindow("Depth Image");
    namedWindow("Left Image");
    namedWindow("Right Image");

    init_stereo_frame(&stereo);

    init();
    
    setDataCallback(&gotData);
    
    spin();

    return (0);
}
