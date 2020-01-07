/*
 * Copyright 2016-2018 GoPro Inc.
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

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <vulkan/vulkan_core.h>

#include "darray.h"
#include "format.h"
#include "buffer.h"
#include "glcontext.h"
#include "glincludes.h"
#include "hmap.h"
#include "image.h"
#include "log.h"
#include "math_utils.h"
#include "memory.h"
#include "nodegl.h"
#include "nodes.h"
#include "pipeline.h"
#include "type.h"
#include "texture.h"
#include "topology.h"
#include "spirv.h"
#include "utils.h"

#if 0
static VkResult create_command_pool(struct pipeline *s, int family_id)
{
    struct glcontext *vk = s->gl;

    VkCommandPoolCreateInfo command_pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = family_id,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // XXX
    };

    return vkCreateCommandPool(vk->device, &command_pool_create_info, NULL, &s->command_pool);
}

static VkResult create_command_buffers(struct pipeline *s)
{
    struct glcontext *vk = s->gl;

    s->nb_command_buffers = vk->nb_framebuffers;
    s->command_buffers = ngli_calloc(s->nb_command_buffers, sizeof(*s->command_buffers));
    if (!s->command_buffers)
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    VkCommandBufferAllocateInfo command_buffers_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = s->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = s->nb_command_buffers,
    };

    VkResult ret = vkAllocateCommandBuffers(vk->device, &command_buffers_allocate_info,
                                            s->command_buffers);
    if (ret != VK_SUCCESS)
        return ret;

    return VK_SUCCESS;
}

static void destroy_pipeline(struct pipeline *s)
{
    struct glcontext *vk = s->gl;

    vkDeviceWaitIdle(vk->device);

    vkFreeCommandBuffers(vk->device, s->command_pool,
                         s->nb_command_buffers, s->command_buffers);
    ngli_free(s->command_buffers);
    vkDestroyPipeline(vk->device, s->vkpipeline, NULL);
}

static VkResult create_graphics_pipeline(struct pipeline *s, VkPipeline *pipeline_dstp)
{
    struct glcontext *vk = s->gl;
    struct pipeline_params *params = &s->params;

    /* Vertex input state */
    VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = s->nb_binds,
        .pVertexBindingDescriptions = s->nb_binds ? s->bind_descs : NULL,
        .vertexAttributeDescriptionCount = s->nb_binds,
        .pVertexAttributeDescriptions = s->nb_binds ? s->attr_descs : NULL,
    };

    /* Input Assembly State */
    VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = ngli_topology_get_vk_topology(params->topology),
    };

    /* Viewport */
    VkViewport viewport = {
        .width = vk->config.width,
        .height = vk->config.height,
        .minDepth = 0.f,
        .maxDepth = 1.f,
    };

    VkRect2D scissor = {
        .extent = vk->extent,
    };

    VkPipelineViewportStateCreateInfo viewport_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    /* Rasterization */
    VkPipelineRasterizationStateCreateInfo rasterization_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.f,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
    };

    /* Multisampling */
    VkPipelineMultisampleStateCreateInfo multisampling_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    const struct glstate *vkstate = &s->ctx->glstate;

    /* Depth & stencil */
#if 0
    VkPipelineDepthStencilStateCreateInfo depthstencil_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = vkstate->depth_test,
        .depthWriteEnable = 0,
        .depthCompareOp = vkstate->depth_func,
        .depthBoundsTestEnable = 0,
        .stencilTestEnable = vkstate->stencil_test,
        .front = 0,
        .back = 0,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 0.0f,
    };
#endif

    /* Blend */
    VkPipelineColorBlendAttachmentState colorblend_attachment_state = {
        .blendEnable = vkstate->blend,
        .srcColorBlendFactor = vkstate->blend_src_factor,
        .dstColorBlendFactor = vkstate->blend_dst_factor,
        .colorBlendOp = vkstate->blend_op,
        .srcAlphaBlendFactor = vkstate->blend_src_factor_a,
        .dstAlphaBlendFactor = vkstate->blend_dst_factor_a,
        .alphaBlendOp = vkstate->blend_op_a,
        .colorWriteMask = vkstate->color_write_mask,
    };

    VkPipelineColorBlendStateCreateInfo colorblend_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorblend_attachment_state,
    };

    /* Dynamic states */
    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_LINE_WIDTH,
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = NGLI_ARRAY_NB(dynamic_states),
        .pDynamicStates = dynamic_states,

    };

    const struct program_priv *program_priv = params->program->priv_data;
    const struct program *program = &program_priv->program;
    VkPipelineShaderStageCreateInfo shader_stage_create_info[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = program->shaders[NGLI_PROGRAM_SHADER_VERT].vkmodule,
            .pName = "main",
        },{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = program->shaders[NGLI_PROGRAM_SHADER_FRAG].vkmodule,
            .pName = "main",
        },
    };

    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = NGLI_ARRAY_NB(shader_stage_create_info),
        .pStages = shader_stage_create_info,
        .pVertexInputState = &vertex_input_state_create_info,
        .pInputAssemblyState = &input_assembly_state_create_info,
        .pViewportState = &viewport_state_create_info,
        .pRasterizationState = &rasterization_state_create_info,
        .pMultisampleState = &multisampling_state_create_info,
        .pDepthStencilState = NULL,
        .pColorBlendState = &colorblend_state_create_info,
        .pDynamicState = &dynamic_state_create_info,
        .layout = s->pipeline_layout,
        .renderPass = vk->render_pass,
        .subpass = 0,
    };

    return vkCreateGraphicsPipelines(vk->device, NULL, 1,
                                     &graphics_pipeline_create_info,
                                     NULL, pipeline_dstp);
}

static VkResult create_compute_pipeline(struct pipeline *s, VkPipeline *pipeline_dstp)
{
    struct glcontext *vk = s->gl;
    struct pipeline_params *params = &s->params;
    const struct program_priv *program_priv = params->program->priv_data;
    const struct program *program = &program_priv->program;

    VkPipelineShaderStageCreateInfo shader_stage_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = program->shaders[NGLI_PROGRAM_SHADER_COMP].vkmodule,
        .pName = "main",
    };

    VkComputePipelineCreateInfo compute_pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = shader_stage_create_info,
        .layout = s->pipeline_layout,
    };

    return vkCreateComputePipelines(vk->device, NULL, 1,
                                    &compute_pipeline_create_info,
                                    NULL, pipeline_dstp);
}

static int set_textures(struct pipeline *s)
{
    struct glcontext *vk = s->gl;

    const int nb_texture_pairs = ngli_darray_count(&s->texture_pairs);
    const struct nodeprograminfopair *pairs = ngli_darray_data(&s->texture_pairs);
    for (int i = 0; i < nb_texture_pairs; i++) {
        const struct nodeprograminfopair *pair = &pairs[i];
        struct textureprograminfo *info = pair->program_info;
        const struct ngl_node *tnode = pair->node;
        struct texture_priv *texture = tnode->priv_data;
        struct image *image = &texture->image;
        if (!image->layout)
            continue;

        const struct texture *plane = image->planes[0];
        if (info->binding >= 0) {
            VkDescriptorImageInfo image_info = {
                .imageLayout = plane->image_layout,
                .imageView = plane->image_view,
                .sampler = plane->image_sampler,
            };
            VkWriteDescriptorSet write_descriptor_set = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = s->descriptor_sets[vk->img_index],
                .dstBinding = info->binding,
                .dstArrayElement = 0,
                .descriptorType = info->is_sampler ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount = 1,
                .pImageInfo = &image_info,
            };
            vkUpdateDescriptorSets(vk->device, 1, &write_descriptor_set, 0, NULL);
        }
    }

    return 0;
}

