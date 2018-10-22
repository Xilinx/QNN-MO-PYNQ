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

# ignore the first 2 args, since Vivado HLS also passes -f tclname as args
set config_proj_name [lindex $argv 2]
puts "HLS project: $config_proj_name"
set config_platform [lindex $argv 3]
puts "target platform: $config_platform"
set config_hwsrcdir [lindex $argv 4]
puts "HW source dir: $config_hwsrcdir"
set config_swsrcdir "$::env(XILINX_QNN_ROOT)/network/sw"
puts "SW source dir: $config_swsrcdir"
set config_networksdir "$::env(XILINX_QNN_ROOT)/network"
puts "Networks source dir: $config_networksdir"
set config_qnnlibdirhls "$::env(XILINX_QNN_ROOT)/library/hls"
puts "QNN HLS library: $config_qnnlibdirhls"
set config_qnnlibdirhost "$::env(XILINX_QNN_ROOT)/library/host"
puts "QNN HLS library: $config_qnnlibdirhost"
set config_jsondir "$::env(XILINX_QNN_ROOT)/library/rapidjson/include"
set config_qnnlibdirdrv "$::env(XILINX_QNN_ROOT)/library/driver"
puts "QNN HLS library driver: $config_qnnlibdirdrv"

set overlay_json "$::env(XILINX_QNN_ROOT)/../bitstreams/$config_platform/${config_proj_name}-overlay.json"
puts "Overlay JSON description: $overlay_json"
set network_json [lindex $argv 7]
puts "Network JSON description: $network_json"

set config_toplevelfxn "BlackBoxJam"

set config_proj_part [lindex $argv 5]
set config_clkperiod [lindex $argv 6]

# set up project
open_project -reset $config_proj_name-$config_platform
add_files $config_hwsrcdir/top.cpp -cflags "-std=c++0x -I$config_qnnlibdirhls"
add_files -tb $config_swsrcdir/main.cpp -cflags "-std=c++0x -DNOZIP -O3 -I$config_qnnlibdirdrv -I$config_qnnlibdirhls -I$config_qnnlibdirhost -I$config_hwsrcdir -I$config_jsondir"
add_files -tb $config_qnnlibdirhost/general-utils.cpp -cflags "-std=c++0x -O3 -I$config_qnnlibdirdrv -I$config_qnnlibdirhls -I$config_qnnlibdirhost -I$config_hwsrcdir -I$config_jsondir"
add_files -tb $config_qnnlibdirhost/offload-utils.cpp -cflags "-std=c++0x -O3 -I$config_qnnlibdirdrv -I$config_qnnlibdirhls -I$config_qnnlibdirhost -I$config_hwsrcdir -I$config_jsondir"
add_files -tb $config_qnnlibdirhost/offload-adapter-sw.cpp -cflags "-std=c++0x -O3 -I$config_qnnlibdirdrv -I$config_qnnlibdirhls -I$config_qnnlibdirhost -I$config_hwsrcdir -I$config_jsondir"
add_files -tb $config_qnnlibdirhost/layers.cpp -cflags "-std=c++0x -O3 -I$config_qnnlibdirdrv -I$config_qnnlibdirhls -I$config_qnnlibdirhost -I$config_hwsrcdir -I$config_jsondir"
add_files -tb $config_qnnlibdirhost/network.cpp -cflags "-std=c++0x -O3 -I$config_qnnlibdirdrv -I$config_qnnlibdirhls -I$config_qnnlibdirhost -I$config_hwsrcdir -I$config_jsondir"
add_files -tb $config_qnnlibdirhost/jobber.cpp -cflags "-std=c++0x -O3 -I$config_qnnlibdirdrv -I$config_qnnlibdirhls -I$config_qnnlibdirhost -I$config_hwsrcdir -I$config_jsondir"


set_top $config_toplevelfxn
open_solution sol1
set_part $config_proj_part

# use 64-bit AXI MM addresses
config_interface -m_axi_addr64

csim_design -argv "-n $overlay_json -l $network_json" -compiler clang
# syntesize and export
create_clock -period $config_clkperiod -name default
csynth_design
export_design -format ip_catalog

exit 0
