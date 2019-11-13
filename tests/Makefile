#
# Copyright 2017 GoPro Inc.
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

include ../common.mak

all: tests

# 1:category, 2:name
define DECLARE_TEST
test-$(1)-$(2): FUNC_NAME = $(1)_$(2)
TESTS_$(1) += test-$(1)-$(2)
endef

# 1:category, 2:namelist
define DECLARE_REF_TESTS
$(foreach X,$(2),$(eval $(call DECLARE_TEST,$(1),$(X))))
test-$(1): $$(TESTS_$(1))
.PHONY: test-$(1)
REF_TESTS += $$(TESTS_$(1))
$$(TESTS_$(1)): MODULE = $(1)
endef

# 1:category, 2:namelist
define DECLARE_SIMPLE_TESTS
$(foreach X,$(2),$(eval $(call DECLARE_TEST,$(1),$(X))))
test-$(1): $$(TESTS_$(1))
.PHONY: test-$(1)
SIMPLE_TESTS += $$(TESTS_$(1))
$$(TESTS_$(1)): MODULE = $(1)
endef

include anim.mak
include api.mak
include blending.mak
include data.mak
include live.mak
include media.mak
include shape.mak
include text.mak
include texture.mak
include transform.mak

$(REF_TESTS):
	@echo $@
	@ngl-test $(MODULE).py $(FUNC_NAME) refs/$(FUNC_NAME).ref > /dev/null

$(SIMPLE_TESTS):
	@echo $@
	@$(PYTHON) -c 'from $(MODULE) import $(FUNC_NAME); $(FUNC_NAME)()' #> /dev/null

TESTS = $(SIMPLE_TESTS) $(REF_TESTS)

tests: $(TESTS)

tests-list:
	@for test in $(TESTS); do echo $$test; done

.PHONY: all tests tests-list $(TESTS)
