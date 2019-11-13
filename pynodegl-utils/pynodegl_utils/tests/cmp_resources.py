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

import os
import csv
import tempfile
import pynodegl as ngl

from .cmp import CompareBase


class _CompareResources(CompareBase):

    def __init__(self, scene_func, width=320, height=240, nb_keyframes=1,
                 columns = (
                     'Textures memory',
                     'Buffers count',
                     'Buffers total',
                     'Blocks count',
                     'Blocks total',
                     'Medias count',
                     'Medias total',
                     'Textures count',
                     'Textures total',
                     'Computes',
                     'GraphicCfgs',
                     'Renders',
                     'RTTs',
                 ), **scene_kwargs):
        self._width = width
        self._height = height
        self._nb_keyframes = nb_keyframes
        self._columns = columns
        self._scene_func = scene_func
        self._scene_kwargs = scene_kwargs

    # TODO: refactor with fingerprint
    def get_out_data(self):
        ret = self._scene_func(**self._scene_kwargs)

        width, height = self._width, self._height
        duration = ret['duration']
        scene = ret['scene']

        # We can't use NamedTemporaryFile because we may not be able to open it
        # twice on some systems
        fd, csvfile = tempfile.mkstemp(suffix='.csv', prefix='ngl-test-resources-')
        os.close(fd)

        scene = ngl.HUD(scene, export_filename=csvfile)

        # We exercise the serialization/deserialization/dot on purpose
        scene_str = scene.serialize()
        assert scene.dot()

        viewer = ngl.Viewer()
        assert viewer.configure(offscreen=1, width=width, height=height) == 0
        timescale = duration / float(self._nb_keyframes)
        viewer.set_scene_from_string(scene_str)
        for t_id in range(self._nb_keyframes):
            viewer.draw(t_id * timescale)

        del viewer

        # filter columns
        with open(csvfile) as csvfile_xx:
            reader = csv.DictReader(csvfile_xx)
            data = [self._columns]
            for row in reader:
                data.append([v for k, v in row.items() if k in self._columns])

        # rely on base string diff
        ret = ''
        for row in data:
            ret += ','.join(row) + '\n'

        os.remove(csvfile)

        return ret


def test_resources(*args, **kwargs):
    def test_decorator(scene_func):
        scene_func.tester = _CompareResources(scene_func, *args, **kwargs)
        return scene_func
    return test_decorator
