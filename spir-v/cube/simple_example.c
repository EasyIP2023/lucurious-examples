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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>

#include "simple_example.h"

#define NUM_DESCRIPTOR_SETS 1
#define WIDTH 800
#define HEIGHT 600
#define DEPTH 1

static dlu_otma_mems ma = {
  .vkcomp_cnt = 1, .desc_cnt = 1, .gp_cnt = 1,
  .si_cnt = 5, .scd_cnt = 1, .gpd_cnt = 1,
  .cmdd_cnt = 1, .bd_cnt = 2, .dd_cnt = 1
};

static struct uniform_block_data {
  mat4 proj;
  mat4 view;
  mat4 model;
  mat4 clip;
  mat4 mvp;
} ubd;

static void dlu_print_matrices() {
  dlu_log_me(DLU_INFO, "Perspective Matrix");
  dlu_log_me(DLU_INFO, "Projection from camera to screen");
  dlu_print_matrix(DLU_MAT4, ubd.proj);
  dlu_log_me(DLU_INFO, "View Matrix");
  dlu_log_me(DLU_INFO, "View from world space to camera space");
  dlu_print_matrix(DLU_MAT4, ubd.view);
  dlu_log_me(DLU_INFO, "Model Matrix");
  dlu_log_me(DLU_INFO, "Mapping object's local coordinate space into world space");
  dlu_print_matrix(DLU_MAT4, ubd.model);
  dlu_log_me(DLU_INFO, "Clip Matrix");
  dlu_print_matrix(DLU_MAT4, ubd.clip);
  dlu_log_me(DLU_INFO, "MVP Matrix");
  dlu_print_matrix(DLU_MAT4, ubd.mvp);
}

static bool init_buffs(vkcomp *app) {
  bool err;

  err = dlu_otba(DLU_BUFF_DATA, app, INDEX_IGNORE, 2);
  if (!err) return err;

  err = dlu_otba(DLU_SC_DATA, app, INDEX_IGNORE, 1);
  if (!err) return err;

  err = dlu_otba(DLU_GP_DATA, app, INDEX_IGNORE, 1);
  if (!err) return err;

  err = dlu_otba(DLU_CMD_DATA, app, INDEX_IGNORE, 1);
  if (!err) return err;

  err = dlu_otba(DLU_DESC_DATA, app, INDEX_IGNORE, 1);
  if (!err) return err;

  return err;
}

