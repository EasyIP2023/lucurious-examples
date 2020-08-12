
/* Parts of this file are similar to what's here: https://github.com/dvdhrm/docs/blob/master/drm-howto/modeset-vsync.c */

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

/* File not fully implemented */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/epoll.h>
#include <fcntl.h>

#include "simple_example.h"

#define UNUSED __attribute__((unused))

struct _map_info {
  size_t bytes;
  uint8_t *pixel_data;
  int fd;
};

static dlu_otma_mems ma = { .drmc_cnt = 1, .dod_cnt = 1, .dob_cnt = 2 };
static struct _map_info map_info[2];

static inline void init_epoll_values(struct epoll_event *event) {
  event->events = 0; event->data.ptr = NULL; event->data.fd = 0;
  event->data.u32 = 0; event->data.u64 = 0;
}

static bool init_buffs(dlu_drm_core *core) {
  bool err;

  err = dlu_otba(DLU_DEVICE_OUTPUT_DATA, core, INDEX_IGNORE, ma.dod_cnt);
  if (!err) return err;

  err = dlu_otba(DLU_DEVICE_OUTPUT_BUFF_DATA, core, INDEX_IGNORE, ma.dob_cnt);
  if (!err) return err;

  return err;
}

static uint8_t next_color(bool *up, uint8_t cur, unsigned int mod) {
  uint8_t next;
  
  next = cur + (*up ? 1 : -1) * (rand() % mod);
  if ((*up && next < cur) || (!*up && next > cur)) {
    *up = !*up;
    next = cur;
  }
  
  return next;
}

static void draw_screen(dlu_drm_core *core) {
  static uint8_t r = 0, g = 0, b = 0;
  static bool r_up = true, g_up = true, b_up = true, run_once = false;
  static uint32_t front_buf = 0;

  if (!run_once) {
    srand(time(NULL));
    r = rand() % 0xff;
    g = rand() % 0xff;
    b = rand() % 0xff;
    run_once = true;
  }

  r = next_color(&r_up, r, 20);
  g = next_color(&g_up, g, 10);
  b = next_color(&b_up, b, 5);

  if (!dlu_drm_do_page_flip(core, front_buf, core)) return;

  /*  Render back buffer */
  for (uint32_t j = 0; j <  core->output_data[0].mode.vdisplay; j++)
    for (uint32_t k = 0; k < core->output_data[0].mode.hdisplay; k++) /* pitch = stride */
      *(uint32_t *) &map_info[front_buf^1].pixel_data[core->buff_data[0].pitches[0] * j + k * 4] = (r << 16) | (g << 8) | b;

  /* Page flip handle event */
  dlu_drm_gbm_bo_write(core->buff_data[front_buf^1].bo, map_info[front_buf^1].pixel_data, map_info[front_buf].bytes);

  front_buf ^= 1;
}

static void modeset_page_flip_event(int UNUSED fd, unsigned int UNUSED frame, unsigned int UNUSED sec, unsigned int UNUSED usec, void *data) {
  draw_screen((dlu_drm_core *) data);
}

