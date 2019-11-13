#!/usr/bin/env python
#
# Copyright 2019 GoPro Inc.
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

from .cmp import CompareBase

# TODO: share a base class with _CompareFingerprints
class _CompareCuePoints(CompareBase):

    def __init__(self, scene_func, points,
                 width=128,
                 height=128,
                 nb_keyframes=1,
                 keyframes_callback=None,
                 tolerance=0,
                 clear_color=(0.0, 0.0, 0.0, 1.0),
                 exercise_serialization=True,
                 exercise_dot=True,
                 **scene_kwargs):
        self._points = points
        self._width = width
        self._height = height
        self._nb_keyframes = nb_keyframes
        self._keyframes_callback = keyframes_callback
        self._tolerance = tolerance
        self._clear_color = clear_color
        self._scene_func = scene_func
        self._scene_kwargs = scene_kwargs
        self._exercise_serialization = exercise_serialization
        self._exercise_dot = exercise_dot

        # TODO: honor tolerance per color component
        assert self._tolerance == 0

    @staticmethod
    def serialize(data):
        ret = ''
        for color_points in data:
            color_strings = ['{}:{:08X}'.format(point_name, color) for point_name, color in sorted(color_points.items())]
            ret += ' '.join(color_strings) + '\n'
        return ret

    @staticmethod
    def deserialize(data):
        ret = []
        for line in data.splitlines():
            color_points = {}
            for color_kv in line.split():
                key, value = color_kv.split(':')
                color_points[key] = int(value, 16)
            ret.append(color_points)
        return ret

    @staticmethod
    def _pos_to_px(pos, width, height):
        x = int(round((pos[0] + 1.) / 2. * width))
        y = height - 1 - int(round((pos[1] + 1.) / 2. * height))
        x = min(max(x, 0), width - 1)
        y = min(max(y, 0), height - 1)
        return [x, y]

    # TODO: refactor with fingerprint
    def get_out_data(self):
        ret = self._scene_func(**self._scene_kwargs)
        width, height = self._width, self._height
        duration = ret['duration']
        scene = ret['scene']

        capture_buffer = bytearray(width * height * 4)
        viewer = ngl.Viewer()
        assert viewer.configure(offscreen=1, width=width, height=height,
                                clear_color=self._clear_color,
                                capture_buffer=capture_buffer,
                                ) == 0
        timescale = duration / float(self._nb_keyframes)

        if self._exercise_dot:
            assert scene.dot()

        if self._exercise_serialization:
            scene_str = scene.serialize()
            viewer.set_scene_from_string(scene_str)
        else:
            viewer.set_scene(scene)

        data = []
        for t_id in range(self._nb_keyframes):
            if self._keyframes_callback:
                self._keyframes_callback(t_id)
            viewer.draw(t_id * timescale)
            color_points = {}
            for point_name, (x, y) in self._points.items():
                pix_x, pix_y = self._pos_to_px((x, y), width, height)
                pos = (pix_y * width + pix_x) * 4
                c = capture_buffer[pos:pos + 4]
                color_points[point_name] = c[0]<<24 | c[1]<<16 | c[2]<<8 | c[3]
            data.append(color_points)
            if not self._exercise_serialization and self._exercise_dot:
                scene.dot()
        return data


def test_cuepoints(*args, **kwargs):
    def test_decorator(scene_func):
        scene_func.tester = _CompareCuePoints(scene_func, *args, **kwargs)
        return scene_func
    return test_decorator
