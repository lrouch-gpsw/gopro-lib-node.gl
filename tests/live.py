#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# Copyright 2020 GoPro Inc.
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

import pynodegl as ngl
from pynodegl_utils.misc import scene
from pynodegl_utils.tests.cmp_cuepoints import test_cuepoints

from pynodegl_utils.tests.data import (
    _ANIM_DURATION,
    _FUNCS,
    _gen_floats,
    _gen_ints,
    _get_debug_positions,
    _get_field,
    _get_random_block_info,
    _get_render,
)


def _get_live_spec(layout):
    livechange_f      = [[v] for v in _gen_floats(4)[1:3]]
    livechange_v2     = _gen_floats(2), _gen_floats(2)[::-1]
    livechange_v3     = _gen_floats(3), _gen_floats(3)[::-1]
    livechange_v4     = _gen_floats(4), _gen_floats(4)[::-1]
    livechange_i      = [[v] for v in _gen_ints(4)[1:3]]
    livechange_mat4   = _gen_floats(4 * 4), _gen_floats(4 * 4)[::-1]
    livechange_quat   = livechange_v4

    spec = [
        dict(name='f',  type='float',     category='single', livechange=livechange_f),
        dict(name='v2', type='vec2',      category='single', livechange=livechange_v2),
        dict(name='v3', type='vec3',      category='single', livechange=livechange_v3),
        dict(name='v4', type='vec4',      category='single', livechange=livechange_v4),
        dict(name='i',  type='int',       category='single', livechange=livechange_i),
        dict(name='m4', type='mat4',      category='single', livechange=livechange_mat4),
        dict(name='qm', type='quat_mat4', category='single', livechange=livechange_quat),
        dict(name='qv', type='quat_vec4', category='single', livechange=livechange_quat),
    ]

    for item in spec:
        item['func'] = _FUNCS['{category}_{type}'.format(**item)]

    return spec


def _live_scene(cfg, spec, field_id, seed, layout, debug_positions, color_tint):

    # duration set to 0 makes it always draw the same time, and we want that
    # FIXME: can not work with blocks because their update is not re-called
    cfg.duration = 0 if layout == 'uniform' else _ANIM_DURATION

    cfg.aspect_ratio = (1, 1)
    fields_info, block_fields, color_fields, block_definition, color_definition = _get_random_block_info(spec, seed, layout, color_tint=color_tint)
    fields = _get_field(fields_info, field_id)
    quad = ngl.Quad((-1, -1, 0), (2, 0, 0), (0, 2, 0))
    render = _get_render(cfg, quad, fields, block_definition, color_definition, block_fields, color_fields, layout, debug_positions=debug_positions)
    return render


def _get_live_function(field_id, layout):

    fields = _get_field(spec, field_id)
    assert len(fields) == 1
    field = fields[0]
    data_src = field['livechange']

    def keyframes_callback(t_id):
        if t_id:
            v = data_src[t_id - 1]
            field['node'].set_value(*v)

    @test_cuepoints(points=_get_debug_positions(spec, field_id),
                    nb_keyframes=len(data_src) + 1,
                    keyframes_callback=keyframes_callback,
                    exercise_serialization=False,
                    debug_positions=False)
    @scene(seed=scene.Range(range=[0, 100]), debug_positions=scene.Bool(), color_tint=scene.Bool())
    def scene_func(cfg, seed=0, debug_positions=True, color_tint=False):
        return _live_scene(cfg, spec, field_id, seed, layout, debug_positions, color_tint)
    return scene_func


for layout in {'std140', 'std430', 'uniform'}:
    spec = _get_live_spec(layout)
    for field_info in spec:
        field_id = '{category}_{type}'.format(**field_info)
        globals()['live_{}_{}'.format(field_id, layout)] = _get_live_function(field_id, layout)
