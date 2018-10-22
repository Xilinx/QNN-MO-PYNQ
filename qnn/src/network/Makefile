#   Copyright (c) 2018, Xilinx, Inc.
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions are met:
#
#   1.  Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#
#   2.  Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#
#   3.  Neither the name of the copyright holder nor the names of its
#       contributors may be used to endorse or promote products derived from
#       this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
#   THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
#   PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
#   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
#   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
#   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
#   OR BUSINESS INTERRUPTION). HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
#   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
#   OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
#   ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

XILINX_QNN_ROOT=$(abspath ..)
MYSELF=$(abspath $(lastword $(MAKEFILE_LIST)))

#Want to add a new Network? Do it here:
app_sw_targets  = app_sw_W1A3
app_sw_targets += app_sw_W1A2
lib_sw_targets  = lib_sw_W1A3
lib_sw_targets += lib_sw_W1A2

lib_sw_targets_internal = $(foreach lib_target, $(lib_sw_targets), output/$(lib_target).so)
app_sw_targets_internal = $(foreach app_target, $(app_sw_targets), output/$(app_target).elf)

.PHONY: help .output_dir $(app_sw_targets) $(lib_sw_targets) app_hw lib_hw

lib_linking_hw  = -lcma
ifndef NOZIP
lib_linking 	= -lzip
else
lib_linking 	=
export DEFINES = -DNOZIP
endif

obj_linking  = $(XILINX_QNN_ROOT)/library/host/general-utils.o
obj_linking += $(XILINX_QNN_ROOT)/library/host/offload-utils.o
obj_linking += $(XILINX_QNN_ROOT)/library/host/network.o
obj_linking += $(XILINX_QNN_ROOT)/library/host/layers.o
obj_linking += $(XILINX_QNN_ROOT)/library/host/jobber.o

obj_linking_hw = $(XILINX_QNN_ROOT)/library/host/offload-adapter-hw.o
obj_linking_sw = $(XILINX_QNN_ROOT)/library/host/offload-adapter-sw.o

app = $(XILINX_QNN_ROOT)/network/sw/main.o

ifdef NETWORK
obj_linking_sw += $(XILINX_QNN_ROOT)/network/$(NETWORK)/top.o
endif

.PHONY: all clean help .output_dir $(app_sw_targets) $(lib_sw_targets)

help:
	@printf "Compile QNN Software\n\n"

	@printf "Targets:\n"
	@printf "\tapp_hw\n"
	@printf "\tlib_hw\n"
	@printf "%s\n" $(app_sw_targets) | sed 's/ /\n/g' | sed 's/^/\t/g'
	@printf "%s\n" $(lib_sw_targets) | sed 's/ /\n/g' | sed 's/^/\t/g'
	@printf "\n"

	@printf "Meta Targets:\n"
	@printf "\tall\n"
	@printf "\tclean\n"
	@printf "\treset_xlnk\n\n"

	@printf "Options:\n"
	@printf "\tCROSS_COMPILE\t\t- Set cross compiling prefix\n"
	@printf "\tVIVADOHLS_INCLUDE_PATH\t- Set HLS include path for sw implementations\n"
	@printf "\tNOZIP\t\t\t- Do not compile zip capabilites in\n\n"

	@printf "Requirements:\n"
	@printf "\trapidjson headers in $(XILINX_QNN_ROOT)/library/rapidjson/include/\n"

all:
	@$(MAKE) --no-print-directory app_hw
	@$(MAKE) --no-print-directory $(app_sw_targets)
	@$(MAKE) --no-print-directory lib_hw
	@$(MAKE) --no-print-directory $(lib_sw_targets)

.output_dir:
	@if [ ! -d $(XILINX_QNN_ROOT)/network/output/ ]; then mkdir -p $(XILINX_QNN_ROOT)/network/output/; fi

.rapidjson:
	@if [ ! -d $(XILINX_QNN_ROOT)/library/rapidjson ]; then \
		git clone https://github.com/Tencent/rapidjson.git $(XILINX_QNN_ROOT)/library/rapidjson; \
		git -C $(XILINX_QNN_ROOT)/library/rapidjson/ checkout tags/v1.1.0; \
	fi

$(obj_linking_hw) $(obj_linking_sw) $(obj_linking) $(app) $(miscs): %.o: %.cpp
	@$(MAKE) --no-print-directory -C $(dir $@) $(notdir $@)

$(app_sw_targets): export NETWORK = $(shell X=$@; echo $${X#app_sw_*})
app_hw $(app_sw_targets): .output_dir .rapidjson
	@if [ -e .SHARED ]; then $(MAKE) --no-print-directory .clean_part; fi
	@$(MAKE) --no-print-directory output/$@.elf

$(lib_sw_targets): export NETWORK = $(shell X=$@; echo $${X#lib_sw_*})
lib_hw $(lib_sw_targets): export CXXFLAGS = -fPIC -shared
lib_hw $(lib_sw_targets): .output_dir .rapidjson
	@if [ ! -e .SHARED ]; then $(MAKE) --no-print-directory .clean_part; fi
	@$(MAKE) --no-print-directory output/$@.so
	@touch .SHARED

$(app_sw_targets_internal) $(lib_sw_targets_internal): $(obj_linking) $(obj_linking_sw) $(app)
	$(CROSS_COMPILE)$(CXX) $(CXXFLAGS) -pthread -o $(XILINX_QNN_ROOT)/network/$@ $(app) $(obj_linking) $(obj_linking_sw) $(lib_linking)

output/app_hw.elf output/lib_hw.so : $(obj_linking) $(obj_linking_hw) $(app)
	$(CROSS_COMPILE)$(CXX) $(CXXFLAGS) -pthread -o $(XILINX_QNN_ROOT)/network/$@ $(app) $(obj_linking) $(obj_linking_hw) $(lib_linking) $(lib_linking_hw)


reset_xlnk:
	echo "import pynq.xlnk; xlnk = pynq.Xlnk(); xlnk.xlnk_reset();" | python3.6

.clean_part:
	@$(eval TARGETS := $(shell find $(XILINX_QNN_ROOT)/ -type f -name "Makefile" -not -path $(MYSELF)))
	@$(foreach target,$(TARGETS),$(MAKE) --no-print-directory -C $(dir $(target)) clean &&) true
	@rm -f .SHARED

clean: .clean_part
	@printf "Cleaning..."
	@rm -Rf $(XILINX_QNN_ROOT)/network/output/app_*
	@rm -Rf $(XILINX_QNN_ROOT)/network/output/lib_*
	@printf " done\n"
