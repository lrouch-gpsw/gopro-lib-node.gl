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

from PIL import Image

import pynodegl as ngl

from .cmp import CompareBase

# TODO: move to class
_HSIZE = 8
_HNBITS = _HSIZE * _HSIZE * 2
_MODE = "RGBA"


class _CompareFingerprints(CompareBase):

    def __init__(self, scene_func, width=1280, height=800, nb_keyframes=1, tolerance=0, **scene_kwargs):
        self._width = width
        self._height = height
        self._nb_keyframes = nb_keyframes
        self._tolerance = tolerance
        self._scene_func = scene_func
        self._scene_kwargs = scene_kwargs

    @staticmethod
    def serialize(data):
        ret = ''
        for comp_hashes in data:
            ret += ' '.join(format(x, '032X') for x in comp_hashes) + '\n'
        return ret

    @staticmethod
    def deserialize(data):
        ret = []
        for line in data.splitlines():
            hashes = [int(x, 16) for x in line.split()]
            ret.append(hashes)
        return ret

    @staticmethod
    def _get_plane_hashes(buf):
        # XXX: dhash + different interleaving + splitted components
        hashes = []
        linesize = _HSIZE + 1
        comp_bufs = (buf[x::4] for x in range(4))  # R, G, B, A
        for comp_buf in comp_bufs:
            comp_hash = 0
            for y in range(_HSIZE):
                for x in range(_HSIZE):
                    pos = y * linesize + x
                    px_ref = comp_buf[pos]
                    px_xp1 = comp_buf[pos + 1]
                    px_yp1 = comp_buf[pos + linesize]
                    h_bit = px_ref < px_xp1
                    v_bit = px_ref < px_yp1
                    comp_hash = comp_hash << 2 | h_bit << 1 | v_bit
            hashes.append(comp_hash)
        return hashes

    def get_out_data(self):
        ret = self._scene_func(**self._scene_kwargs)
        width, height = self._width, self._height
        duration = ret['duration']
        scene = ret["scene"]

        # We exercise the serialization/deserialization on purpose
        scene_str = scene.serialize()
        assert scene.dot()

        # TODO: png output?
        capture_buffer = bytearray(width * height * 4)
        viewer = ngl.Viewer()
        assert viewer.configure(offscreen=1, width=width, height=height, capture_buffer=capture_buffer) == 0
        timescale = duration / float(self._nb_keyframes)
        viewer.set_scene_from_string(scene_str)
        hashes = []
        for t_id in range(self._nb_keyframes):
            viewer.draw(t_id * timescale)
            img = Image.frombuffer(_MODE, (width, height), capture_buffer, 'raw', _MODE, 0, 1)
            img = img.resize((_HSIZE + 1, _HSIZE + 1), resample=Image.LANCZOS)
            data = img.tobytes()
            h = self._get_plane_hashes(data)
            hashes.append(h)
        return hashes

    @staticmethod
    def _hash_repr(hash_val):
        linesize = _HSIZE + 1
        diff_chars = '.v>+'  # identical, vertical diff, horizontal diff, vertical+horizontal diff
        ret = ''
        for y in range(_HSIZE):
            line = ''
            for x in range(_HSIZE):
                pos = y * linesize + x
                bits = hash_val >> (pos * 2) & 0b11
                line += ' {}'.format(diff_chars[bits])
            ret += line + '\n'
        return ret

    def compare_data(self, test_name, ref_data, out_data):
        err = []
        for frame, (frame_ref_hashes, frame_out_hashes) in enumerate(zip(ref_data, out_data)):
            for comp, (ref_hash, out_hash) in enumerate(zip(frame_ref_hashes, frame_out_hashes)):
                hash_diff = ref_hash ^ out_hash
                bstring = '{:b}'.format(hash_diff)
                diff = bstring.count('1') * 100 // _HNBITS
                if diff > self._tolerance:
                    err.append('{} frame #{} Component {}: Diff too high ({}% > {}%)\n{:s}'.format(
                        test_name, frame, _MODE[comp], diff, self._tolerance, self._hash_repr(hash_diff)))

        return err


def test_fingerprint(*args, **kwargs):
    def test_decorator(scene_func):
        scene_func.tester = _CompareFingerprints(scene_func, *args, **kwargs)
        return scene_func
    return test_decorator
