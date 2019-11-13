#!/usr/bin/env python
# -*- coding: utf-8 -*-
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
from pynodegl_utils.tests.cmp_floats import test_floats
from pynodegl_utils.toolbox.easings import easing_split, easing_list, easing_names


@test_floats()
def anim_forward(nb_points=7):
    scale = 1. / float(nb_points)
    ret = []
    for easing, _ in easing_list:
        easing_name, easing_args = easing_split(easing)
        values = [ngl.easing_evaluate(easing_name, i * scale, easing_args) for i in range(nb_points + 1)]
        ret.append([easing_name] + values)
    return ret


@test_floats()
def anim_resolution(nb_points=7):
    scale = 1. / float(nb_points)
    ret = []
    for easing, _ in easing_list:
        easing_name, easing_args = easing_split(easing)
        try:
            values = [ngl.easing_solve(easing_name, i * scale, easing_args) for i in range(nb_points + 1)]
        except Exception as e:
            pass
        else:
            ret.append([easing_name] + values)
    return ret
