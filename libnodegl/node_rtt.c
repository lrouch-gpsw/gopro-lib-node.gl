/*
 * Copyright 2016 GoPro Inc.
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

#include "fbo.h"
#include "format.h"
#include "log.h"
#include "nodegl.h"
#include "nodes.h"
#include "utils.h"

#define DEFAULT_CLEAR_COLOR {-1.0f, -1.0f, -1.0f, -1.0f}
#define FEATURE_DEPTH       (1 << 0)
#define FEATURE_STENCIL     (1 << 1)

static const struct param_choices feature_choices = {
    .name = "framebuffer_features",
    .consts = {
        {"depth",   FEATURE_DEPTH,   .desc=NGLI_DOCSTRING("depth")},
        {"stencil", FEATURE_STENCIL, .desc=NGLI_DOCSTRING("stencil")},
        {NULL}
    }
};

#define OFFSET(x) offsetof(struct rtt_priv, x)
static const struct node_param rtt_params[] = {
    {"child",         PARAM_TYPE_NODE, OFFSET(child),
                      .flags=PARAM_FLAG_CONSTRUCTOR,
                      .desc=NGLI_DOCSTRING("scene to be rasterized to `color_texture` and optionally to `depth_texture`")},
    {"color_texture", PARAM_TYPE_NODE, OFFSET(color_texture),
                      .flags=PARAM_FLAG_CONSTRUCTOR,
                      .node_types=(const int[]){NGL_NODE_TEXTURE2D, -1},
                      .desc=NGLI_DOCSTRING("destination color texture")},
    {"depth_texture", PARAM_TYPE_NODE, OFFSET(depth_texture),
                      .flags=PARAM_FLAG_DOT_DISPLAY_FIELDNAME,
                      .node_types=(const int[]){NGL_NODE_TEXTURE2D, -1},
                      .desc=NGLI_DOCSTRING("destination depth (and potentially combined stencil) texture")},
    {"samples",       PARAM_TYPE_INT, OFFSET(samples),
                      .desc=NGLI_DOCSTRING("number of samples used for multisampling anti-aliasing")},
    {"clear_color",   PARAM_TYPE_VEC4, OFFSET(clear_color), {.vec=DEFAULT_CLEAR_COLOR},
                      .desc=NGLI_DOCSTRING("color used to clear the `color_texture`")},
    {"features",      PARAM_TYPE_FLAGS, OFFSET(features),
                      .choices=&feature_choices,
                      .desc=NGLI_DOCSTRING("framebuffer feature mask")},
    {"vflip",         PARAM_TYPE_BOOL, OFFSET(vflip), {.i64=1},
                      .desc=NGLI_DOCSTRING("apply a vertical flip to `color_texture` and `depth_texture` transformation matrices to match the `node.gl` uv coordinates system")},
    {NULL}
};

static int has_stencil(int format)
{
    switch (format) {
    case NGLI_FORMAT_D24_UNORM_S8_UINT:
    case NGLI_FORMAT_D32_SFLOAT_S8_UINT:
        return 1;
    default:
        return 0;
    }
}

static int rtt_init(struct ngl_node *node)
{
    struct rtt_priv *s = node->priv_data;

    float clear_color[4] = DEFAULT_CLEAR_COLOR;
    s->use_clear_color = memcmp(s->clear_color, clear_color, sizeof(s->clear_color));

    return 0;
}

static int rtt_prefetch(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct rtt_priv *s = node->priv_data;
    struct texture_priv *texture = s->color_texture->priv_data;
    struct texture_priv *depth_texture = NULL;

    s->width = texture->width;
    s->height = texture->height;

    if (!(gl->features & NGLI_FEATURE_FRAMEBUFFER_OBJECT) &&
        s->samples > 0) {
        LOG(WARNING, "context does not support the framebuffer object feature, multisample anti-aliasing will be disabled");
        s->samples = 0;
    }

    if (s->depth_texture) {
        depth_texture = s->depth_texture->priv_data;
        if (s->width != depth_texture->width || s->height != depth_texture->height) {
            LOG(ERROR, "color and depth texture dimensions do not match: %dx%d != %dx%d",
                s->width, s->height, depth_texture->width, depth_texture->height);
            return -1;
        }
    }

    int ret;
    struct fbo *fbo = &s->fbo;
    if ((ret = ngli_fbo_init(fbo, gl, texture->width, texture->height, 0))          < 0 ||
        (ret = ngli_fbo_attach_texture(&s->fbo, texture->data_format, texture->id)) < 0)
        return ret;

    GLenum depth_format = NGLI_FORMAT_UNDEFINED;

    if (depth_texture) {
        depth_format = depth_texture->data_format;
        s->features |= FEATURE_DEPTH;
        s->features |= has_stencil(depth_format) ? FEATURE_STENCIL : 0;

        ret = ngli_fbo_attach_texture(&s->fbo, depth_format, depth_texture->id);
        if (ret < 0)
            return ret;
    } else {
        if (s->features & FEATURE_STENCIL)
            depth_format = NGLI_FORMAT_D24_UNORM_S8_UINT;
        else if (s->features & FEATURE_DEPTH)
            depth_format = NGLI_FORMAT_D16_UNORM;

        if (depth_format) {
            ret = ngli_fbo_create_renderbuffer(fbo, depth_format);
            if (ret < 0)
                return ret;
        }
    }

    ret = ngli_fbo_allocate(fbo);
    if (ret < 0)
        return ret;

    if (s->samples > 0) {
        struct fbo *fbo_ms = &s->fbo_ms;
        if ((ret = ngli_fbo_init(fbo_ms, gl, s->width, s->height, s->samples)) < 0 ||
            (ret = ngli_fbo_create_renderbuffer(fbo_ms, texture->data_format)) < 0)
            return ret;

        if ((s->features & FEATURE_DEPTH) || (s->features & FEATURE_STENCIL)) {
            ret = ngli_fbo_create_renderbuffer(fbo_ms, depth_format);
            if (ret < 0)
                return ret;
        }

        ret = ngli_fbo_allocate(fbo_ms);
        if (ret < 0)
            return ret;
    }

    if (s->vflip) {
        /* flip vertically the color and depth textures so the coordinates
         * match how the uv coordinates system works */
        texture->coordinates_matrix[5] = -1.0f;
        texture->coordinates_matrix[13] = 1.0f;

        if (depth_texture) {
            depth_texture->coordinates_matrix[5] = -1.0f;
            depth_texture->coordinates_matrix[13] = 1.0f;
        }
    }

    return 0;
}