static int set_uniforms(struct pipeline *s)
{
    const int nb_uniform_pairs = ngli_darray_count(&s->uniform_pairs);
    const struct nodeprograminfopair *uniform_pairs = ngli_darray_data(&s->uniform_pairs);

    const int nb_texture_pairs = ngli_darray_count(&s->texture_pairs);
    const struct nodeprograminfopair *texture_pairs = ngli_darray_data(&s->texture_pairs);

    if (!s->uniform_buffer.size || (!nb_uniform_pairs && !nb_texture_pairs))
        return 0;

    // FIXME: check uniform type and use its size instead of source size
    void *mapped_memory = ngli_buffer_map(&s->uniform_buffer);
    for (int i = 0; i < nb_uniform_pairs; i++) {
        const struct nodeprograminfopair *pair = &uniform_pairs[i];
        const int offset = (intptr_t)pair->program_info; // HACK / won't work with quaternion
        const struct ngl_node *unode = pair->node;
        void *datap = mapped_memory + offset;

        switch (unode->class->id) {
            case NGL_NODE_UNIFORMFLOAT: {
                const struct uniform_priv *u = unode->priv_data;
                *(float *)datap = (float)u->scalar;
                break;
            }
            case NGL_NODE_UNIFORMVEC2: {
                const struct uniform_priv *u = unode->priv_data;
                memcpy(datap, u->vector, 2 * sizeof(float));
                break;
            }
            case NGL_NODE_UNIFORMVEC3: {
                const struct uniform_priv *u = unode->priv_data;
                memcpy(datap, u->vector, 3 * sizeof(float));
                break;
            }
            case NGL_NODE_UNIFORMVEC4: {
                const struct uniform_priv *u = unode->priv_data;
                memcpy(datap, u->vector, 4 * sizeof(float));
                break;
            }
            default:
                LOG(ERROR, "unsupported uniform of type %s", unode->class->name);
                break;
        }
    }

    for (int i = 0; i < nb_texture_pairs; i++) {
        const struct nodeprograminfopair *pair = &texture_pairs[i];
        struct textureprograminfo *info = pair->program_info;
        const struct ngl_node *tnode = pair->node;
        const struct texture_priv *texture = tnode->priv_data;
        const struct image *image = &texture->image;
        /* FIXME */
        if (!image->layout)
            continue;
        const struct texture_params *params = &image->planes[0]->params;

        if (info->coord_matrix_offset >= 0) {
            void *datap = mapped_memory + info->coord_matrix_offset;
            memcpy(datap, image->coordinates_matrix, sizeof(image->coordinates_matrix));
        }
        if (info->dimensions_offset >= 0) {
            const float dimensions[] = {
                params->width,
                params->height,
            };
            void *datap = mapped_memory + info->dimensions_offset;
            memcpy(datap, dimensions, sizeof(dimensions));
        }
        if (info->ts_offset >= 0) {
            const float data_src_ts = image->ts;
            void *datap = mapped_memory + info->ts_offset;
            memcpy(datap, &data_src_ts, sizeof(data_src_ts));
        }
    }

    ngli_buffer_unmap(&s->uniform_buffer);

    return 0;
}

static VkDescriptorSetLayoutBinding *get_descriptor_layout_binding(struct darray *binding_descriptors, int binding)
{
    int nb_descriptors = ngli_darray_count(binding_descriptors);
    for (int i = 0; i < nb_descriptors; i++) {
        VkDescriptorSetLayoutBinding *descriptor = ngli_darray_get(binding_descriptors, i);
        if (descriptor->binding == binding) {
            return descriptor;
        }
    }
    return NULL;
}

static VkResult create_descriptor_layout_bindings(struct pipeline *s)
{
    struct pipeline_params *params = &s->params;

    ngli_darray_init(&s->binding_descriptors, sizeof(VkDescriptorSetLayoutBinding), 0);
    ngli_darray_init(&s->constant_descriptors, sizeof(VkPushConstantRange), 0);

    static const int stages_map[] = {
        VK_SHADER_STAGE_VERTEX_BIT,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        VK_SHADER_STAGE_COMPUTE_BIT,
    };

    struct program_priv *program_priv = params->program->priv_data;
    struct program *program = &program_priv->program;
    struct hmap *bindings_map[] = {
        program->shaders[NGLI_PROGRAM_SHADER_VERT].probe ? program->shaders[NGLI_PROGRAM_SHADER_VERT].probe->bindings : NULL,
        program->shaders[NGLI_PROGRAM_SHADER_FRAG].probe ? program->shaders[NGLI_PROGRAM_SHADER_FRAG].probe->bindings : NULL,
        program->shaders[NGLI_PROGRAM_SHADER_COMP].probe ? program->shaders[NGLI_PROGRAM_SHADER_COMP].probe->bindings : NULL,
    };

    // Create descriptor sets
    int constant_offset = 0;
    for (int i = 0; i < NGLI_ARRAY_NB(bindings_map); i++) {
        const struct hmap *bindings = bindings_map[i];
        if (!bindings)
            continue;

        const struct hmap_entry *binding_entry = NULL;
        while ((binding_entry = ngli_hmap_next(bindings, binding_entry))) {
            struct spirv_binding *binding = binding_entry->data;
            if ((binding->flag & NGLI_SHADER_CONSTANT)) {
                struct spirv_block *block = binding_entry->data;
                VkPushConstantRange descriptor = {
                    .stageFlags = stages_map[i],
                    .offset = constant_offset,
                    .size = block->size,
                };
                constant_offset = block->size;
                ngli_darray_push(&s->constant_descriptors, &descriptor);
            } else if ((binding->flag & NGLI_SHADER_UNIFORM)) {
                VkDescriptorSetLayoutBinding *descriptorp = get_descriptor_layout_binding(&s->binding_descriptors, binding->index);
                if (descriptorp) {
                    descriptorp->stageFlags |= stages_map[i];
                } else {
                    VkDescriptorSetLayoutBinding descriptor = {
                        .binding = binding->index,
                        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        .descriptorCount = 1,
                        .stageFlags = stages_map[i],
                    };
                    ngli_darray_push(&s->binding_descriptors, &descriptor);
                }
            } else if (binding->flag & NGLI_SHADER_STORAGE) {
                VkDescriptorSetLayoutBinding *descriptorp = get_descriptor_layout_binding(&s->binding_descriptors, binding->index);
                if (descriptorp) {
                    descriptorp->stageFlags |= stages_map[i];
                } else {
                    VkDescriptorSetLayoutBinding descriptor = {
                        .binding = binding->index,
                        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                        .descriptorCount = 1,
                        .stageFlags = stages_map[i],
                    };
                    ngli_darray_push(&s->binding_descriptors, &descriptor);
                }
            } else if (binding->flag & NGLI_SHADER_SAMPLER) {
                VkDescriptorSetLayoutBinding *descriptorp = get_descriptor_layout_binding(&s->binding_descriptors, binding->index);
                if (descriptorp) {
                    descriptorp->stageFlags |= stages_map[i];
                } else {
                    VkDescriptorSetLayoutBinding descriptor = {
                        .binding = binding->index,
                        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .descriptorCount = 1,
                        .stageFlags = stages_map[i],
                    };
                    ngli_darray_push(&s->binding_descriptors, &descriptor);
                }
            } else if (binding->flag & NGLI_SHADER_TEXTURE) {
                VkDescriptorSetLayoutBinding *descriptorp = get_descriptor_layout_binding(&s->binding_descriptors, binding->index);
                if (descriptorp) {
                    descriptorp->stageFlags |= stages_map[i];
                } else {
                    VkDescriptorSetLayoutBinding descriptor = {
                        .binding = binding->index,
                        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                        .descriptorCount = 1,
                        .stageFlags = stages_map[i],
                    };
                    ngli_darray_push(&s->binding_descriptors, &descriptor);
                }
            }
        }
    }

    return VK_SUCCESS;
}

static VkResult create_descriptor_sets(struct pipeline *s)
{
    struct glcontext *vk = s->gl;

    const int nb_bindings = ngli_darray_count(&s->binding_descriptors);
    if (nb_bindings) {
        static const VkDescriptorType types[] = {
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        };

        VkDescriptorPoolSize *descriptor_pool_sizes = ngli_calloc(NGLI_ARRAY_NB(types), sizeof(struct VkDescriptorPoolSize));
        if (!descriptor_pool_sizes)
            return -1;
        for (uint32_t i = 0; i < NGLI_ARRAY_NB(types); i++) {
            VkDescriptorPoolSize *descriptor_pool_size = &descriptor_pool_sizes[i];
            descriptor_pool_size->type = types[i];
            descriptor_pool_size->descriptorCount = 16; // FIXME:
        }

        VkDescriptorPoolCreateInfo descriptor_pool_create_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .poolSizeCount = NGLI_ARRAY_NB(types),
            .pPoolSizes = descriptor_pool_sizes,
            .maxSets = vk->nb_framebuffers,
        };

        // TODO: descriptor tool should be shared for all nodes
        VkResult vkret = vkCreateDescriptorPool(vk->device, &descriptor_pool_create_info, NULL, &s->descriptor_pool);
        ngli_free(descriptor_pool_sizes);
        if (vkret != VK_SUCCESS)
            return -1;

        VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = nb_bindings,
            .pBindings = (const VkDescriptorSetLayoutBinding *)ngli_darray_data(&s->binding_descriptors),
        };

        // create descriptor_set_layout
        vkret = vkCreateDescriptorSetLayout(vk->device, &descriptor_set_layout_create_info, NULL, &s->descriptor_set_layout);
        if (vkret != VK_SUCCESS)
            return -1;
        VkDescriptorSetLayout *descriptor_set_layouts = ngli_calloc(vk->nb_framebuffers, sizeof(*descriptor_set_layouts));
        for (uint32_t i = 0; i < vk->nb_framebuffers; i++) {
            descriptor_set_layouts[i] = s->descriptor_set_layout;
        }

        VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = s->descriptor_pool,
            .descriptorSetCount = vk->nb_framebuffers,
            .pSetLayouts = descriptor_set_layouts,
        };

        s->descriptor_sets = ngli_calloc(vk->nb_framebuffers, sizeof(*s->descriptor_sets));
        vkret = vkAllocateDescriptorSets(vk->device, &descriptor_set_allocate_info, s->descriptor_sets);
        ngli_free(descriptor_set_layouts);
        if (vkret != VK_SUCCESS)
            return -1;
    }

    return VK_SUCCESS;
}

