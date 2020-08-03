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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include <sys/mman.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "simple_example.h"

#define UNUSED __attribute__((unused))

dlu_otma_mems ma = { .drmc_cnt = 1, .dod_cnt = 1, .odb_cnt = 2 };

static bool init_buffs(dlu_drm_core *core) {
  bool err;

  err = dlu_otba(DLU_DEVICE_OUTPUT_DATA, core, INDEX_IGNORE, ma.dod_cnt);
  if (!err) return err;

  err = dlu_otba(DLU_DEVICE_OUTPUT_BUFF_DATA, core, INDEX_IGNORE, ma.odb_cnt);
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

static void modeset_draw(dlu_drm_core *core) {
  uint8_t r, g, b;
  bool r_up, g_up, b_up;
  unsigned int front_buf = 0;
  int UNUSED ret;

  uint32_t width = core->output_data[0].mode.hdisplay;
  uint32_t height = core->output_data[0].mode.vdisplay;
  uint32_t offset = core->buff_data[0].offsets[0];
  uint32_t stride = core->buff_data[0].pitches[0];

  struct _map_info {
    uint8_t *pixel_data;
    int fd;
  } map_info[2];

  map_info[0].fd = core->buff_data[0].dma_buf_fds[0];
  map_info[1].fd = core->buff_data[1].dma_buf_fds[0];

  size_t bytes = width * height * 4;
  map_info[0].pixel_data = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, map_info[0].fd, offset);
  if (map_info[0].pixel_data == MAP_FAILED) { dlu_log_me(DLU_DANGER, "[x] %s", strerror(errno)); goto exit_func; }

  map_info[1].pixel_data = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, map_info[1].fd, offset);
  if (map_info[1].pixel_data == MAP_FAILED) { dlu_log_me(DLU_DANGER, "[x] %s", strerror(errno)); goto exit_func; }

  srand(time(NULL));
  r = rand() % 0xff;
  g = rand() % 0xff;
  b = rand() % 0xff;
  r_up = g_up = b_up = true;

  for (uint32_t i = 0; i < 51; i++) {
    r = next_color(&r_up, r, 20);
    g = next_color(&g_up, g, 10);
    b = next_color(&b_up, b, 5);

    for (uint32_t j = 0; j <  height; j++)
      for (uint32_t k = 0; k < width; k++)
        *(uint32_t *) &map_info[front_buf^1].pixel_data[stride * j + k * 4] = (r << 16) | (g << 8) | b;

    void *pData = NULL;
    dlu_drm_gbm_bo_map(core, front_buf^1, &pData, GBM_BO_TRANSFER_READ_WRITE);

    /* Copy data to back buffer */
    memcpy(pData, map_info[front_buf^1].pixel_data, bytes);

    dlu_drm_gbm_bo_unmap(core->buff_data[front_buf^1].bo, pData);
    pData = NULL;

    /* Present front buffer */
    if (!dlu_drm_do_modeset(core, front_buf)) {
      dlu_log_me(DLU_DANGER, "[x] cannot flip CRTC for connector (%u): %s\n", core->output_data[0].conn_id, strerror(errno));
      goto exit_func_mm;
    }

    /* Clear Pixel Data */
    memset(map_info[front_buf].pixel_data, 0, bytes);
    
    front_buf ^= 1;
    usleep(100000);
  }

exit_func_mm:
  munmap(map_info[0].pixel_data, bytes);
  munmap(map_info[1].pixel_data, bytes);
exit_func:
  return;
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

  uint32_t bo_flags = GBM_BO_USE_RENDERING | GBM_BO_USE_SCANOUT | GBM_BO_USE_LINEAR | GBM_BO_USE_WRITE;
  for (uint32_t i = 0; i < ma.odb_cnt; i++)
    check_err(!dlu_drm_create_fb(DLU_DRM_GBM_BO, core, i, cur_od, GBM_BO_FORMAT_XRGB8888, bo_flags, 0), core);

  // check_err(!dlu_drm_do_modeset(core, cur_bi), core);

  modeset_draw(core);

  FREEME(core);

  return EXIT_SUCCESS;
}
