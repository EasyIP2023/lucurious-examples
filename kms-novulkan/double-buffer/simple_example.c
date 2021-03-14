
/* Parts of this file are similar to what's here: https://github.com/dvdhrm/docs/blob/master/drm-howto/modeset-double-buffered.c */

/**
* The MIT License (MIT)
*
* Copyright (c) 2019-2021 Vincent Davis Jr.
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

struct _map_info {
  bool is_image;
  uint8_t *pixel_data;
  size_t bytes;
} map_info;

dlu_otma_mems ma = { .drmc_cnt = 1, .dod_cnt = 1, .dob_cnt = 2 };

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
 
/* Taken from: https://github.com/dvdhrm/docs/blob/master/drm-howto/modeset-double-buffered.c */
static uint8_t next_color(bool *up, uint8_t cur, unsigned int mod) {
  uint8_t next;
  
  next = cur + (*up ? 1 : -1) * (rand() % mod);
  if ((*up && next < cur) || (!*up && next > cur)) {
    *up = !*up;
    next = cur;
  }
  
  return next;
}

static void draw_screen(dlu_disp_core *core) {
  static uint8_t r, g, b, front_buf = 0;
  static bool r_up = true, g_up = true, b_up = true, run_once = false;

  if (map_info.is_image) goto display;

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

  for (uint32_t j = 0; j <  core->oc_data[0].mode.vdisplay; j++)
    for (uint32_t k = 0; k < core->oc_data[0].mode.hdisplay; k++) /* pitch = stride */
      *(uint32_t *) &map_info.pixel_data[core->dfb_data[0].pitches[0] * j + k * 4] = (r << 16) | (g << 8) | b;

display:
  dlu_fb_gbm_bo_write(core->dfb_data[front_buf^1].bo, map_info.pixel_data, map_info.bytes);

  if (!dlu_kms_modeset(core, front_buf^1)) return;
  front_buf ^= 1;
}

static void handle_screen(dlu_disp_core *core, const char *image) {
  if (image) {
    dlu_file_info picture = dlu_read_file(image);
    if (!picture.bytes) goto exit_func;

    int pw = 0, ph = 0, pchannels = 0, requested_channels = STBI_rgb_alpha;
    map_info.pixel_data = stbi_load_from_memory((unsigned char *) picture.bytes, picture.byte_size, &pw, &ph, &pchannels, requested_channels);
    if (!map_info.pixel_data) {
      dlu_log_me(DLU_DANGER, "[x] %s", stbi_failure_reason());
      free(picture.bytes);
      goto exit_func;
    }

    map_info.is_image = true;
    free(picture.bytes);

    if (pw != core->oc_data[0].mode.hdisplay) {
      dlu_log_me(DLU_DANGER, "[x] For now the picture pixel width must be picture must be the \
                           same amount of pixels the monitor will allow (%u)", core->oc_data[0].mode.hdisplay);
      goto exit_func;
    }

    /* Calculate image size in bytes */
    map_info.bytes = core->oc_data[0].mode.hdisplay * core->oc_data[0].mode.vdisplay * (requested_channels <= 0 ? pchannels : requested_channels); 
  } else {
    /* Create space to assign pixel data to */
    map_info.bytes = core->oc_data[0].mode.hdisplay * core->oc_data[0].mode.vdisplay * 4; /* 4 bytes = 32 bit, R = 8 bits, G = 8 bits, B = 8 bits, A = 8 bits */
    map_info.pixel_data = mmap(NULL, map_info.bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, INDEX_IGNORE, core->dfb_data[0].offsets[0]);
    if (map_info.pixel_data == MAP_FAILED) { dlu_log_me(DLU_DANGER, "[x] %s", strerror(errno)); goto exit_func; }
  }

  uint32_t key_code = UINT32_MAX;
  while(1) {
    draw_screen(core);
    if (dlu_input_retrieve(core, &key_code)) {
      switch(key_code) {
        case KEY_ESC: goto exit_func; break;
        case KEY_Q: goto exit_func; break;
        default: break;
      }
    }
  }

exit_func:
  if (map_info.pixel_data) {
    if (image) stbi_image_free(map_info.pixel_data);
    else munmap(map_info.pixel_data, map_info.bytes);
  }
}

int main(int argc, char *argv[]) {

  if (argc > 2) {
    dlu_log_me(DLU_DANGER, "Usage: %s <path to image>", argv[0]);
    return EXIT_FAILURE;
  }

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

  handle_screen(core, argv[1]);

  FREEME(core);

  return EXIT_SUCCESS;
}