static VkResult create_pipeline_layout(struct pipeline *s)
{
    struct glcontext *vk = s->gl;

    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    };

    const int nb_constants = ngli_darray_count(&s->constant_descriptors);
    if (nb_constants) {
        pipeline_layout_create_info.pushConstantRangeCount = nb_constants;
        pipeline_layout_create_info.pPushConstantRanges = (const VkPushConstantRange *)ngli_darray_data(&s->constant_descriptors);
    }

    if (s->descriptor_set_layout != VK_NULL_HANDLE) {
        pipeline_layout_create_info.setLayoutCount = 1;
        pipeline_layout_create_info.pSetLayouts = &s->descriptor_set_layout;
    }

    return vkCreatePipelineLayout(vk->device, &pipeline_layout_create_info, NULL, &s->pipeline_layout);
}

static void buffer_bind(struct glcontext *vk,
                        struct buffer *buffer,
                        struct pipeline *pipeline,
                        int offset,
                        int size,
                        int index,
                        int type)
{
    for (uint32_t i = 0; i < vk->nb_framebuffers; i++) {
        VkDescriptorBufferInfo descriptor_buffer_info = {
            .buffer = buffer->vkbuf,
            .offset = offset,
            .range = size,
        };
        VkWriteDescriptorSet write_descriptor_set = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = pipeline->descriptor_sets[i],
            .dstBinding = index,
            .dstArrayElement = 0,
            .descriptorType = type,
            .descriptorCount = 1,
            .pBufferInfo = &descriptor_buffer_info,
            .pImageInfo = NULL,
            .pTexelBufferView = NULL,
        };
        vkUpdateDescriptorSets(vk->device, 1, &write_descriptor_set, 0, NULL);
    }
}

static int pair_node_to_attribinfo(struct pipeline *s,
                                   struct darray *attribute_pairs,
                                   const char *name,
                                   struct ngl_node *anode)
{
    const struct pipeline_params *params = &s->params;
    const struct ngl_node *pnode = params->program;
    const struct program_priv *program_priv = pnode->priv_data;
    const struct program *program = &program_priv->program;
    const struct spirv_variable *active_attribute =
        ngli_hmap_get(program->shaders[NGLI_PROGRAM_SHADER_VERT].probe->attributes, name);

    if (!active_attribute)
        return 1;

#ifndef VULKAN_BACKEND
    /* FIXME */
    if (active_attribute->location < 0)
        return 0;
#endif

    int ret = ngli_node_buffer_ref(anode);
    if (ret < 0)
        return ret;

    struct nodeprograminfopair pair = {
        .node = anode,
        .program_info = (void *)active_attribute,
    };
    snprintf(pair.name, sizeof(pair.name), "%s", name);
    if (!ngli_darray_push(attribute_pairs, &pair)) {
        ngli_node_buffer_unref(anode);
        return -1;
    }

    return 0;
}

static int pair_nodes_to_attribinfo(struct pipeline *s,
                                    struct darray *attribute_pairs,
                                    struct hmap *attributes,
                                    int per_instance)
{
    if (!attributes)
        return 0;

    struct pipeline_params *params = &s->params;

    const struct hmap_entry *entry = NULL;
    while ((entry = ngli_hmap_next(attributes, entry))) {
        struct ngl_node *anode = entry->data;

        int ret = pair_node_to_attribinfo(s, attribute_pairs, entry->key, anode);
        if (ret < 0)
            return ret;

        const int warn_not_found = strcmp(entry->key, "ngl_position") &&
                                   strcmp(entry->key, "ngl_uvcoord") &&
                                   strcmp(entry->key, "ngl_normal");
        if (warn_not_found && ret == 1) {
            const struct ngl_node *pnode = params->program;
            LOG(WARNING, "attribute %s attached to %s not found in %s",
                entry->key, params->label, pnode->label);
        }
    }
    return 0;
}

static int create_vertex_input_attrib_desc(struct pipeline *s, const struct darray *attribute_pairs, int instance)
{
    struct glcontext *vk = s->gl;

    const struct nodeprograminfopair *pairs = ngli_darray_data(attribute_pairs);
    const int nb_pairs = ngli_darray_count(attribute_pairs);
    for (int i = 0; i < nb_pairs; i++) {
        const struct nodeprograminfopair *pair = &pairs[i];
        const struct spirv_variable *info = pair->program_info;
        struct buffer_priv *buffer = pair->node->priv_data;
        struct buffer *graphic_buffer = &buffer->buffer;
        uint32_t offset = 0;
        uint32_t stride = buffer->data_stride;
        if (buffer->block) {
            struct block_priv *block = buffer->block->priv_data;
            graphic_buffer = &block->buffer;

            const struct block_field_info *fi = &block->field_info[buffer->block_field];
            stride = fi->stride;
            offset = fi->offset;
        }

        VkVertexInputBindingDescription bind_desc = {
            .binding = s->nb_binds,
            .stride = stride,
            .inputRate = instance ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX,
        };

        VkFormat data_format = VK_FORMAT_UNDEFINED;
        int ret = ngli_format_get_vk_format(vk, buffer->data_format, &data_format);
        if (ret < 0)
            return ret;

        VkVertexInputAttributeDescription attr_desc = {
            .binding = s->nb_binds,
            .location = info->offset,
            .format = data_format,
            .offset = offset,
        };

        s->bind_descs[s->nb_binds] = bind_desc;
        s->attr_descs[s->nb_binds] = attr_desc;
        s->vkbufs[s->nb_binds] = graphic_buffer->vkbuf;
        s->nb_binds++;
    }

    return 0;
}

static int build_vertex_input_attrib_desc(struct pipeline *s)
{
    const int nb_attribute_pairs = ngli_darray_count(&s->attribute_pairs);
    const int nb_instance_attribute_pairs = ngli_darray_count(&s->instance_attribute_pairs);
    const int nb_attributes = nb_attribute_pairs + nb_instance_attribute_pairs;

    s->bind_descs     = ngli_calloc(nb_attributes, sizeof(*s->bind_descs));
    s->attr_descs     = ngli_calloc(nb_attributes, sizeof(*s->attr_descs));
    s->vkbufs         = ngli_calloc(nb_attributes, sizeof(*s->vkbufs));
    s->vkbufs_offsets = ngli_calloc(nb_attributes, sizeof(*s->vkbufs_offsets));
    if (!s->bind_descs || !s->attr_descs || !s->vkbufs || !s->vkbufs_offsets)
        return -1;

    int ret = create_vertex_input_attrib_desc(s, &s->attribute_pairs, 0);
    if (ret < 0)
        return ret;

    ret = create_vertex_input_attrib_desc(s, &s->instance_attribute_pairs, 1);
    if (ret < 0)
        return ret;

    return 0;
}

static int build_vertex_attribs_pairs(struct pipeline *s)
{
    struct pipeline_params *params = &s->params;

    ngli_darray_init(&s->attribute_pairs, sizeof(struct nodeprograminfopair), 0);
    ngli_darray_init(&s->instance_attribute_pairs, sizeof(struct nodeprograminfopair), 0);

    if (s->type != NGLI_PIPELINE_TYPE_GRAPHIC)
        return 0;

    int ret = pair_nodes_to_attribinfo(s, &s->attribute_pairs, params->attributes, 0);
    if (ret < 0)
        return ret;

    ret = pair_nodes_to_attribinfo(s, &s->instance_attribute_pairs, params->instance_attributes, 1);
    if (ret < 0)
        return ret;

    return 0;
}

