/*
 * Copyright 2019 GoPro Inc.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef PIPELINE_H
#define PIPELINE_H

#include "buffer.h"
#include "darray.h"
#include "program.h"
#include "texture.h"

struct ngl_ctx;
struct glcontext;

struct pipeline_uniform {
    char name[MAX_ID_LEN];
    int type;
    int count;
    int offset;
    int index;
    const void *data;
};

struct pipeline_texture {
    char name[MAX_ID_LEN];
    int type;
    int location;
    int binding;
    struct texture *texture;
};

struct pipeline_buffer {
    char name[MAX_ID_LEN];
    int binding;
    int type;
    struct buffer *buffer;
};

struct pipeline_attribute {
    char name[MAX_ID_LEN];
    int location;
    int format;
    int count;
    int stride;
    int offset;
    int rate;
    struct buffer *buffer;
};

struct pipeline_graphics {
    int topology;
    union {
        int nb_vertices;
        int nb_indices;
    };
    int indices_format;
    int nb_instances;
    struct buffer *indices;
};

struct pipeline_compute {
    int nb_group_x;
int nb_group_y;
    int nb_group_z;
};

enum {
    NGLI_PIPELINE_TYPE_GRAPHICS,
    NGLI_PIPELINE_TYPE_COMPUTE,
};

struct pipeline_params {
    int type;
    const struct pipeline_graphics graphics;
    const struct pipeline_compute compute;
    const struct program *program;

    const struct pipeline_texture *textures;
    int nb_textures;
    const struct pipeline_uniform *uniforms;
    int nb_uniforms;
    const struct pipeline_buffer *buffers;
    int nb_buffers;
    const struct pipeline_attribute *attributes;
    int nb_attributes;
};

struct pipeline {
    struct ngl_ctx *ctx;

    int type;
    struct pipeline_graphics graphics;
    struct pipeline_compute compute;
    const struct program *program;

    struct darray uniform_pairs;
    struct darray texture_pairs;
    struct darray buffer_pairs;
    struct darray attribute_pairs;

    void (*exec)(const struct pipeline *s, struct glcontext *gl);

#ifdef VULKAN_BACKEND
#if 0


    int last_width;
    int last_height;

    VkPipelineLayout pipeline_layout;
    VkPipeline vkpipeline;
    VkPipelineBindPoint bind_point;
    VkCommandBuffer *command_buffers;
    int nb_command_buffers; // XXX drop for vk->nb_framebuffers

    struct darray binding_descriptors;
    struct darray constant_descriptors;

    VkDescriptorPool descriptor_pool;
    VkDescriptorSetLayout descriptor_set_layout;
    VkDescriptorSet *descriptor_sets;

    VkVertexInputBindingDescription *bind_descs;
    VkVertexInputAttributeDescription *attr_descs;
    VkBuffer *vkbufs;
    VkDeviceSize *vkbufs_offsets;
    int nb_binds;

    struct buffer uniform_buffer;

    int flags;
#else
    struct darray attribute_descs;
    struct darray vertex_binding_descs;
    struct darray vertex_buffers;
    struct darray vertex_offsets;
    int nb_vertex_buffers;

    uint8_t *uniform_data;
    int uniform_binding;
    VkDescriptorType uniform_type;
    struct buffer uniform_buffer;

    VkDescriptorPool desc_pool;
    struct darray desc_set_layout_bindings;
    VkDescriptorSetLayout desc_set_layout;
    VkDescriptorSet *desc_sets;
    VkPipelineLayout pipeline_layout;
    VkPipeline pipeline;

    VkCommandPool command_pool;
    VkCommandBuffer *command_buffers;

    VkPipelineBindPoint bind_point;
#endif
#else
    uint64_t used_texture_units;
    GLuint vao_id;
#endif

};

int ngli_pipeline_init(struct pipeline *s, struct ngl_ctx *ctx, const struct pipeline_params *params);
int ngli_pipeline_get_uniform_index(struct pipeline *s, const char *name);
int ngli_pipeline_get_texture_index(struct pipeline *s, const char *name);
int ngli_pipeline_update_uniform(struct pipeline *s, int index, const void *value);
int ngli_pipeline_update_texture(struct pipeline *s, int index, struct texture *texture);
void ngli_pipeline_exec(struct pipeline *s);
void ngli_pipeline_reset(struct pipeline *s);

#endif