static int rtt_update(struct ngl_node *node, double t)
{
    struct rtt_priv *s = node->priv_data;
    int ret = ngli_node_update(s->child, t);
    if (ret < 0)
        return ret;

    if (s->depth_texture) {
        ret = ngli_node_update(s->depth_texture, t);
        if (ret < 0)
            return ret;
    }

    return ngli_node_update(s->color_texture, t);
}

static void rtt_draw(struct ngl_node *node)
{
    struct ngl_ctx *ctx = node->ctx;
    struct glcontext *gl = ctx->glcontext;
    struct rtt_priv *s = node->priv_data;

    struct fbo *fbo = s->samples > 0 ? &s->fbo_ms : &s->fbo;
    int ret = ngli_fbo_bind(fbo);
    if (ret < 0)
        return;

    GLint viewport[4];
    ngli_glGetIntegerv(gl, GL_VIEWPORT, viewport);
    ngli_glViewport(gl, 0, 0, s->width, s->height);

    if (s->use_clear_color) {
        float *rgba = s->clear_color;
        ngli_glClearColor(gl, rgba[0], rgba[1], rgba[2], rgba[3]);
    }

    ngli_glClear(gl, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    ngli_node_draw(s->child);

    if (s->use_clear_color) {
        struct ngl_config *config = &ctx->config;
        float *rgba = config->clear_color;
        ngli_glClearColor(gl, rgba[0], rgba[1], rgba[2], rgba[3]);
    }

    if (s->samples > 0)
        ngli_fbo_blit(fbo, &s->fbo);

    ngli_fbo_invalidate_depth_buffers(fbo);
    ngli_fbo_unbind(fbo);

    ngli_glViewport(gl, viewport[0], viewport[1], viewport[2], viewport[3]);

    struct ngl_node *texture_node = s->color_texture;
    struct texture_priv *texture = s->color_texture->priv_data;

    if (ngli_node_texture_has_mipmap(texture_node)) {
        ngli_glBindTexture(gl, GL_TEXTURE_2D, texture->id);
        ngli_glGenerateMipmap(gl, GL_TEXTURE_2D);
    }

    if (s->vflip) {
        texture->coordinates_matrix[5] = -1.0f;
        texture->coordinates_matrix[13] = 1.0f;

        if (s->depth_texture) {
            struct texture_priv *depth_texture = s->depth_texture->priv_data;
            depth_texture->coordinates_matrix[5] = -1.0f;
            depth_texture->coordinates_matrix[13] = 1.0f;
        }
    }
}

static void rtt_release(struct ngl_node *node)
{
    struct rtt_priv *s = node->priv_data;

    ngli_fbo_reset(&s->fbo);
    ngli_fbo_reset(&s->fbo_ms);
}

const struct node_class ngli_rtt_class = {
    .id        = NGL_NODE_RENDERTOTEXTURE,
    .name      = "RenderToTexture",
    .init      = rtt_init,
    .prefetch  = rtt_prefetch,
    .update    = rtt_update,
    .draw      = rtt_draw,
    .release   = rtt_release,
    .priv_size = sizeof(struct rtt_priv),
    .params    = rtt_params,
    .file      = __FILE__,
};