int ngli_pipeline_init(struct pipeline *s, struct ngl_ctx *ctx, const struct pipeline_params *params)
{
    s->ctx = ctx;
    s->gl = ctx->glcontext;
    s->params = *params;
    s->type = params->program->class->id == NGL_NODE_PROGRAM ? NGLI_PIPELINE_TYPE_GRAPHIC
                                                             : NGLI_PIPELINE_TYPE_COMPUTE;

    struct glcontext *vk = s->gl;
    struct program_priv *program_priv = params->program->priv_data;
    struct program *program = &program_priv->program;

    ngli_darray_init(&s->uniform_pairs, sizeof(struct nodeprograminfopair), 0);
    ngli_darray_init(&s->texture_pairs, sizeof(struct nodeprograminfopair), 0);
    ngli_darray_init(&s->block_pairs, sizeof(struct nodeprograminfopair), 0);

    build_vertex_attribs_pairs(s);

    VkResult vkret = create_command_pool(s, s->queue_family_id);
    if (vkret != VK_SUCCESS)
        return -1;

    vkret = build_vertex_input_attrib_desc(s);
    if (vkret != VK_SUCCESS)
        return -1;

    vkret = create_descriptor_sets(s);
    if (vkret != VK_SUCCESS)
        return -1;

    vkret = create_descriptor_layout_bindings(s);
    if (vkret != VK_SUCCESS)
        return -1;

    vkret = create_descriptor_sets(s);
    if (vkret != VK_SUCCESS)
        return -1;

    vkret = create_pipeline_layout(s);
    if (vkret != VK_SUCCESS)
        return -1;

    const struct hmap *bindings_map[] = {
        program->shaders[NGLI_PROGRAM_SHADER_VERT].probe ? program->shaders[NGLI_PROGRAM_SHADER_VERT].probe->bindings : NULL,
        program->shaders[NGLI_PROGRAM_SHADER_FRAG].probe ? program->shaders[NGLI_PROGRAM_SHADER_FRAG].probe->bindings : NULL,
        program->shaders[NGLI_PROGRAM_SHADER_COMP].probe ? program->shaders[NGLI_PROGRAM_SHADER_COMP].probe->bindings : NULL,
    };

    // XXX:
    if (params->textures) {
        int nb_textures = ngli_hmap_count(params->textures) * NGLI_ARRAY_NB(bindings_map);
        s->textureprograminfos = ngli_calloc(nb_textures, sizeof(*s->textureprograminfos));
        if (!s->textureprograminfos)
            return -1;
    }

    // compute uniform buffer size needed
    // VkDeviceSize          minUniformBufferOffsetAlignment;
    // VkDeviceSize          minStorageBufferOffsetAlignment;
    int uniform_buffer_size = 0;
    for (int i = 0; i < NGLI_ARRAY_NB(bindings_map); i++) {
        const struct hmap *blocks = bindings_map[i];
        if (!blocks)
            continue;

        const struct hmap_entry *block_entry = NULL;
        while ((block_entry = ngli_hmap_next(blocks, block_entry))) {
            struct spirv_binding *binding = block_entry->data;
            if ((binding->flag & NGLI_SHADER_UNIFORM)) {
                struct spirv_block *block = block_entry->data;
                uniform_buffer_size += NGLI_ALIGN(block->size, 32);
            }
        }
    }


    if (uniform_buffer_size) {
        // allocate buffer
        int ret = ngli_buffer_init(&s->uniform_buffer,
                                   ctx,
                                   uniform_buffer_size,
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        if (ret < 0)
            return -1;
    }

    struct spirv_block *ngl_uniforms_blocks[] = {
        bindings_map[0] ? ngli_hmap_get(bindings_map[0], "ngl_uniforms") : NULL,
        bindings_map[1] ? ngli_hmap_get(bindings_map[1], "ngl_uniforms") : NULL,
        bindings_map[2] ? ngli_hmap_get(bindings_map[2], "ngl_uniforms") : NULL,
    };
    int ngl_uniforms_block_offsets[] = {
        0,
        0,
        0,
    };

    if (uniform_buffer_size) {
        // attach uniform buffers
        int uniform_block_offset = 0;
        for (int i = 0; i < NGLI_ARRAY_NB(bindings_map); i++) {
            const struct hmap *blocks = bindings_map[i];
            if (!blocks)
                continue;
            const struct hmap_entry *block_entry = NULL;
            while ((block_entry = ngli_hmap_next(blocks, block_entry))) {
                struct spirv_block *block = block_entry->data;
                struct spirv_binding *binding = block_entry->data;
                if (!strcmp(block_entry->key, "ngl_uniforms")) {
                    ngl_uniforms_block_offsets[i] = uniform_block_offset;
                }
                if (binding->flag & NGLI_SHADER_UNIFORM) {
                    int aligned_size = NGLI_ALIGN(block->size, 32); // wtf
                    buffer_bind(vk, &s->uniform_buffer, s, uniform_block_offset, aligned_size, binding->index, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
                    // TODO: map static uniforms directly
                    // fill uniform pairs
                    if (params->uniforms) {
                        const struct hmap_entry *variable_entry = NULL;
                        while ((variable_entry = ngli_hmap_next(block->variables, variable_entry))) {
                            struct ngl_node *unode = ngli_hmap_get(params->uniforms, variable_entry->key);
                            if (!unode)
                                continue;
                            const struct spirv_variable *variable = variable_entry->data;
                            intptr_t uniform_offset = uniform_block_offset + variable->offset;

                            struct nodeprograminfopair pair = {
                                .node = unode,
                                .program_info = (void *)uniform_offset,
                            };
                            snprintf(pair.name, sizeof(pair.name), "%s", variable_entry->key);
                            ngli_darray_push(&s->uniform_pairs, &pair);
                        }
                    }
                    uniform_block_offset += aligned_size;
                } else if (binding->flag & NGLI_SHADER_STORAGE) {
                    if (params->blocks) {
                        struct ngl_node *bnode = ngli_hmap_get(params->blocks, block_entry->key);
                        if (!bnode)
                            continue;
                        int ret = ngli_node_block_ref(bnode);
                        if (ret < 0)
                            return ret;
                        struct block_priv *block = bnode->priv_data;
                        struct buffer *graphic_buffer = &block->buffer;
                        buffer_bind(vk, graphic_buffer, s, 0, block->data_size, binding->index, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

                        struct nodeprograminfopair pair = {
                            .node = bnode,
                            .program_info = NULL, /* FIXME: ? */
                        };
                        snprintf(pair.name, sizeof(pair.name), "%s", block_entry->key);
                        ngli_darray_push(&s->block_pairs, &pair);
                    }
                }
            }
        }
    }

    if (params->textures) {
        const struct hmap_entry *entry = NULL;
        while ((entry = ngli_hmap_next(params->textures, entry))) {
            struct ngl_node *tnode = entry->data;

            /* FIXME: remove _sampler suffix or support both names */
            char name[128];
            snprintf(name, sizeof(name), "%s_sampler", entry->key);

            for (int i = 0; i < NGLI_ARRAY_NB(bindings_map); i++) {
                struct textureprograminfo *info = &s->textureprograminfos[s->nb_textureprograminfos];
                info->binding = -1;
                info->is_sampler = 0;
                info->coord_matrix_offset = -1;
                info->dimensions_offset = -1;
                info->ts_offset = -1;

                int submit_info = 0;

                const struct hmap *bindings = bindings_map[i];
                if (bindings) {
                    struct spirv_binding *binding = ngli_hmap_get(bindings, name);
                    if (binding && binding->flag & NGLI_SHADER_SAMPLER) {
                        info->binding = binding->index;
                        info->is_sampler = 1;
                        submit_info = 1;
                    /* FIXME: should be flagged as storage image */
                    } else if (binding && binding->flag & NGLI_SHADER_TEXTURE) {
                        info->binding = binding->index;
                        submit_info = 1;
                    }
                }

                struct spirv_block *block = ngl_uniforms_blocks[i];
                if (block) {
                    int block_offset = ngl_uniforms_block_offsets[i];

#define GET_UNIFORM_VARIABLE(name) do {                                              \
    char uniform_name[128];                                                          \
    snprintf(uniform_name, sizeof(uniform_name), "%s_" #name, entry->key);           \
    struct spirv_variable *variable = ngli_hmap_get(block->variables, uniform_name); \
    if (variable) {                                                                  \
        info->name ## _offset = block_offset + variable->offset;                     \
        submit_info = 1;                                                             \
    }                                                                                \
} while (0)

                    GET_UNIFORM_VARIABLE(coord_matrix);
                    GET_UNIFORM_VARIABLE(dimensions);
                    GET_UNIFORM_VARIABLE(ts);
                }

                if (submit_info) {
                    struct nodeprograminfopair pair = {
                        .node = tnode,
                        .program_info = info,
                    };
                    snprintf(pair.name, sizeof(pair.name), "%s", name);
                    ngli_darray_push(&s->texture_pairs, &pair);

                    s->nb_textureprograminfos++;
                }
            }
        }
    }

    /* FIXME: init pipeline here instead of in update */

    return 0;
}

#define NODE_TYPE_DEFAULT 0
#define NODE_TYPE_BLOCK   1
#define NODE_TYPE_BUFFER  2

static void reset_pairs(struct darray *p, int type)
{
    struct nodeprograminfopair *pairs = ngli_darray_data(p);
    for (int i = 0; i < ngli_darray_count(p); i++) {
        struct nodeprograminfopair *pair = &pairs[i];
        if (type == NODE_TYPE_BLOCK)
            ngli_node_block_unref(pair->node);
        else if (type == NODE_TYPE_BUFFER)
            ngli_node_buffer_unref(pair->node);
    }
    ngli_darray_reset(p);
}

#define DECLARE_RESET_PAIRS_FUNC(name, type)       \
static void reset_##name##_pairs(struct darray *p) \
{                                                  \
    reset_pairs(p, type);                          \
}                                                  \

DECLARE_RESET_PAIRS_FUNC(block, NODE_TYPE_BLOCK)
DECLARE_RESET_PAIRS_FUNC(buffer, NODE_TYPE_BUFFER)

void ngli_pipeline_uninit(struct pipeline *s)
{
    if (!s->gl)
        return;

    ngli_free(s->textureprograminfos);

    ngli_darray_reset(&s->texture_pairs);
    ngli_darray_reset(&s->uniform_pairs);
    reset_buffer_pairs(&s->attribute_pairs);
    reset_buffer_pairs(&s->instance_attribute_pairs);
    reset_block_pairs(&s->block_pairs);

    destroy_pipeline(s);

    struct glcontext *vk = s->gl;
    vkDestroyDescriptorSetLayout(vk->device, s->descriptor_set_layout, NULL);
    vkDestroyDescriptorPool(vk->device, s->descriptor_pool, NULL);
    ngli_free(s->descriptor_sets);
    vkDestroyPipelineLayout(vk->device, s->pipeline_layout, NULL);

    vkDestroyCommandPool(vk->device, s->command_pool, NULL);

    ngli_free(s->bind_descs);
    ngli_free(s->attr_descs);
    ngli_free(s->vkbufs);
    ngli_free(s->vkbufs_offsets);

    ngli_buffer_reset(&s->uniform_buffer);
}

static int update_pairs(struct darray *p, double t, int type)
{
    struct nodeprograminfopair *pairs = ngli_darray_data(p);
    for (int i = 0; i < ngli_darray_count(p); i++) {
        struct nodeprograminfopair *pair = &pairs[i];
        struct ngl_node *node = pair->node;
        int ret = ngli_node_update(node, t);
        if (ret < 0)
            return ret;
        if (type == NODE_TYPE_BLOCK)
            ret = ngli_node_block_upload(node);
        else if (type == NODE_TYPE_BUFFER)
            ret = ngli_node_buffer_upload(node);
        if (ret < 0)
            return ret;
    }
    return 0;
}

#define DECLARE_UPDATE_PAIRS_FUNC(name, type)                \
static int update_##name##_pairs(struct darray *p, double t) \
{                                                            \
    return update_pairs(p, t, type);                         \
}                                                            \

DECLARE_UPDATE_PAIRS_FUNC(common, NODE_TYPE_DEFAULT)
DECLARE_UPDATE_PAIRS_FUNC(block, NODE_TYPE_BLOCK)
DECLARE_UPDATE_PAIRS_FUNC(buffer, NODE_TYPE_BUFFER)

int ngli_pipeline_update(struct pipeline *s, double t)
{
    int ret;
    if ((ret = update_common_pairs(&s->texture_pairs, t)) < 0 ||
        (ret = update_common_pairs(&s->uniform_pairs, t)) < 0 ||
        (ret = update_block_pairs(&s->block_pairs, t)) < 0 ||
        (ret = update_buffer_pairs(&s->attribute_pairs, t)) < 0 ||
        (ret = update_buffer_pairs(&s->instance_attribute_pairs, t)) < 0)
        return ret;

    struct pipeline_params *params = &s->params;
    ret = ngli_node_update(params->program, t);
    if (ret < 0)
        return ret;

    struct glcontext *vk = s->gl;
    if ((s->last_width != vk->config.width ||
        s->last_height != vk->config.height)) {
        LOG(INFO, "reconfigure from %dx%d to %dx%d",
            s->last_width, s->last_height,
            vk->config.width, vk->config.height);

        destroy_pipeline(s);

        VkResult ret = create_command_buffers(s);
        if (ret != VK_SUCCESS)
            return ret;

        if (s->type == NGLI_PIPELINE_TYPE_GRAPHIC) {
            VkResult vret = create_graphics_pipeline(s, &s->vkpipeline);
            if (vret != VK_SUCCESS)
                return -1;
            s->bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
        } else {
            VkResult vret = create_compute_pipeline(s, &s->vkpipeline);
            if (vret != VK_SUCCESS)
                return -1;
            s->bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
        }

        s->last_width = vk->config.width;
        s->last_height = vk->config.height;
    }

    return 0;
}

int ngli_pipeline_bind(struct pipeline *s)
{
    int ret;

    // TODO: set_blocks
    if ((ret = set_uniforms(s)) < 0 ||
        (ret = set_textures(s)) < 0)
        return ret;

    struct glcontext *vk = s->gl;

    VkCommandBuffer cmd_buf = s->command_buffers[vk->img_index];
    vkCmdBindPipeline(cmd_buf, s->bind_point, s->vkpipeline);

    if (s->type == NGLI_PIPELINE_TYPE_GRAPHIC)
        vkCmdBindVertexBuffers(cmd_buf, 0, s->nb_binds, s->vkbufs, s->vkbufs_offsets);

    struct ngl_ctx *ctx = s->ctx;
    const size_t matrix_size = 4 * 4 * sizeof(float);
    const float *modelview_matrix = ngli_darray_tail(&ctx->modelview_matrix_stack);
    const float *projection_matrix = ngli_darray_tail(&ctx->projection_matrix_stack);

    if (s->type == NGLI_PIPELINE_TYPE_GRAPHIC) {
        vkCmdPushConstants(cmd_buf, s->pipeline_layout,
                        VK_SHADER_STAGE_VERTEX_BIT, 0, matrix_size,
                        modelview_matrix);
        vkCmdPushConstants(cmd_buf, s->pipeline_layout,
                        VK_SHADER_STAGE_VERTEX_BIT, matrix_size, matrix_size,
                        projection_matrix);
    }

    if (s->descriptor_sets) {
        vkCmdBindDescriptorSets(cmd_buf, s->bind_point, s->pipeline_layout,
                                0, 1, &s->descriptor_sets[vk->img_index], 0, NULL);
    }

    return 0;
}

int ngli_pipeline_unbind(struct pipeline *s)
{
    return 0;
}
#else

struct uniform_pair {
    uint64_t offset;
    uint64_t index;
    uint64_t size;
    struct pipeline_uniform uniform;
};

struct attribute_pair {
    struct pipeline_attribute attribute;
};

struct buffer_pair {
    struct pipeline_buffer buffer;
};

struct texture_pair {
    struct pipeline_texture texture;
};

static int build_attribute_pairs(struct pipeline *s, const struct pipeline_params *params)
{
    struct ngl_ctx *ctx = s->ctx;
    struct glcontext *vk = ctx->glcontext;

    ngli_darray_init(&s->attribute_descs, sizeof(VkVertexInputAttributeDescription), 0);
    ngli_darray_init(&s->vertex_binding_descs,   sizeof(VkVertexInputBindingDescription), 0);
    ngli_darray_init(&s->vertex_buffers, sizeof(VkBuffer), 0);
    ngli_darray_init(&s->vertex_offsets, sizeof(VkDeviceSize), 0);

    for (int i = 0; i < params->nb_attributes; i++) {
        const struct pipeline_attribute *attribute = &params->attributes[i];

        if (attribute->count > 4) {
            LOG(ERROR, "attribute count could not exceed 4");
            return NGL_ERROR_INVALID_ARG;
        }

        struct attribute_pair pair = {
            .attribute = *attribute,
        };
        if (!ngli_darray_push(&s->attribute_pairs, &pair))
            return NGL_ERROR_MEMORY;

        VkVertexInputBindingDescription binding_desc = {
            .binding   = ngli_darray_count(&s->vertex_buffers),
            .stride    = attribute->stride,
            .inputRate = attribute->rate ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX,
        };
        if (!ngli_darray_push(&s->vertex_binding_descs, &binding_desc))
            return NGL_ERROR_MEMORY;

        VkFormat format = VK_FORMAT_UNDEFINED;
        int ret = ngli_format_get_vk_format(vk, attribute->format, &format);
        if (ret < 0)
            return ret;

        VkVertexInputAttributeDescription attr_desc = {
            .binding  = ngli_darray_count(&s->vertex_buffers),
            .location = attribute->location,
            .format   = format,
            .offset   = attribute->offset,
        };
        if (!ngli_darray_push(&s->attribute_descs, &attr_desc))
            return NGL_ERROR_MEMORY;

        struct buffer *buffer = attribute->buffer;
        if (!ngli_darray_push(&s->vertex_buffers, &buffer->vkbuf))
            return NGL_ERROR_MEMORY;

        VkDeviceSize offset = 0;
        if (!ngli_darray_push(&s->vertex_offsets, &offset))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

static int pipeline_graphics_init(struct pipeline *s, const struct pipeline_params *params)
{
    struct ngl_ctx *ctx = s->ctx;
    struct glcontext *vk = ctx->glcontext;
    struct pipeline_graphics *graphics = &s->graphics;

    int ret = build_attribute_pairs(s, params);
    if (ret < 0)
        return ret;

    VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = ngli_darray_count(&s->vertex_binding_descs),
        .pVertexBindingDescriptions = ngli_darray_data(&s->vertex_binding_descs),
        .vertexAttributeDescriptionCount = ngli_darray_count(&s->attribute_descs),
        .pVertexAttributeDescriptions = ngli_darray_data(&s->attribute_descs),
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = ngli_topology_get_vk_topology(graphics->topology),
    };

    /* Viewport */
    VkViewport viewport = {
        .width = vk->config.width,
        .height = vk->config.height,
        .minDepth = 0.f,
        .maxDepth = 1.f,
    };

    VkRect2D scissor = {
        .extent = vk->extent,
    };

    VkPipelineViewportStateCreateInfo viewport_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    /* Rasterization */
    VkPipelineRasterizationStateCreateInfo rasterization_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.f,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
    };

    /* Multisampling */
    VkPipelineMultisampleStateCreateInfo multisampling_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    //const struct glstate *vkstate = &s->ctx->glstate;

    /* Depth & stencil */
#if 0
    VkPipelineDepthStencilStateCreateInfo depthstencil_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = vkstate->depth_test,
        .depthWriteEnable = 0,
        .depthCompareOp = vkstate->depth_func,
        .depthBoundsTestEnable = 0,
        .stencilTestEnable = vkstate->stencil_test,
        .front = 0,
        .back = 0,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 0.0f,
    };
#endif

    /* Blend */
    VkPipelineColorBlendAttachmentState colorblend_attachment_state = {
        /*
        .blendEnable = vkstate->blend,
        .srcColorBlendFactor = vkstate->blend_src_factor,
        .dstColorBlendFactor = vkstate->blend_dst_factor,
        .colorBlendOp = vkstate->blend_op,
        .srcAlphaBlendFactor = vkstate->blend_src_factor_a,
        .dstAlphaBlendFactor = vkstate->blend_dst_factor_a,
        .alphaBlendOp = vkstate->blend_op_a,
        .colorWriteMask = vkstate->color_write_mask,
        */
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE,
    };

    VkPipelineColorBlendStateCreateInfo colorblend_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorblend_attachment_state,
    };

    /* Dynamic states */
    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_LINE_WIDTH,
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = NGLI_ARRAY_NB(dynamic_states),
        .pDynamicStates = dynamic_states,

    };

    const struct program *program = s->program;
    VkPipelineShaderStageCreateInfo shader_stage_create_info[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = program->shaders[NGLI_PROGRAM_SHADER_VERT].vkmodule,
            .pName = "main",
        },{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = program->shaders[NGLI_PROGRAM_SHADER_FRAG].vkmodule,
            .pName = "main",
        },
    };

    VkGraphicsPipelineCreateInfo graphics_pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = NGLI_ARRAY_NB(shader_stage_create_info),
        .pStages = shader_stage_create_info,
        .pVertexInputState = &vertex_input_state_create_info,
        .pInputAssemblyState = &input_assembly_state_create_info,
        .pViewportState = &viewport_state_create_info,
        .pRasterizationState = &rasterization_state_create_info,
        .pMultisampleState = &multisampling_state_create_info,
        .pDepthStencilState = NULL,
        .pColorBlendState = &colorblend_state_create_info,
        .pDynamicState = &dynamic_state_create_info,
        .layout = s->pipeline_layout,
        .renderPass = vk->render_pass,
        .subpass = 0,
    };

    VkResult vkret = vkCreateGraphicsPipelines(vk->device, NULL, 1, &graphics_pipeline_create_info, NULL, &s->pipeline);
    if (vkret != VK_SUCCESS)
        return NGL_ERROR_EXTERNAL;

    s->bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;

    return 0;
}

