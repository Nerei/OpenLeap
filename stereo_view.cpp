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

typedef struct stereoframe_s stereoframe_t; 
struct stereoframe_s{
    IplImage * left;
    IplImage * right;
    CvMat *cv_image_depth;
    IplImage *cv_image_depth_aux;
    CvStereoBMState *stereo_state; /* Block Matching State */

};

ctx_t ctx_data;
ctx_t *ctx = NULL;
map<uint32_t, frame_t*> framesMap;
vector<uint32_t> pending;
stack<frame_t *> recycling;
frame_t *current = NULL;
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

void process_video_frame(ctx_t *ctx) {
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
                ctx->quit = 1;
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

void process_usb_frame(ctx_t *ctx, unsigned char *data, int size) {
    int i, x, y;

    int bHeaderLen = data[0];
    int bmHeaderInfo = data[1];

    uint32_t dwPresentationTime = *( (uint32_t *) &data[2] );

    frame_t* currentFrame = NULL;

    // Check if frame is already in the vector
    for(vector<uint32_t>::const_iterator it = pending.begin(); it!=pending.end();it++) {
        if ((uint32_t)(*it) == dwPresentationTime) {
            currentFrame = framesMap[dwPresentationTime];
            break;
        }
    }

    // If no frame was found, create one
    if (currentFrame == NULL) {
        // If there's one in the recycling bin, use it
        if (!recycling.empty()) {
            currentFrame = recycling.top();
            recycling.pop();
            currentFrame->data_len = 0;
            currentFrame->state = 0;
        } else { // Else create a new one
            currentFrame = (frame_t *) malloc(sizeof(frame_t));
            memset(currentFrame, 0, sizeof(frame_t));
            currentFrame->frame = cvCreateImage(cvs, IPL_DEPTH_8U, 3);
        }

        // Push it to the vector of pending frames
        framesMap[dwPresentationTime] = currentFrame;
        pending.push_back(dwPresentationTime);
        sort(pending.begin(),pending.end());
        currentFrame->id = dwPresentationTime;
    }

    for (x = 0, y = 0, i = bHeaderLen; i < size; i += 2) {
        if (currentFrame->data_len >= VFRAME_SIZE) {
            break;
        }

        x = currentFrame->data_len % VFRAME_WIDTH;
        y = (int) floor((1.0f * currentFrame->data_len) / (1.0f * VFRAME_WIDTH));
        // y = currentFrame->data_len / VFRAME_WIDTH;
        
        // setTwoPixels(x, y, 0, data[i], data[i])
        // setTwoPixels(currentFrame->frame, x+VFRAME_WIDTH, y, data[i+1], 0, 0)

        // setTwoPixels(stereo.left, x, y, 0, data[i], data[i]);
        setPixels(stereo.left, x, y, data[i]);
        setPixels(stereo.right, x, y, data[i]);

        currentFrame->data_len++;
    }
    // If stream frame is done
    if (bmHeaderInfo & UVC_STREAM_EOF) {
        //printf("End-of-Frame.  Got %i\n", currentFrame->data_len);

        // If frame isn't complete, push it to the recycle bin
        if (currentFrame->data_len != VFRAME_SIZE) {
            // printf("wrong frame size got %i expected %i\n", currentFrame->data_len, VFRAME_SIZE);
            recycling.push(currentFrame);
            framesMap.erase(currentFrame->id);
            pending.erase(remove(pending.begin(), pending.end(), currentFrame->id), pending.end());
            return ;
        }


        if (current != NULL) {
            recycling.push(current);
        }

        current = framesMap[dwPresentationTime];
        // current->frame = stereo.left;
        pending.erase(remove(pending.begin(), pending.end(), dwPresentationTime), pending.end());
        framesMap.erase(dwPresentationTime);
        sort(pending.begin(),pending.end());

        if (pending.size() > 10) {
            // printf("Oh noez! There are %d pending frames in the queue!\n",(int)pending.size());
            
            for(vector<uint32_t>::const_iterator it = pending.begin(); it!=pending.begin()+5; it++) {
                recycling.push(framesMap[*it]);
                framesMap.erase(*it);
            }

            reverse(pending.begin(), pending.end());
            pending.resize(5);
            reverse(pending.begin(), pending.end());
        }

        computeStereoBM(&stereo);

        // IplImage * newDepth = cvCreateImage(cvs, IPL_DEPTH_8U, 3);
        // cvCvtColor(stereo.cv_image_depth_aux, newDepth, CV_GRAY2RGB);

        process_video_frame(ctx);
        // process_video_frame(ctx, current->frame);
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
    // cvNamedWindow("mainWin", 0);
    // cvResizeWindow("mainWin", VFRAME_WIDTH, 2 * VFRAME_HEIGHT);
    
    namedWindow("Depth Image");
    namedWindow("Left Image");
    namedWindow("Right Image");

    init_stereo_frame(&stereo);

    init();
    
    setDataCallback(&gotData);
    
    spin();
    
    if (current) {
        cvReleaseImage(&current->frame);
        free(current);
    }

    for(std::vector<uint32_t>::const_iterator it = pending.begin(); it != pending.end(); it++) {
        frame_t *tmp = framesMap[*it];
        cvReleaseImage(&tmp->frame);
        framesMap.erase((uint32_t)*it);
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
