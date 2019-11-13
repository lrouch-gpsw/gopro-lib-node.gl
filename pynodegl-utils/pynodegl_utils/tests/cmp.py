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

import difflib


class CompareBase:

    @staticmethod
    def serialize(data):
        return data

    @staticmethod
    def deserialize(data):
        return data

    def get_out_data(self):
        raise NotImplementedError

    def compare_data(self, test_name, ref_data, out_data):
        ref_data = self.serialize(ref_data)
        out_data = self.serialize(out_data)

        err = []
        if ref_data != out_data:
            ref_data = ref_data.splitlines(True)
            out_data = out_data.splitlines(True)
            diff = ''.join(difflib.unified_diff(ref_data, out_data, fromfile=test_name + '-ref', tofile=test_name + '-out', n=10))
            err.append('{} fail:\n{}'.format(test_name, diff))
        return err