static int pipeline_compute_init(struct pipeline *s)
{
    struct ngl_ctx *ctx = s->ctx;
    struct glcontext *vk = ctx->glcontext;

    const struct program *program = s->program;

    VkPipelineShaderStageCreateInfo shader_stage_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = program->shaders[NGLI_PROGRAM_SHADER_COMP].vkmodule,
        .pName = "main",
    };

    VkComputePipelineCreateInfo compute_pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = shader_stage_create_info,
        .layout = s->pipeline_layout,
    };

    VkResult vkret = vkCreateComputePipelines(vk->device, NULL, 1, &compute_pipeline_create_info, NULL, &s->pipeline);
    if (vkret != VK_SUCCESS)
        return NGL_ERROR_EXTERNAL;

    s->bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;

    return 0;
}

static const VkShaderStageFlags stage_flag_map[] = {
    [NGLI_PROGRAM_SHADER_VERT] = VK_SHADER_STAGE_VERTEX_BIT,
    [NGLI_PROGRAM_SHADER_FRAG] = VK_SHADER_STAGE_FRAGMENT_BIT,
    [NGLI_PROGRAM_SHADER_COMP] = VK_SHADER_STAGE_COMPUTE_BIT,
};

static VkShaderStageFlags get_stage_flags(int stages)
{
    VkShaderStageFlags flags = 0;
    if (stages & (1 << NGLI_PROGRAM_SHADER_VERT))
        flags |= VK_SHADER_STAGE_VERTEX_BIT;
    if (stages & (1 << NGLI_PROGRAM_SHADER_FRAG))
        flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (stages & (1 << NGLI_PROGRAM_SHADER_COMP))
        flags |= VK_SHADER_STAGE_COMPUTE_BIT;
    return flags;
}

