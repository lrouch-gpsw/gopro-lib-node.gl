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


def easing_split(easing):
    name_split = easing.split(':')
    easing_name = name_split[0]
    args = [float(x) for x in name_split[1:]] if len(name_split) > 1 else None
    return easing_name, args


def easing_join(easing, args):
    return easing if not args else easing + ':' + ':'.join('%g' % x for x in args)


_easing_specs = (
    ('linear',    0, 1.),
    ('quadratic', 3, 1.),
    ('cubic',     3, 1.),
    ('quartic',   3, 1.),
    ('quintic',   3, 1.),
    ('power:7.3', 3, 1.),
    ('sinus',     3, 1.),
    ('exp',       3, 1.),
    ('circular',  3, 1.),
    ('bounce',    1, 1.),
    ('elastic',   1, 0.5),
    ('back',      3, 0.7),
)


def _get_easing_list():
    easings = []
    for col, (easing, flags, zoom) in enumerate(_easing_specs):
        versions = []
        if flags & 1:
            versions += ['_in', '_out']
        if flags & 2:
            versions += ['_in_out', '_out_in']
        if not flags:
            versions = ['']

        for version in versions:
            base_name, args = easing_split(easing)
            easing_name = easing_join(base_name + version, args)
            easings.append((easing_name, zoom))
    return easings


easing_list = _get_easing_list()
easing_names = [e[0] for e in easing_list]
