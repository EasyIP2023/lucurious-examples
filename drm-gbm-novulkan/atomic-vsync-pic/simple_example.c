/**
* The MIT License (MIT)
*
* Copyright (c) 2019-2020 Vincent Davis Jr.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/epoll.h>

/* For Libinput input event codes */
#include <linux/input-event-codes.h>

#include "simple_example.h"

#define UNUSED __attribute__((unused))

static dlu_otma_mems ma = { .drmc_cnt = 1, .dod_cnt = 1, .dob_cnt = 2 };

static struct _map_info {
  size_t bytes_sz;
  unsigned char *pixel_data;
} map_info;

static inline void init_epoll_values(struct epoll_event *event) {
  event->events = 0; event->data.ptr = NULL; event->data.fd = 0;
  event->data.u32 = 0; event->data.u64 = 0;
}

static bool init_buffs(dlu_disp_core *core) {
  bool err;

  err = dlu_otba(DLU_DEVICE_OUTPUT_DATA, core, INDEX_IGNORE, ma.dod_cnt);
  if (!err) return err;

  err = dlu_otba(DLU_DEVICE_OUTPUT_BUFF_DATA, core, INDEX_IGNORE, ma.dob_cnt);
  if (!err) return err;

  return err;
}

static void draw_screen(dlu_disp_core *core, uint8_t front_buf) {
  dlu_fb_gbm_bo_write(core->buff_data[front_buf].bo, map_info.pixel_data, map_info.bytes_sz);

  drmModeAtomicReq *req = dlu_kms_atomic_alloc();

  dlu_kms_atomic_req(core, front_buf, req);
  dlu_kms_atomic_commit(core, front_buf, req, true);

  dlu_kms_atomic_free(req);
}

static void atomic_event_handler(int UNUSED fd, unsigned int UNUSED sequence, unsigned int UNUSED tv_sec, unsigned int UNUSED tv_usec, unsigned int UNUSED crtc_id, void *data) {
  static uint8_t front_buf = 0;
  dlu_disp_core *core = (dlu_disp_core *) data;
 
  core->output_data[front_buf^1].pflip = false;
  draw_screen(core, front_buf^1);

  front_buf ^= 1;
}