static const VkDescriptorType descriptor_type_map[] = {
    [NGLI_TYPE_UNIFORM_BUFFER] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    [NGLI_TYPE_STORAGE_BUFFER] = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    [NGLI_TYPE_SAMPLER_2D]     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    [NGLI_TYPE_SAMPLER_3D]     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    [NGLI_TYPE_IMAGE_2D]       = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
};

static VkDescriptorType get_descriptor_type(int type)
{
    return descriptor_type_map[type];
}

static int create_desc_set_layout_bindings(struct pipeline *s, const struct pipeline_params *params)
{
    struct ngl_ctx *ctx = s->ctx;
    struct glcontext *vk = ctx->glcontext;

    ngli_darray_init(&s->desc_set_layout_bindings, sizeof(VkDescriptorSetLayoutBinding), 0);

    VkDescriptorPoolSize desc_pool_size_map[] = {
        [NGLI_TYPE_UNIFORM_BUFFER] = {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
        [NGLI_TYPE_STORAGE_BUFFER] = {.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER},
        [NGLI_TYPE_SAMPLER_2D]     = {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
        [NGLI_TYPE_IMAGE_2D]       = {.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
    };

    for (int i = 0; i < params->nb_buffers; i++) {
        const struct pipeline_buffer *pipeline_buffer = &params->buffers[i];

        const VkDescriptorType type = get_descriptor_type(pipeline_buffer->type);
        const VkDescriptorSetLayoutBinding binding = {
            .binding         = pipeline_buffer->binding,
            .descriptorType  = type,
            .descriptorCount = 1,
            .stageFlags      = VK_SHADER_STAGE_ALL,
        };
        if (!ngli_darray_push(&s->desc_set_layout_bindings, &binding))
            return NGL_ERROR_MEMORY;

        struct buffer_pair pair = {
            .buffer = *pipeline_buffer,
        };
        if (!ngli_darray_push(&s->buffer_pairs, &pair))
            return NGL_ERROR_MEMORY;

        desc_pool_size_map[pipeline_buffer->type].descriptorCount += vk->nb_framebuffers;
    }

    /* XXX */
    {
        struct hmap *blocks = s->program->buffer_blocks;
        struct program_variable_info *ngl_block = ngli_hmap_get(blocks, "ngl");
        if (ngl_block) {
            const VkDescriptorType type = get_descriptor_type(ngl_block->type);

            s->uniform_binding = ngl_block->binding;
            s->uniform_type = type;
            s->uniform_data = ngli_calloc(1, ngl_block->size);
            if (!s->uniform_data)
                return NGL_ERROR_MEMORY;

            const VkDescriptorSetLayoutBinding binding = {
                .binding         = s->uniform_binding,
                .descriptorType  = s->uniform_type,
                .descriptorCount = 1,
                .stageFlags      = VK_SHADER_STAGE_ALL,
            };
            if (!ngli_darray_push(&s->desc_set_layout_bindings, &binding))
                return NGL_ERROR_MEMORY;

            desc_pool_size_map[ngl_block->type].descriptorCount += vk->nb_framebuffers;

            int ret = ngli_buffer_init(&s->uniform_buffer, s->ctx, ngl_block->size, NGLI_BUFFER_USAGE_DYNAMIC);
            if (ret < 0)
                return ret;
        }
    }

    for (int i = 0; i < params->nb_textures; i++) {
        const struct pipeline_texture *pipeline_texture = &params->textures[i];

        const VkDescriptorType type = get_descriptor_type(pipeline_texture->type);
        const VkDescriptorSetLayoutBinding binding = {
            .binding         = pipeline_texture->binding,
            .descriptorType  = type,
            .descriptorCount = 1,
            .stageFlags      = VK_SHADER_STAGE_ALL,
        };
        if (!ngli_darray_push(&s->desc_set_layout_bindings, &binding))
            return NGL_ERROR_MEMORY;

        struct texture_pair pair = {
            .texture = *pipeline_texture,
        };
        if (!ngli_darray_push(&s->texture_pairs, &pair))
            return NGL_ERROR_MEMORY;

        desc_pool_size_map[pipeline_texture->type].descriptorCount += vk->nb_framebuffers;
    }

    struct VkDescriptorPoolSize desc_pool_sizes[NGLI_ARRAY_NB(desc_pool_size_map)];
    int nb_desc_pool_sizes = 0;
    for (int i = 0; i < NGLI_ARRAY_NB(desc_pool_size_map); i++) {
        if (desc_pool_size_map[i].descriptorCount) {
            desc_pool_sizes[nb_desc_pool_sizes++] = desc_pool_size_map[i];
        }
    }

    if (!nb_desc_pool_sizes)
        return 0;

    VkDescriptorPoolCreateInfo descriptor_pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = nb_desc_pool_sizes,
        .pPoolSizes = desc_pool_sizes,
        .maxSets = vk->nb_framebuffers,
    };

    VkResult vkret = vkCreateDescriptorPool(vk->device, &descriptor_pool_create_info, NULL, &s->desc_pool);
    if (vkret != VK_SUCCESS)
        return NGL_ERROR_EXTERNAL;

    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = ngli_darray_count(&s->desc_set_layout_bindings),
        .pBindings = (const VkDescriptorSetLayoutBinding *)ngli_darray_data(&s->desc_set_layout_bindings),
    };

    vkret = vkCreateDescriptorSetLayout(vk->device, &descriptor_set_layout_create_info, NULL, &s->desc_set_layout);
    if (vkret != VK_SUCCESS)
        return NGL_ERROR_EXTERNAL;

    VkDescriptorSetLayout *desc_set_layouts = ngli_calloc(vk->nb_framebuffers, sizeof(*desc_set_layouts));
    if (!desc_set_layouts)
        return NGL_ERROR_MEMORY;

    for (int i = 0; i < vk->nb_framebuffers; i++)
        desc_set_layouts[i] = s->desc_set_layout;

    VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = s->desc_pool,
        .descriptorSetCount = vk->nb_framebuffers,
        .pSetLayouts = desc_set_layouts
    };

    s->desc_sets = ngli_calloc(vk->nb_framebuffers, sizeof(*s->desc_sets));
    if (!s->desc_sets) {
        ngli_free(desc_set_layouts);
        return NGL_ERROR_MEMORY;
    }

    vkret = vkAllocateDescriptorSets(vk->device, &descriptor_set_allocate_info, s->desc_sets);
    if (vkret != VK_SUCCESS) {
        ngli_free(desc_set_layouts);
        return NGL_ERROR_EXTERNAL;
    }

    for (int i = 0; i < params->nb_buffers; i++) {
        const struct pipeline_buffer *pipeline_buffer = &params->buffers[i];
        struct buffer *buffer = pipeline_buffer->buffer;

        for (int i = 0; i < vk->nb_framebuffers; i++) {
            const VkDescriptorBufferInfo descriptor_buffer_info = {
                .buffer = buffer->vkbuf,
                .offset = 0,
                .range  = buffer->size,
            };
            const VkDescriptorType type = get_descriptor_type(pipeline_buffer->type);
            const VkWriteDescriptorSet write_descriptor_set = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = s->desc_sets[i],
                .dstBinding = pipeline_buffer->binding,
                .dstArrayElement = 0,
                .descriptorType = type,
                .descriptorCount = 1,
                .pBufferInfo = &descriptor_buffer_info,
                .pImageInfo = NULL,
                .pTexelBufferView = NULL,
            };
            vkUpdateDescriptorSets(vk->device, 1, &write_descriptor_set, 0, NULL);
        }
    }

    {
        for (int i = 0; i < vk->nb_framebuffers; i++) {
            if (s->uniform_data) {
                const VkDescriptorBufferInfo descriptor_buffer_info = {
                    .buffer = s->uniform_buffer.vkbuf,
                    .offset = 0,
                    .range  = s->uniform_buffer.size,
                };
                const VkWriteDescriptorSet write_descriptor_set = {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = s->desc_sets[i],
                    .dstBinding = s->uniform_binding,
                    .dstArrayElement = 0,
                    .descriptorType = s->uniform_type,
                    .descriptorCount = 1,
                    .pBufferInfo = &descriptor_buffer_info,
                    .pImageInfo = NULL,
                    .pTexelBufferView = NULL,
                };
                vkUpdateDescriptorSets(vk->device, 1, &write_descriptor_set, 0, NULL);
            }
        }
    }

    ngli_free(desc_set_layouts);
    return 0;
}

