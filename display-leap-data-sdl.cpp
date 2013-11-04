/*
 ** Author: Elina Lijouvni, Eric McCann
 ** License: GPL v3
 */
#include "low-level-leap.h"
#include <SDL.h>

typedef struct ctx_s ctx_t;
struct ctx_s
{
  SDL_Surface *screen;
};

typedef struct frame_s frame_t;
struct frame_s
{
  unsigned char left[ VFRAME_SIZE ];
  unsigned char right[ VFRAME_SIZE ];
  uint32_t id;
  uint32_t data_len;
  uint32_t state;
};

extern unsigned char* data;
ctx_t ctx_data;
ctx_t *ctx;
frame_t frame;

void SDL_drawpixel(SDL_Surface *screen, int x, int y, Uint8 R, Uint8 G, Uint8 B)
{
  Uint32 color = SDL_MapRGB(screen->format, R, G, B);

  switch (screen->format->BytesPerPixel) {
  case 1: { /* Assuming 8-bpp */
    Uint8 *bufp;

    bufp = (Uint8 *)screen->pixels + y*screen->pitch + x;
    *bufp = color;
  }
    break;

  case 2: { /* Probably 15-bpp or 16-bpp */
    Uint16 *bufp;

    bufp = (Uint16 *)screen->pixels + y*screen->pitch/2 + x;
    *bufp = color;
  }
    break;

  case 3: { /* Slow 24-bpp mode, usually not used */
    Uint8 *bufp;

    bufp = (Uint8 *)screen->pixels + y*screen->pitch + x;
    *(bufp+screen->format->Rshift/8) = R;
    *(bufp+screen->format->Gshift/8) = G;
    *(bufp+screen->format->Bshift/8) = B;
  }
    break;

  case 4: { /* Probably 32-bpp */
    Uint32 *bufp;

    bufp = (Uint32 *)screen->pixels + y*screen->pitch/4 + x;
    *bufp = color;
  }
    break;
  }
}

void SDL_addpixel(SDL_Surface *screen, int x, int y, Uint8 R, Uint8 G, Uint8 B)
{
  Uint32 color = SDL_MapRGB(screen->format, R, G, B);

  switch (screen->format->BytesPerPixel) {
  case 1: { /* Assuming 8-bpp */
    Uint8 *bufp;

    bufp = (Uint8 *)screen->pixels + y*screen->pitch + x;
    *bufp += color;
  }
    break;

  case 2: { /* Probably 15-bpp or 16-bpp */
    Uint16 *bufp;

    bufp = (Uint16 *)screen->pixels + y*screen->pitch/2 + x;
    *bufp += color;
  }
    break;

  case 3: { /* Slow 24-bpp mode, usually not used */
    Uint8 *bufp;

    bufp = (Uint8 *)screen->pixels + y*screen->pitch + x;
    *(bufp+screen->format->Rshift/8) += R;
    *(bufp+screen->format->Gshift/8) += G;
    *(bufp+screen->format->Bshift/8) += B;
  }
    break;

  case 4: { /* Probably 32-bpp */
    Uint32 *bufp;

    bufp = (Uint32 *)screen->pixels + y*screen->pitch/4 + x;
    *bufp += color;
  }
    break;
  }
}

static void
process_video_frame(ctx_t *ctx, frame_t *frame)
{
  int x, y;
  SDL_Surface *screen = ctx->screen;

  if ( SDL_MUSTLOCK(screen) ) {
    if ( SDL_LockSurface(screen) < 0 ) {
      return;
    }
  }

  for (y = 0; y < (VFRAME_HEIGHT * 2); y++) {
    for (x = 0; x < VFRAME_WIDTH; x++) {
      SDL_drawpixel(screen, x, y, frame->left[(y/2) * 640 + x], 0, 0);
      SDL_addpixel(screen, x, y, 0, frame->right[(y/2) * 640 + x], 0);
    }
  }

  if ( SDL_MUSTLOCK(screen) ) {
    SDL_UnlockSurface(screen);
  }
  SDL_UpdateRect(screen, 0, 0, 640, 480);
}

static void
process_usb_frame(ctx_t *ctx, frame_t *frame, unsigned char *data, int size)
{
  int i;

  int bHeaderLen = data[0];
  int bmHeaderInfo = data[1];

  uint32_t dwPresentationTime = *( (uint32_t *) &data[2] );
  //printf("frame time: %u\n", dwPresentationTime);

  if (frame->id == 0)
    frame->id = dwPresentationTime;

  for (i = bHeaderLen; i < size ; i += 2) {
    if (frame->data_len >= VFRAME_SIZE)
      break ;
    frame->left[  frame->data_len ] = data[i];
    frame->right[ frame->data_len ] = data[i+1];
    frame->data_len++;
  }

  if (bmHeaderInfo & UVC_STREAM_EOF) {
    //printf("End-of-Frame.  Got %i\n", frame->data_len);

    if (frame->data_len != VFRAME_SIZE) {
      //printf("wrong frame size got %i expected %i\n", frame->data_len, VFRAME_SIZE);
      frame->data_len = 0;
      frame->id = 0;
      return ;
    }

    process_video_frame(ctx, frame);
    frame->data_len = 0;
    frame->id = 0;
  }
  else {
    if (dwPresentationTime != frame->id && frame->id > 0) {
      //printf("mixed frame TS: dropping frame\n");
      frame->id = dwPresentationTime;
      /* frame->data_len = 0; */
      /* frame->id = 0; */
      /* return ; */
    }
  }
}

void gotData(int usb_frame_size)
{
  process_usb_frame(ctx, &frame, data, usb_frame_size);
}

int
main(int argc, char *argv[])
{
  memset(&ctx_data, 0, sizeof (ctx_data));
  ctx = &ctx_data;

  if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
    fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
    exit(1);
  }
  atexit(SDL_Quit);

  ctx->screen = SDL_SetVideoMode(640, 480, 16, SDL_SWSURFACE);
  if ( ctx->screen == NULL ) {
    fprintf(stderr, "Unable to set 640x480 video: %s\n", SDL_GetError());
    exit(EXIT_FAILURE);
  }

  init();
  setDataCallback(&gotData);
  spin();

  return (0);
}