static void handle_screen(dlu_drm_core *core) {
  uint32_t event_fd = 0, ready_fds = 0, max_events = 1;
  struct epoll_event *events = NULL;

  /**
  * Set this to only the latest version you support. Version 2
  * introduced the page_flip_handler, so we use that.
  */
  drmEventContext ev;
  memset(&ev, 0, sizeof(ev));
  ev.version = 2;
  ev.page_flip_handler = modeset_page_flip_event;

  /* Create space to assign pixel data to */
  map_info[0].bytes = map_info[1].bytes = core->output_data[0].mode.hdisplay * core->output_data[0].mode.vdisplay * 4;
  for (uint32_t i = 0; i < ma.dob_cnt; i++) {
    map_info[i].fd = core->buff_data[i].dma_buf_fds[0]; // core->device.kmsfd;
    map_info[i].pixel_data = mmap(NULL, map_info[i].bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, INDEX_IGNORE, core->buff_data[i].offsets[0]);
    if (map_info[i].pixel_data == MAP_FAILED) { dlu_log_me(DLU_DANGER, "[x] %s", strerror(errno)); goto exit_func_mm; }
  }

  /* Initial scheduling of page-flip */
  draw_screen(core);

  if ((event_fd = epoll_create1(0)) == UINT32_MAX) {
    dlu_log_me(DLU_DANGER, "[x] epoll_create1: %s", strerror(errno));
    goto exit_func_mm;
  }

  events = calloc(max_events, sizeof(struct epoll_event));
  if (!events) {
    dlu_log_me(DLU_DANGER, "[x] calloc: %s", strerror(errno));
    goto exit_free_events;
  }

  /**
  * EPOLLIN: Associate a file descriptor for read() operations
  * By default epoll is level triggered.
  * Add kmsfd to epoll watch list 
  */
  struct epoll_event event;
  init_epoll_values(&event);

  event.events = EPOLLIN;
  event.data.fd = core->device.kmsfd;
  if (epoll_ctl(event_fd, EPOLL_CTL_ADD, event.data.fd, &event) == -1) {
    dlu_log_me(DLU_DANGER, "[x] epoll_ctl: %s", strerror(errno));
    goto exit_free_events;
  }

  /**
  * Libinput key codes from input-event-codes.h.
  * 1 == KEY_ESC
  */
  uint32_t key_code = UINT32_MAX;
  while(1) {
    ready_fds = epoll_wait(event_fd, events, max_events, -1);
    if (ready_fds == UINT32_MAX) {
      dlu_log_me(DLU_DANGER, "[x] epoll_wait: %s", strerror(errno));
      goto exit_free_events;
    }

    for (uint32_t i = 0; i < ready_fds; i++) {
      if (!(events[i].events & EPOLLIN)) continue;

      if (events[i].data.fd == (int) core->device.kmsfd)
        if (dlu_drm_do_handle_event(events[i].data.fd, &ev))
          goto exit_free_events;

      if (dlu_drm_retrieve_input(core, &key_code)) {
        switch(key_code) {
          case 1: goto exit_free_events; break;  /* KEY_ESC */ 
          default: break;
        }
      }
    }
  }

exit_free_events:
  free(events);
  close(event_fd);
exit_func_mm:
  for (uint32_t i = 0; i < ma.dob_cnt; i++)
    if (map_info[i].pixel_data)
      munmap(map_info[i].pixel_data, map_info[i].bytes);
}

int main(void) {

  if (!dlu_otma(DLU_LARGE_BLOCK_PRIV, ma)) return EXIT_FAILURE;

  dlu_drm_core *core = dlu_drm_init_core();
  check_err(!core, NULL);

  check_err(!init_buffs(core), core);

  /**
  * RUN IN TTY:
  * First creates a logind session. This allows for access to
  * privileged devices without being root.
  * Then find a suitable kms node = drm device = gpu
  */
  check_err(!dlu_drm_create_session(core), core)
  check_err(!dlu_drm_create_kms_node(core, "/dev/dri/card0"), core)

  dlu_drm_device_info dinfo[1];
  check_err(!dlu_drm_q_output_dev_info(core, dinfo), core) 

  uint32_t cur_od = 0, UNUSED cur_bi = 0;
  /* Saves the sate of the Plane -> CRTC -> Encoder -> Connector pair */
  check_err(!dlu_drm_kms_node_enum_ouput_dev(core, cur_od, dinfo->conn_idx, dinfo->enc_idx,
                                             dinfo->crtc_idx, dinfo->plane_idx, dinfo->refresh,
                                             dinfo->conn_name), core);

  /* Create gbm_device to allocate framebuffers from. Then allocate the actual framebuffer */
  check_err(!dlu_drm_create_gbm_device(core), core);

  /* Create libinput FD, Establish connection to kernel input system */
  check_err(!dlu_drm_create_input_handle(core), core);

  uint32_t bo_flags = GBM_BO_USE_SCANOUT  | GBM_BO_USE_WRITE;
  for (uint32_t i = 0; i < ma.dob_cnt; i++) {
    check_err(!dlu_drm_create_fb(DLU_DRM_GBM_BO, core, i, cur_od, GBM_BO_FORMAT_XRGB8888, 24, 32, bo_flags, 0), core);
    check_err(!dlu_drm_do_modeset(core, i), core);
  }

  handle_screen(core);

  FREEME(core);

  return EXIT_SUCCESS;
}