static int create_pipeline_layout(struct pipeline *s)
{
    struct ngl_ctx *ctx = s->ctx;
    struct glcontext *vk = ctx->glcontext;

    // FIXME: handle push constants
    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = s->desc_set_layout ? 1 : 0,
        .pSetLayouts    = &s->desc_set_layout,
    };

    VkResult vkret = vkCreatePipelineLayout(vk->device, &pipeline_layout_create_info, NULL, &s->pipeline_layout);
    if (vkret != VK_SUCCESS)
        return NGL_ERROR_EXTERNAL;

    return 0;
}

static int create_command_pool_and_buffers(struct pipeline *s)
{
    struct ngl_ctx *ctx = s->ctx;
    struct glcontext *vk = ctx->glcontext;

    VkCommandPoolCreateInfo command_pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = vk->queue_family_graphics_id,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, // FIXME
    };

    VkResult vkret = vkCreateCommandPool(vk->device, &command_pool_create_info, NULL, &s->command_pool);
    if (vkret != VK_SUCCESS)
        return NGL_ERROR_EXTERNAL;

    s->command_buffers = ngli_calloc(vk->nb_framebuffers, sizeof(*s->command_buffers));
    if (!s->command_buffers)
        return NGL_ERROR_MEMORY;

    VkCommandBufferAllocateInfo command_buffers_allocate_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = s->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = vk->nb_framebuffers,
    };

    VkResult ret = vkAllocateCommandBuffers(vk->device, &command_buffers_allocate_info, s->command_buffers);
    if (ret != VK_SUCCESS)
        return NGL_ERROR_EXTERNAL;

    return 0;
}

static const int type_size_map[NGLI_TYPE_NB] = {
    [NGLI_TYPE_INT]                         = sizeof(int),
    [NGLI_TYPE_IVEC2]                       = sizeof(int) * 2,
    [NGLI_TYPE_IVEC3]                       = sizeof(int) * 3,
    [NGLI_TYPE_IVEC4]                       = sizeof(int) * 4,
    [NGLI_TYPE_UINT]                        = sizeof(unsigned int),
    [NGLI_TYPE_UIVEC2]                      = sizeof(unsigned int) * 2,
    [NGLI_TYPE_UIVEC3]                      = sizeof(unsigned int) * 3,
    [NGLI_TYPE_UIVEC4]                      = sizeof(unsigned int) * 4,
    [NGLI_TYPE_FLOAT]                       = sizeof(float),
    [NGLI_TYPE_VEC2]                        = sizeof(float) * 2,
    [NGLI_TYPE_VEC3]                        = sizeof(float) * 3,
    [NGLI_TYPE_VEC4]                        = sizeof(float) * 4,
    [NGLI_TYPE_MAT3]                        = sizeof(float) * 3 * 3,
    [NGLI_TYPE_MAT4]                        = sizeof(float) * 4 * 4,
    [NGLI_TYPE_BOOL]                        = sizeof(int),
};

static int build_uniform_pairs(struct pipeline *s, const struct pipeline_params *params)
{
    const struct program *program = params->program;

    if (!program->uniforms)
        return 0;

    for (int i = 0; i < params->nb_uniforms; i++) {
        const struct pipeline_uniform *uniform = &params->uniforms[i];
        const struct program_variable_info *info = ngli_hmap_get(program->uniforms, uniform->name);
        if (!info)
            continue;

        if (uniform->type != info->type && (uniform->type != NGLI_TYPE_INT ||
            (info->type != NGLI_TYPE_BOOL && info->type != NGLI_TYPE_INT))) {
            LOG(ERROR, "uniform '%s' type does not match the type declared in the shader", uniform->name);
            return NGL_ERROR_INVALID_ARG;
        }

        struct uniform_pair pair = {
            .offset = info->offset,
            .index = info->index,
            .size = type_size_map[info->type],
            .uniform = *uniform,
        };
        if (!ngli_darray_push(&s->uniform_pairs, &pair))
            return NGL_ERROR_MEMORY;
    }

    return 0;
}

static int set_uniforms(struct pipeline *s)
{
    if (!s->uniform_data)
        return 0;

    struct uniform_pair *pairs = ngli_darray_data(&s->uniform_pairs);
    for (int i = 0; i < ngli_darray_count(&s->uniform_pairs); i++) {
        struct uniform_pair *pair = &pairs[i];
        struct pipeline_uniform *uniform = &pair->uniform;
        if (uniform->data)
            memcpy(s->uniform_data + pair->offset, uniform->data, pair->size);
    }

    ngli_buffer_upload(&s->uniform_buffer, s->uniform_data, s->uniform_buffer.size);

    return 0;
}

int ngli_pipeline_init(struct pipeline *s, struct ngl_ctx *ctx, const struct pipeline_params *params)
{
    int ret;

    s->ctx      = ctx;
    s->type     = params->type;
    s->graphics = params->graphics;
    s->compute  = params->compute;
    s->program  = params->program;

    ngli_darray_init(&s->uniform_pairs, sizeof(struct uniform_pair), 0);
    ngli_darray_init(&s->texture_pairs, sizeof(struct texture_pair), 0);
    ngli_darray_init(&s->buffer_pairs,  sizeof(struct buffer_pair), 0);
    ngli_darray_init(&s->attribute_pairs, sizeof(struct attribute_pair), 0);

    ret = create_command_pool_and_buffers(s);
    if (ret < 0)
        return ret;

    if ((ret = create_desc_set_layout_bindings(s, params)) < 0)
        return ret;

    ret = build_uniform_pairs(s, params);
    if (ret < 0)
        return ret;

    if ((ret = create_pipeline_layout(s)) < 0)
        return ret;

    if (params->type == NGLI_PIPELINE_TYPE_GRAPHICS) {
        ret = pipeline_graphics_init(s, params);
        if (ret < 0)
            return ret;
    } else if (params->type == NGLI_PIPELINE_TYPE_COMPUTE) {
        ret = pipeline_compute_init(s);
        if (ret < 0)
            return ret;
    } else {
        ngli_assert(0);
    }

    return 0;
}

