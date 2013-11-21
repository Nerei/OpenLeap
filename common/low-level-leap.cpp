#include "low-level-leap.h"

using namespace std;
using namespace _leap;

void fprintf_data(FILE *fp, const char * title, unsigned char *_data, size_t size)
{
  unsigned int i;
 
  debug_printf("%s", title);
  for (i = 0; i < size; i++) {
    if ( ! (i % 16) )
      debug_printf("\n");
    debug_printf("%02x ", _data[i]);
  }
  debug_printf("\n");
}
std::map<int, unsigned char *> gcc48canDIAF;
unsigned char *setValAndGetAddr(int l, ...) {
    if (gcc48canDIAF.find(l) == gcc48canDIAF.end())
        gcc48canDIAF[l] = (unsigned char *)malloc(l);
    va_list ap;
    va_start(ap, l);
    for(int i=0;i<l;i++) {
        gcc48canDIAF[l][i] = (unsigned char)va_arg(ap, int);
    }
    va_end(ap);
    return gcc48canDIAF[l];
}
void leap_init(_ctx_t *ctx)
{
  unsigned char data[16384];
  int ret;
  #include "leap_libusb_init.c.inc"
  for(std::map<int, unsigned char*>::iterator it = gcc48canDIAF.begin(); it != gcc48canDIAF.end(); ++it) {
    free(it->second);
  }
  gcc48canDIAF.clear();
}

using namespace leap;
driver::driver(boost::function<void(camdata_t*)> dc) :
  dataCallback(dc),
  _ctx(&_ctx_data),
  finisher(NULL),
  avoidFlashes(false)
{
  init();
}

void driver::agressivelyAvoidFlashes(bool state=true)
{
  avoidFlashes = state;
}

void driver::shutdown()
{
  _ctx->quit = 1;
  while (_ctx->quit != 3)
    sleep(0.1);
}

void driver::finishFunc()
{
  while (_ctx->quit != 2) {
    frame_t *current = NULL;
    {
      boost::mutex::scoped_lock l(completelock);
      while (!complete.empty())
      {
        current = complete.front();
        complete.pop();
      }
    }
    if (current != NULL)
    {
      for(uint32_t i=0;i<current->interleaved_pos;i+=2)
      {
        if (current->left_pos < VFRAME_SIZE) current->data.left[current->left_pos++] = current->data.interleaved[i];
        if (current->right_pos < VFRAME_SIZE) current->data.right[current->right_pos++] = current->data.interleaved[i+1];
      }
      if (dataCallback != NULL)
        dataCallback(&current->data);
      memset(current, 0, sizeof(frame_t));
      {
        boost::mutex::scoped_lock l(readylock);
        ready.push(current);
      }
    }
    sleep(0.01);
  }
  _ctx->quit = 3;
}

void driver::spin()
{
  finisher = new boost::thread(boost::bind(&driver::finishFunc, this));
  unsigned char data[16384];
  double lasttotal = 0, total = 0;
  int ret, transferred;
  frame_t *current = NULL;
  while(_ctx->quit == 0) {
    ret = libusb_bulk_transfer(_ctx->dev_handle, 0x83, data, sizeof(data), &transferred, 0);
    if (ret != 0) {
      printf("libusb_bulk_transfer(): %i: %s\n", ret, libusb_error_name(ret));
      continue;
    }

    int bHeaderLen = data[0];
    int bmHeaderInfo = data[1];

    uint32_t dwPresentationTime;
    memcpy(&dwPresentationTime, &(data[2]), sizeof(uint32_t));

    if (current == NULL)
    {
      {
        boost::mutex::scoped_lock l(readylock);
        if (!ready.empty())
        {
          current = ready.front();
          ready.pop();
        }
      }
      if (current == NULL)
      {
        current = (frame_t *)malloc(sizeof(frame_t));
        current->id = 0;
      }
    }
    if (current->id == 0 || dwPresentationTime > current->id) {
      memset(current, 0, sizeof(frame_t));
      current->id = dwPresentationTime;
      total = 0;
    }
    int i;
    for(i=bHeaderLen; i<transferred; i++) {
      if (current->interleaved_pos >= VFRAME_INTERLEAVED_SIZE)
      {
        break;
      }
      current->data.interleaved[current->interleaved_pos++] = data[i];
      total += data[i];
    }
    if (transferred - i != 0)
    {
      //printf("FOUND JUNK AT END OF READ!\n");
      current->id = 0;
      continue;
    }
    current->state++;

    if (bmHeaderInfo & UVC_STREAM_EOF) {
      if (current->interleaved_pos == VFRAME_INTERLEAVED_SIZE && (!avoidFlashes || lasttotal == 0 || abs(lasttotal-total)<1000000)) //empirically determined + hardcoded... this keeps the image from flashing
      {
        boost::mutex::scoped_lock l(completelock);
        complete.push(current);
        current = NULL;
      }
      lasttotal=total;
      if (current != NULL)
        current->id = 0;
    }
  }
  libusb_exit(_ctx->libusb_ctx);
  if (current)
  {
    free(current);
  }
  {
    boost::mutex::scoped_lock l(completelock);
    while (!complete.empty())
    {
      free(complete.front());
      complete.pop();
    }
  }
  {
    boost::mutex::scoped_lock l(readylock);
    while (!ready.empty())
    {
      free(ready.front());
      ready.pop();
    }
  }
  _ctx->quit = 2;
  if (finisher != NULL)
  {
    finisher->join();
    free(finisher);
    finisher = NULL;
  }
}

void driver::init()
{
  memset(_ctx, 0, sizeof (_ctx_data));
  libusb_init( & _ctx->libusb_ctx );
  _ctx->dev_handle = libusb_open_device_with_vid_pid(_ctx->libusb_ctx, LEAP_VID, LEAP_PID);
  if (_ctx->dev_handle == NULL) {
    fprintf(stderr, "ERROR: can't find leap.\n");
    exit(EXIT_FAILURE);
  }

  debug_printf("Found leap\n");

  int ret;

  ret = libusb_reset_device(_ctx->dev_handle);
  debug_printf("libusb_reset_device() ret: %i: %s\n", ret, libusb_error_name(ret));

  ret = libusb_kernel_driver_active(_ctx->dev_handle, 0);
  if ( ret == 1 ) {
    debug_printf("kernel active for interface 0\n");
    libusb_detach_kernel_driver(_ctx->dev_handle, 0);
  }
  else if (ret != 0) {
    printf("error\n");
    exit(EXIT_FAILURE);
  }

  ret = libusb_kernel_driver_active(_ctx->dev_handle, 1);
  if ( ret == 1 ) {
    debug_printf("kernel active\n");
    libusb_detach_kernel_driver(_ctx->dev_handle, 1);
  }
  else if (ret != 0) {
    printf("error\n");
    exit(EXIT_FAILURE);
  }

  ret = libusb_claim_interface(_ctx->dev_handle, 0);
  debug_printf("libusb_claim_interface() ret: %i: %s\n", ret, libusb_error_name(ret));

  ret = libusb_claim_interface(_ctx->dev_handle, 1);
  debug_printf("libusb_claim_interface() ret: %i: %s\n", ret, libusb_error_name(ret));

  leap_init(_ctx);

  debug_printf( "max %i\n",  libusb_get_max_packet_size(libusb_get_device( _ctx->dev_handle ), 0x83));
}
