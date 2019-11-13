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

import os
import sys

from pynodegl_utils.com import load_script


def run():
    gen_ref = os.environ.get('GEN')

    allowed_gen_ref = {None, 'force', 'yes'}
    if gen_ref not in allowed_gen_ref:
        sys.stderr.write('GEN environment variable must be any of {}\n'.format(allowed_gen_ref))
        sys.exit(1)

    if len(sys.argv) != 4:
        sys.stderr.write('Usage: {} <script_path> <func_name> <ref_filepath>\n'.format(sys.argv[0]))
        sys.exit(1)

    script_path, func_name, ref_filepath = sys.argv[1:4]

    module = load_script(script_path)
    func = getattr(module, func_name)
    tester = func.tester

    out_data = tester.get_out_data()
    if gen_ref == 'force':
        sys.stderr.write('Forced new ref for {}\n'.format(func_name))
        with open(ref_filepath, "w") as ref_file:
            ref_file.write(tester.serialize(out_data))
        ref_data = out_data
    else:
        with open(ref_filepath) as ref_file:
            serialized_data = ref_file.read()
        ref_data = tester.deserialize(serialized_data)

    err = []
    if len(ref_data) != len(out_data):
        err = ['{}: data len mismatch (ref:{} out:{})'.format(func_name, len(ref_data), len(out_data))]
    err += tester.compare_data(func_name, ref_data, out_data)

    if err:
        if gen_ref:
            sys.stderr.write('Re-generating ref for {}\n'.format(func_name))
            with open(ref_filepath, "w") as ref_file:
                ref_file.write(tester.serialize(out_data))
        else:
            sys.stderr.write('\n'.join(err) + '\n')
            sys.exit(1)

    sys.exit(0)
