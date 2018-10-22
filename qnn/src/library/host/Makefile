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

XILINX_QNN_ROOT=$(abspath ../..)
MYSELF=$(abspath $(lastword $(MAKEFILE_LIST)))

CXXFLAGS += -std=c++11
CXXFLAGS += -O2
#CXXFLAGS += -ggdb

INCLUDES += -I$(XILINX_QNN_ROOT)/library/rapidjson/include
INCLUDES += -I$(XILINX_QNN_ROOT)/library/driver

ifdef VIVADOHLS_INCLUDE_PATH
INCLUDES += -I$(VIVADOHLS_INCLUDE_PATH)
endif

obj-o = $(patsubst %.cpp,%.o,$(wildcard *.cpp))
obj-abs = $(abspath $(obj))

.PHONY: all help clean

help:
	@echo "Compile object files"
	@echo ""
	@echo "Targets:"
	@echo "\t $(patsubst %.o,%.o\n\t,$(obj-o))"

all: $(obj-o)

$(obj-o) $(obj-abs): %.o: %.cpp
	$(CROSS_COMPILE)$(CXX) $(CXXFLAGS) $(DEFINES) $(INCLUDES) -c -o $@ $<

clean:
	@rm -f $(obj-o)