static void handle_screen(dlu_disp_core *core) {
  uint32_t event_fd = 0, ready_fds = 0, max_events = 2;
  struct epoll_event *events = NULL;

  /* Version 3 utilizes the page_flip_handler2, so we use that. */
  drmEventContext ev;
  memset(&ev, 0, sizeof(ev));
  ev.version = 3;
  ev.page_flip_handler2 = atomic_event_handler;

  dlu_file_info picture = dlu_read_file(IMG_SRC);
  if (!picture.bytes) return;

  /* First load the image */
  int pw = 0, ph = 0, pchannels = 0, requested_channels = STBI_rgb_alpha;
  map_info.pixel_data = stbi_load_from_memory((unsigned char *) picture.bytes, picture.byte_size, &pw, &ph, &pchannels, requested_channels);
  if (!map_info.pixel_data) {  
    dlu_log_me(DLU_DANGER, "[x] %s failed to load", IMG_SRC);
    dlu_log_me(DLU_DANGER, "[x] %s", stbi_failure_reason());
    dlu_freeup_spriv_bytes(DLU_UTILS_FILE_SPRIV, picture.bytes);
    goto exit_free_pixels;
  }

  dlu_log_me(DLU_SUCCESS, "%s successfully loaded", IMG_SRC);
  dlu_log_me(DLU_SUCCESS, "Image width: %dpx, Image height: %dpx", pw, ph);
  dlu_freeup_spriv_bytes(DLU_UTILS_FILE_SPRIV, picture.bytes); picture.bytes = NULL;
  /* End of image loader */

  /* Calculate image size in bytes */
  map_info.bytes_sz = pw * ph * (requested_channels <= 0 ? pchannels : requested_channels);

  /* Draw into intial buffer */
  draw_screen(core, 1);

  if ((event_fd = epoll_create1(0)) == UINT32_MAX) {
    dlu_log_me(DLU_DANGER, "[x] epoll_create1: %s", strerror(errno));
    goto exit_free_pixels;
  }

  events = alloca(max_events * sizeof(struct epoll_event));

  /**
  * EPOLLIN: Associate a file descriptor for read() operations
  * By default epoll is level triggered.
  * Add kmsfd to epoll interest list 
  */
  struct epoll_event event;
  init_epoll_values(&event);

  event.events = EPOLLIN;
  event.data.fd = core->device.kmsfd;
  if (epoll_ctl(event_fd, EPOLL_CTL_ADD, event.data.fd, &event) == -1) {
    dlu_log_me(DLU_DANGER, "[x] epoll_ctl: %s", strerror(errno));
    goto exit_free_events;
  } 

  /* Add libinput FD to epoll interest list */
  int input_fd = dlu_input_retrieve_fd(core);
  init_epoll_values(&event);

  event.events = EPOLLIN;
  event.data.fd = input_fd; /* Not needed but adding anyways */
  if (epoll_ctl(event_fd, EPOLL_CTL_ADD, event.data.fd, &event) == -1) {
    dlu_log_me(DLU_DANGER, "[x] epoll_ctl: %s", strerror(errno));
    goto exit_free_events;
  }

  uint32_t key_code = UINT32_MAX;
  while(1) {
    /* Fetch the list of FD's that are ready for I/O from interest list. The kernel checks for this */
    ready_fds = epoll_wait(event_fd, events, max_events, -1);
    if (ready_fds == UINT32_MAX) {
      dlu_log_me(DLU_DANGER, "[x] epoll_wait: %s", strerror(errno));
      goto exit_free_events;
    }

    for (uint32_t i = 0; i < ready_fds; i++) {
      if (!(events[i].events & EPOLLIN)) continue;
  
      if (dlu_input_retrieve(core, &key_code)) {
        switch(key_code) {
          case KEY_ESC: goto exit_free_events; break;
          case KEY_Q: goto exit_free_events; break;
          default: break;
        }
      }

      if (events[i].data.fd == (int) core->device.kmsfd)
        if (dlu_kms_handle_event(events[i].data.fd, &ev))
          goto exit_free_events;
    }
  }

exit_free_events:
  close(event_fd);
exit_free_pixels:
  stbi_image_free(map_info.pixel_data);
}

int main(void) {

  if (!dlu_otma(DLU_LARGE_BLOCK_PRIV, ma)) return EXIT_FAILURE;

  dlu_disp_core *core = dlu_disp_init_core();
  check_err(!core, NULL);

  check_err(!init_buffs(core), core);

  /**
  * RUN IN TTY:
  * First creates a logind session. This allows for access to
  * privileged devices without being root.
  * Then find a suitable kms node = drm device = gpu
  */
  check_err(!dlu_session_create(core), core)
  check_err(!dlu_kms_node_create(core, "/dev/dri/card0"), core)

  dlu_disp_device_info dinfo[1];
  check_err(!dlu_kms_q_output_chain(core, dinfo), core) 

  uint32_t cur_odb = 0;
  /* Saves the sate of the Plane -> CRTC -> Encoder -> Connector pair */
  check_err(!dlu_kms_enum_device(core, cur_odb, dinfo->conn_idx, dinfo->enc_idx, dinfo->crtc_idx,
                                dinfo->plane_idx, dinfo->refresh, dinfo->conn_name), core);

  /* Create libinput context, Establish connection to kernel input system */
  check_err(!dlu_input_create(core), core);

  check_err(!dlu_fb_create(core, ma.dob_cnt, &(dlu_disp_fb_info) {
    .type = DLU_DISPLAY_GBM_BO, .cur_odb = cur_odb, .depth = 24, .bpp = 32,
    .bo_flags = GBM_BO_USE_SCANOUT|GBM_BO_USE_WRITE, .format = GBM_BO_FORMAT_XRGB8888, .flags = 0
  }), core);

  for (uint32_t i = 0; i < ma.dob_cnt; i++)
    check_err(!dlu_kms_modeset(core, i), core);

  handle_screen(core);

  FREEME(core);

  return EXIT_SUCCESS;
}