int main(void) {
  VkResult err;

  if (!dlu_otma(DLU_LARGE_BLOCK_PRIV, ma)) return EXIT_FAILURE;

  wclient *wc = dlu_init_wc();
  check_err(!wc, NULL, NULL, NULL)

  vkcomp *app = dlu_init_vk();
  check_err(!app, NULL, wc, NULL)

  err = init_buffs(app);
  check_err(!err, app, wc, NULL)

  err = dlu_create_instance(app, "Draw Cube", "Desktop Engine", 0, NULL, ARR_LEN(instance_extensions), instance_extensions);
  check_err(err, app, wc, NULL)

  check_err(!dlu_create_client(wc), app, wc, NULL)

  /* initialize vulkan app surface */
  err = dlu_create_vkwayland_surfaceKHR(app, wc->display, wc->surface);
  check_err(err, app, wc, NULL)

  /* This will get the physical device, it's properties, and features */
  VkPhysicalDeviceProperties device_props;
  VkPhysicalDeviceFeatures device_feats;
  err = dlu_create_physical_device(app, VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU, &device_props, &device_feats);
  check_err(err, app, wc, NULL)

  err = dlu_create_queue_families(app, VK_QUEUE_GRAPHICS_BIT);
  check_err(err, app, wc, NULL)

  err = dlu_create_logical_device(app, &device_feats, 1, 0, NULL, ARR_LEN(device_extensions), device_extensions);
  check_err(err, app, wc, NULL)

  VkSurfaceCapabilitiesKHR capabilities = dlu_get_physical_device_surface_capabilities(app);
  check_err(capabilities.minImageCount == UINT32_MAX, app, wc, NULL)

  /**
  * VK_FORMAT_B8G8R8A8_UNORM will store the R, G, B and alpha channels
  * in that order with an 8 bit unsigned integer and a total of 32 bits per pixel.
  * SRGB if used for colorSpace if available, because it
  * results in more accurate perceived colors
  */
  VkSurfaceFormatKHR surface_fmt = dlu_choose_swap_surface_format(app, VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
  check_err(surface_fmt.format == VK_FORMAT_UNDEFINED, app, wc, NULL)

  VkPresentModeKHR pres_mode = dlu_choose_swap_present_mode(app);
  check_err(pres_mode == VK_PRESENT_MODE_MAX_ENUM_KHR, app, wc, NULL)

  VkExtent2D extent2D = dlu_choose_swap_extent(capabilities, WIDTH, HEIGHT);
  check_err(extent2D.width == UINT32_MAX, app, wc, NULL)

  uint32_t cur_buff = 0, cur_scd = 0, cur_pool = 0, cur_dd = 0, cur_gpd = 0, cur_bd = 0, cur_cmd = 0;
  err = dlu_otba(DLU_SC_DATA_MEMS, app, cur_scd, capabilities.minImageCount);
  check_err(!err, app, wc, NULL)

  err = dlu_create_swap_chain(app, cur_scd, capabilities, surface_fmt, pres_mode, extent2D.width, extent2D.height, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
  check_err(err, app, wc, NULL)

  err = dlu_create_cmd_pool(app, cur_scd, cur_cmd, app->indices.graphics_family, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
  check_err(err, app, wc, NULL)

  err = dlu_create_cmd_buffs(app, cur_pool, cur_scd, VK_COMMAND_BUFFER_LEVEL_PRIMARY);
  check_err(err, app, wc, NULL)

  /* describe what the image's purpose is and which part of the image should be accessed */
  VkComponentMapping comp_map = dlu_set_component_mapping(VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A);
  VkImageSubresourceRange img_sub_rr = dlu_set_image_sub_resource_range(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);
  VkImageViewCreateInfo img_view_info = dlu_set_image_view_info(0, VK_NULL_HANDLE, VK_IMAGE_VIEW_TYPE_2D, surface_fmt.format, comp_map, img_sub_rr);

  err = dlu_create_image_views(DLU_SC_IMAGE_VIEWS, app, cur_scd, &img_view_info);
  check_err(err, app, wc, NULL)

  VkExtent3D extend3D = {extent2D.width, extent2D.height, DEPTH};
  VkImageCreateInfo img_info = dlu_set_image_info(0, VK_IMAGE_TYPE_2D, VK_FORMAT_D16_UNORM, extend3D, 1,
    1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    VK_SHARING_MODE_EXCLUSIVE, 0, NULL, VK_IMAGE_LAYOUT_UNDEFINED
  );

  img_sub_rr = dlu_set_image_sub_resource_range(VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1);
  img_view_info = dlu_set_image_view_info(0, VK_NULL_HANDLE, VK_IMAGE_TYPE_2D, VK_FORMAT_D16_UNORM, comp_map, img_sub_rr);
  err = dlu_create_depth_buff(app, cur_scd, &img_info, &img_view_info, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  check_err(err, app, wc, NULL)

  err = dlu_create_syncs(app, cur_scd);
  check_err(err, app, wc, NULL)

  /* Acquire the swapchain image in order to set its layout */
  uint32_t cur_img, cur_frame = 0;
  err = dlu_acquire_sc_image_index(app, cur_scd, cur_frame, &cur_img);
  check_err(err, app, wc, NULL)

  float fovy = dlu_set_radian(45.0f);
  float hw = (float) extent2D.width / (float) extent2D.height;
  if (extent2D.width > extent2D.height) fovy *= hw;
  dlu_set_perspective(ubd.proj, fovy, hw, 0.1f, 100.0f);
  dlu_set_lookat(ubd.view, eye, center, up);
  dlu_set_matrix(DLU_MAT4_IDENTITY, ubd.model, NULL);
  dlu_set_matrix(DLU_MAT4, ubd.clip, clip_matrix);
  dlu_set_mvp_matrix(ubd.mvp, &ubd.clip, &ubd.proj, &ubd.view, &ubd.model);
  dlu_print_matrices();

  /* Create uniform buffer that has the transformation matrices (for the vertex shader) */
  err = dlu_create_vk_buffer(app, cur_bd, sizeof(ubd.mvp), 0,
    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, NULL, 'u',
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
  );
  check_err(err, app, wc, NULL)

  /* Only map mvp matrix into memory so that it's binary compatible with shader variable */
  err = dlu_create_vk_buff_mem_map(app, cur_bd, ubd.mvp);
  check_err(err, app, wc, NULL)
  cur_bd++;

  /* MVP transformation is in a single uniform buffer variable (not an array), So descriptor count is 1 */
  err = dlu_otba(DLU_DESC_DATA_MEMS, app, cur_dd, NUM_DESCRIPTOR_SETS);
  check_err(!err, app, wc, NULL)

  VkDescriptorSetLayoutBinding desc_set = dlu_set_desc_set_layout_binding(
    0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, NUM_DESCRIPTOR_SETS, VK_SHADER_STAGE_VERTEX_BIT, NULL
  );

  VkDescriptorSetLayoutCreateInfo desc_set_info = dlu_set_desc_set_layout_info(0, 1, &desc_set);

  /* Using same layout for all VkDescriptorSetLayout objects */
  err = dlu_create_desc_set_layouts(app, cur_dd, &desc_set_info);
  check_err(err, app, wc, NULL)

  VkDescriptorPoolSize pool_size = dlu_set_desc_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
  err = dlu_create_desc_pool(app, cur_dd, 0, 1, &pool_size);
  check_err(err, app, wc, NULL)

  err = dlu_create_desc_set(app, cur_dd);
  check_err(err, app, wc, NULL)

  VkDescriptorBufferInfo buff_info = dlu_set_desc_buff_info(app->buff_data[0].buff, 0, sizeof(ubd.mvp));
  VkWriteDescriptorSet write = dlu_write_desc_set(app->desc_data[cur_dd].desc_set[0], 0, 0,
                               app->desc_data[cur_dd].dlsc, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, NULL,
                               &buff_info, NULL);

  dlu_update_desc_sets(app, NUM_DESCRIPTOR_SETS, &write, 0, NULL);

  err = dlu_create_pipeline_layout(app, cur_gpd, cur_dd, 0, NULL);
  check_err(err, app, wc, NULL)

  /* start of render pass creation */
  dlu_log_me(DLU_INFO, "Start of render pass creation");

  VkAttachmentDescription attachments[2];
  /* Create render pass color attachment for swapchain images */
  attachments[0] = dlu_set_attachment_desc(surface_fmt.format,
    VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
    VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
  );

  /* Create render pass stencil/depth attachment for depth buffer */
  attachments[1] = dlu_set_attachment_desc(VK_FORMAT_D16_UNORM, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_CLEAR, 
    VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, 
    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
  );

  VkAttachmentReference color_ref = dlu_set_attachment_ref(0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkAttachmentReference depth_ref = dlu_set_attachment_ref(1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
  VkSubpassDescription subpass = dlu_set_subpass_desc(VK_PIPELINE_BIND_POINT_GRAPHICS, 0, NULL, 1, &color_ref, NULL, &depth_ref, 0, NULL);

  err = dlu_create_render_pass(app, cur_gpd, 2, attachments, 1, &subpass, 0, NULL);
  check_err(err, app, wc, NULL)

  dlu_log_me(DLU_SUCCESS, "Successfully created the render pass!!!");
  /* End of render pass creation */

  VkImageView vkimg_attach[2];
  vkimg_attach[1] = app->sc_data[cur_scd].depth.view;
  err = dlu_create_framebuffers(app, cur_scd, cur_gpd, 2, vkimg_attach, extent2D.width, extent2D.height, 1);
  check_err(err, app, wc, NULL)

  err = dlu_create_pipeline_cache(app, 0, NULL);
  check_err(err, app, wc, NULL)

  /* Start of vertex buffer */
  VkDeviceSize vsize = sizeof(vertices);
  err = dlu_create_vk_buffer(
    app, cur_bd, vsize, 0, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    VK_SHARING_MODE_EXCLUSIVE, 0, NULL, 'v',
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
  );
  check_err(err, app, wc, NULL)

  err = dlu_create_vk_buff_mem_map(app, cur_bd, vertices);
  check_err(err, app, wc, NULL)
  cur_bd++;

  VkVertexInputBindingDescription vi_binding = dlu_set_vertex_input_binding_desc(0, sizeof(vertex_3D), VK_VERTEX_INPUT_RATE_VERTEX);

  VkVertexInputAttributeDescription vi_attribs[2];
  vi_attribs[0] = dlu_set_vertex_input_attrib_desc(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vertex_3D, pos));
  vi_attribs[1] = dlu_set_vertex_input_attrib_desc(1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(vertex_3D, color));

  VkPipelineVertexInputStateCreateInfo vertex_input_info = dlu_set_vertex_input_state_info(
    1, &vi_binding, 2, vi_attribs
  );
  /* End of vertex buffer */

  dlu_log_me(DLU_INFO, "Start of shader creation");
  dlu_file_info shi_vert = dlu_read_file("vert.spv");
  check_err(!shi_vert.bytes, app, wc, NULL)
  dlu_file_info shi_frag = dlu_read_file("frag.spv");
  check_err(!shi_frag.bytes, app, wc, NULL)
  dlu_log_me(DLU_SUCCESS, "vert.spv and frag.spv bytes officially created");

  VkShaderModule vert_shader_module = dlu_create_shader_module(app, shi_vert.bytes, shi_vert.byte_size);
  check_err(!vert_shader_module, app, wc, NULL)

  VkShaderModule frag_shader_module = dlu_create_shader_module(app, shi_frag.bytes, shi_frag.byte_size);
  check_err(!frag_shader_module, app, wc, vert_shader_module)

  dlu_freeup_spriv_bytes(DLU_UTILS_FILE_SPRIV, shi_vert.bytes);
  dlu_freeup_spriv_bytes(DLU_UTILS_FILE_SPRIV, shi_frag.bytes);
  dlu_log_me(DLU_INFO, "End of shader creation");

  VkPipelineShaderStageCreateInfo vert_shader_stage_info = dlu_set_shader_stage_info(
    vert_shader_module, "main", VK_SHADER_STAGE_VERTEX_BIT, NULL
  );

  VkPipelineShaderStageCreateInfo frag_shader_stage_info = dlu_set_shader_stage_info(
    frag_shader_module, "main", VK_SHADER_STAGE_FRAGMENT_BIT, NULL
  );

  VkPipelineShaderStageCreateInfo shader_stages[] = {
    vert_shader_stage_info, frag_shader_stage_info
  };

  VkDynamicState dynamic_states[2] = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR
  };

  VkPipelineDynamicStateCreateInfo dynamic_state = dlu_set_dynamic_state_info(2, dynamic_states);

  VkPipelineInputAssemblyStateCreateInfo input_assembly = dlu_set_input_assembly_state_info(
    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE
  );

  VkPipelineRasterizationStateCreateInfo rasterizer = dlu_set_rasterization_state_info(
    VK_TRUE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT,
    VK_FRONT_FACE_CLOCKWISE, VK_FALSE, 0.0f, 0.0f, 0.0f, 1.0f
  );

  VkPipelineColorBlendAttachmentState color_blend_attachment = dlu_set_color_blend_attachment_state(
    VK_FALSE, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
    VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD,
    0xf
  );

  float blend_const[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  VkPipelineColorBlendStateCreateInfo color_blending = dlu_set_color_blend_attachment_state_info(
    VK_FALSE, VK_LOGIC_OP_NO_OP, 1, &color_blend_attachment, blend_const
  );

  VkViewport viewport = dlu_set_view_port(0.0f, 0.0f, (float) extent2D.width, (float) extent2D.height, 0.0f, 1.0f);
  VkRect2D scissor = dlu_set_rect2D(0, 0, extent2D.width, extent2D.height);
  VkPipelineViewportStateCreateInfo view_port_info = dlu_set_view_port_state_info(1, &viewport, 1, &scissor);

  VkStencilOpState back = dlu_set_stencil_op_state(
    VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
    VK_COMPARE_OP_ALWAYS, 0, 0, 0
  );

  VkPipelineDepthStencilStateCreateInfo ds_info = dlu_set_depth_stencil_state(
    VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL,
    VK_FALSE, VK_FALSE, back, back, 0, 0
  );

  VkPipelineMultisampleStateCreateInfo multisampling = dlu_set_multisample_state_info(
    VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0.0f, NULL, VK_FALSE, VK_FALSE
  );

  err = dlu_otba(DLU_GP_DATA_MEMS, app, cur_gpd, 1);
  check_err(!err, app, wc, NULL)

  err = dlu_create_graphics_pipelines(app, cur_gpd, 2, shader_stages,
    &vertex_input_info, &input_assembly, VK_NULL_HANDLE, &view_port_info,
    &rasterizer, &multisampling, &ds_info, &color_blending,
    &dynamic_state, 0, VK_NULL_HANDLE, UINT32_MAX
  );
  check_err(err, NULL, NULL, vert_shader_module)
  check_err(err, app, wc, frag_shader_module)

  dlu_log_me(DLU_SUCCESS, "Successfully created graphics pipeline");
  dlu_vk_destroy(DLU_DESTROY_VK_SHADER, app, frag_shader_module); frag_shader_module = VK_NULL_HANDLE;
  dlu_vk_destroy(DLU_DESTROY_VK_SHADER, app, vert_shader_module); vert_shader_module = VK_NULL_HANDLE;

  VkClearValue clear_values[2];
  float float32[4] = {0.2f, 0.2f, 0.2f, 0.2f};
  int32_t int32[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  uint32_t uint32[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  clear_values[0] = dlu_set_clear_value(float32, int32, uint32, 0.0f, 0);
  clear_values[1] = dlu_set_clear_value(float32, int32, uint32, 1.0f, 1);

  err = dlu_exec_begin_cmd_buffs(app, cur_pool, cur_scd, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, NULL);
  check_err(err, app, wc, NULL)

  /* Vertex buffer cannot be binded until we begin a renderpass */
  dlu_exec_begin_render_pass(app, cur_pool, cur_scd, cur_gpd, 0, 0, extent2D.width, extent2D.height, 2, clear_values, VK_SUBPASS_CONTENTS_INLINE);
  dlu_bind_pipeline(app, cur_pool, cur_buff, VK_PIPELINE_BIND_POINT_GRAPHICS, app->gp_data[cur_gpd].graphics_pipelines[0]);
  dlu_bind_desc_sets(app, cur_pool, cur_buff, cur_gpd, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 1, &app->desc_data[cur_dd].desc_set[0], 0, NULL);

  for (uint32_t i = 0; i < app->bdc; i++) {
    dlu_log_me(DLU_INFO, "app->buff_data[%d].name: %c", i, app->buff_data[i].name);
    dlu_log_me(DLU_INFO, "app->buff_data[%d].buff: %p - %p", i, &app->buff_data[i].buff, app->buff_data[i].buff);
  }

  const VkDeviceSize offsets[1] = {0};
  dlu_bind_vertex_buffs_to_cmd_buff(app, cur_pool, cur_buff, 0, 1, &app->buff_data[1].buff, offsets);
  dlu_cmd_set_viewport(app, &viewport, cur_pool, cur_buff, 0, 1);
  dlu_cmd_set_scissor(app, &scissor, cur_pool, cur_buff, 0, 1);

  const uint32_t vertex_count = ARR_LEN(vertices);
  dlu_cmd_draw(app, cur_pool, cur_buff, vertex_count, 1, 0, 0);

  dlu_exec_stop_render_pass(app, cur_pool, cur_scd);
  err = dlu_exec_stop_cmd_buffs(app, cur_pool, cur_scd);
  check_err(err, app, wc, NULL)

  VkPipelineStageFlags pipe_stage_flags[1] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  VkSemaphore acquire_sems[1] = {app->sc_data[cur_scd].syncs[0].sem.image};
  VkSemaphore render_sems[1] = {app->sc_data[cur_scd].syncs[0].sem.render};
  VkCommandBuffer cmd_buffs[1] = {app->cmd_data[cur_pool].cmd_buffs[cur_buff]};

  err = dlu_vk_sync(DLU_VK_RESET_RENDER_FENCE, app, cur_scd, 0);
  check_err(err, app, wc, NULL)

  err = dlu_queue_graphics_queue(app, cur_scd, 0, 1, cmd_buffs, 1, acquire_sems, pipe_stage_flags, 1, render_sems);
  check_err(err, app, wc, NULL)

  err = dlu_queue_present_queue(app, 1, render_sems, 1, &app->sc_data[cur_scd].swap_chain, &cur_buff, NULL);
  check_err(err, app, wc, NULL)

  sleep(1);
  FREEME(app, wc)

  return EXIT_SUCCESS;
}