int ngli_pipeline_get_uniform_index(struct pipeline *s, const char *name)
{
    struct uniform_pair *pairs = ngli_darray_data(&s->uniform_pairs);
    for (int i = 0; i < ngli_darray_count(&s->uniform_pairs); i++) {
        struct uniform_pair *pair = &pairs[i];
        struct pipeline_uniform *uniform = &pair->uniform;
        if (!strcmp(uniform->name, name))
            return i;
    }
    return NGL_ERROR_NOT_FOUND;
}

int ngli_pipeline_get_texture_index(struct pipeline *s, const char *name)
{
    struct pipeline_texture *pairs = ngli_darray_data(&s->texture_pairs);
    for (int i = 0; i < ngli_darray_count(&s->texture_pairs); i++) {
        struct pipeline_texture *pipeline_texture = &pairs[i];
        if (!strcmp(pipeline_texture->name, name))
            return i;
    }
    return NGL_ERROR_NOT_FOUND;
}

int ngli_pipeline_update_uniform(struct pipeline *s, int index, const void *data)
{
    if (index < 0)
        return NGL_ERROR_NOT_FOUND;

    ngli_assert(index < ngli_darray_count(&s->uniform_pairs));
    struct uniform_pair *pairs = ngli_darray_data(&s->uniform_pairs);
    struct uniform_pair *pair = &pairs[index];
    struct pipeline_uniform *pipeline_uniform = &pair->uniform;
    pipeline_uniform->data = data;

    return 0;
}

int ngli_pipeline_update_texture(struct pipeline *s, int index, struct texture *texture)
{
    struct ngl_ctx *ctx = s->ctx;
    struct glcontext *vk = ctx->glcontext;

    ngli_assert(index < ngli_darray_count(&s->texture_pairs));
    struct pipeline_texture *pairs = ngli_darray_data(&s->texture_pairs);
    struct pipeline_texture *pipeline_texture = &pairs[index];
    pipeline_texture->texture = texture;

    VkDescriptorImageInfo image_info = {
        .imageLayout = texture->image_layout,
        .imageView   = texture->image_view,
        .sampler     = texture->image_sampler,
    };
    VkWriteDescriptorSet write_descriptor_set = {
        .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet           = s->desc_sets[vk->img_index],
        .dstBinding       = pipeline_texture->binding,
        .dstArrayElement  = 0,
        .descriptorType   = get_descriptor_type(pipeline_texture->type),
        .descriptorCount  = 1,
        .pImageInfo       = &image_info,
    };
    vkUpdateDescriptorSets(vk->device, 1, &write_descriptor_set, 0, NULL);

    return 0;
}

static const VkIndexType vk_indices_type_map[NGLI_FORMAT_NB] = {
    [NGLI_FORMAT_R16_UNORM] = VK_INDEX_TYPE_UINT16,
    [NGLI_FORMAT_R32_UINT]  = VK_INDEX_TYPE_UINT32,
};

static VkIndexType get_vk_indices_type(int indices_format)
{
    return vk_indices_type_map[indices_format];
}

void ngli_pipeline_exec(struct pipeline *s)
{
    struct ngl_ctx *ctx = s->ctx;
    struct glcontext *vk = ctx->glcontext;

    set_uniforms(s);

    VkCommandBuffer cmd_buf = s->command_buffers[vk->img_index];

    VkCommandBufferBeginInfo command_buffer_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
    };

    VkResult ret = vkBeginCommandBuffer(cmd_buf, &command_buffer_begin_info);
    if (ret != VK_SUCCESS)
        return;

    vkCmdBindPipeline(cmd_buf, s->bind_point, s->pipeline);

    if (s->type == NGLI_PIPELINE_TYPE_GRAPHICS) {
    const float *rgba = vk->config.clear_color;
    VkClearValue clear_color = {.color.float32={rgba[0], rgba[1], rgba[2], rgba[3]}};

    VkRenderPassBeginInfo render_pass_begin_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = vk->render_pass,
        .framebuffer = vk->framebuffers[vk->img_index],
        .renderArea = {
            .extent = vk->extent,
        },
        .clearValueCount = 1,
        .pClearValues = &clear_color,
    };

    vkCmdBeginRenderPass(cmd_buf, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {
        .width = vk->config.width,
        .height = vk->config.height,
        .minDepth = 0.f,
        .maxDepth = 1.f,
    };
    vkCmdSetViewport(cmd_buf, 0, 1, &viewport);

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = {vk->config.width, vk->config.height},
    };
    vkCmdSetScissor(cmd_buf, 0, 1, &scissor);

    const int nb_vertex_buffers = ngli_darray_count(&s->vertex_buffers);
    VkBuffer *vertex_buffers = ngli_darray_data(&s->vertex_buffers);
    VkDeviceSize *vertex_offsets = ngli_darray_data(&s->vertex_offsets);
    vkCmdBindVertexBuffers(cmd_buf, 0, nb_vertex_buffers, vertex_buffers, vertex_offsets);

    if (s->desc_sets) {
        LOG(ERROR, "bind> %d", s->type == NGLI_PIPELINE_TYPE_GRAPHICS);
        vkCmdBindDescriptorSets(cmd_buf, s->bind_point, s->pipeline_layout, 0, 1, &s->desc_sets[vk->img_index], 0, NULL);
        LOG(ERROR, "bind<");
    }


    struct pipeline_graphics *graphics = &s->graphics;
    struct buffer *indices = graphics->indices;
    int nb_instances = graphics->nb_instances ? graphics->nb_instances : 1;
    if (indices) {
        VkIndexType indices_type = get_vk_indices_type(graphics->indices_format);
        vkCmdBindIndexBuffer(cmd_buf, indices->vkbuf, 0, indices_type);
        vkCmdDrawIndexed(cmd_buf, graphics->nb_indices, nb_instances, 0, 0, 0);
    } else {
        vkCmdDraw(cmd_buf, graphics->nb_vertices, nb_instances, 0, 0);
    }

    vkCmdEndRenderPass(cmd_buf);
    } else {
        struct pipeline_compute *compute = &s->compute;
    if (s->desc_sets) {
        LOG(ERROR, "bind> %d", s->type == NGLI_PIPELINE_TYPE_GRAPHICS);
        vkCmdBindDescriptorSets(cmd_buf, s->bind_point, s->pipeline_layout, 0, 1, &s->desc_sets[vk->img_index], 0, NULL);
        LOG(ERROR, "bind<");
    }

        vkCmdDispatch(cmd_buf, compute->nb_group_x, compute->nb_group_y, compute->nb_group_z);
    }

    ret = vkEndCommandBuffer(cmd_buf);
    if (ret != VK_SUCCESS)
        return;

    ngli_darray_push(&vk->command_buffers, &cmd_buf);



#if 0
    ///
    //    //
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *vk = ctx->glcontext;
    struct compute_priv *s = node->priv_data;
    struct pipeline *pipeline = &s->pipeline;

    VkCommandBuffer cmd_buf = pipeline->command_buffers[vk->img_index];

    VkCommandBufferBeginInfo command_buffer_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
    };

    VkResult vkret = vkBeginCommandBuffer(cmd_buf, &command_buffer_begin_info);
    if (vkret != VK_SUCCESS)
        return;

    int ret = ngli_pipeline_bind(&s->pipeline);
    if (ret < 0) {
        LOG(ERROR, "could not bind pipeline");
    }


    ret = ngli_pipeline_unbind(&s->pipeline);
    if (ret < 0) {
        LOG(ERROR, "could not unbind pipeline");
    }

    vkret = vkEndCommandBuffer(cmd_buf);
    if (vkret != VK_SUCCESS)
        return;

    vk->command_buffers[vk->nb_command_buffers++] = cmd_buf;
#endif
}

void ngli_pipeline_reset(struct pipeline *s)
{
    if (!s->ctx)
        return;

    struct ngl_ctx *ctx = s->ctx;
    struct glcontext *vk = ctx->glcontext;

    vkDeviceWaitIdle(vk->device);

    vkFreeCommandBuffers(vk->device, s->command_pool, vk->nb_framebuffers, s->command_buffers);
    ngli_free(s->command_buffers);
    vkDestroyCommandPool(vk->device, s->command_pool, NULL);

    vkDestroyDescriptorPool(vk->device, s->desc_pool, NULL);
    vkDestroyDescriptorSetLayout(vk->device, s->desc_set_layout, NULL);
    ngli_free(s->desc_sets);

    vkDestroyPipeline(vk->device, s->pipeline, NULL);
    vkDestroyPipelineLayout(vk->device, s->pipeline_layout, NULL);

    ngli_buffer_reset(&s->uniform_buffer);

    ngli_darray_reset(&s->attribute_descs);
    ngli_darray_reset(&s->vertex_binding_descs);
    ngli_darray_reset(&s->vertex_buffers);
    ngli_darray_reset(&s->vertex_offsets);
    ngli_darray_reset(&s->desc_set_layout_bindings);

    memset(s, 0, sizeof(*s));
}
#endif
