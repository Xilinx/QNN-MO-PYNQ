#!/bin/bash
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

NETWORKS=$(ls -d W*A*/ | cut -f1 -d'/' | tr "\n" " ")

if [ "$#" -ne 3 ]; then
  echo "Usage: $0 <network> <platform> <mode>" >&2
  echo "where <network> = $NETWORKS" >&2
  echo "<platform> = pynqZ1-Z2 ultra96" >&2
  echo "<mode> = regenerate (h)ls only, (b)itstream only, (a)ll" >&2
  exit 1
fi

NETWORK=$1
PLATFORM=$2
MODE=$3
PATH_TO_VIVADO=$(which vivado)
PATH_TO_VIVADO_HLS=$(which vivado_hls)

if [ -z "$XILINX_QNN_ROOT" ]; then
    echo "Need to set XILINX_QNN_ROOT"
    exit 1
fi

if [ -z "$PATH_TO_VIVADO" ]; then
    echo "vivado not found in path"
    exit 1
fi

if [ -z "$PATH_TO_VIVADO_HLS" ]; then
    echo "vivado_hls not found in path"
    exit 1
fi


if [ -d "${XILINX_QNN_ROOT}/library/rapidjson" ]; then
	echo "rapidjson already cloned"
else	
	git clone https://github.com/Tencent/rapidjson.git ${XILINX_QNN_ROOT}/library/rapidjson
	git -C ${XILINX_QNN_ROOT}/library/rapidjson/ checkout tags/v1.1.0
fi


QNN_PATH=$XILINX_QNN_ROOT/network

HLS_SRC_DIR="$QNN_PATH/$NETWORK"
HLS_OUT_DIR="$QNN_PATH/output/hls-syn/$NETWORK-$PLATFORM"

HLS_SCRIPT=$QNN_PATH/hls-syn.tcl
HLS_IP_REPO="$HLS_OUT_DIR/sol1/impl/ip"

HLS_REPORT_PATH="$HLS_OUT_DIR/sol1/syn/report/BlackBoxJam_csynth.rpt"
REPORT_OUT_DIR="$QNN_PATH/output/report/$NETWORK-$PLATFORM"
VIVADO_HLS_LOG="$QNN_PATH/output/hls-syn/vivado_hls.log"

VIVADO_SCRIPT_DIR=$XILINX_QNN_ROOT/library/script/$PLATFORM
VIVADO_SCRIPT=$VIVADO_SCRIPT_DIR/make-vivado-proj.tcl

if [[ ("$PLATFORM" == "pynqZ1-Z2") ]]; then
  PLATFORM_PART="xc7z020clg400-1"
  TARGET_CLOCK="10"
elif [[ ("$PLATFORM" == "ultra96") ]]; then
  PLATFORM_PART="xczu3eg-sbva484-1-i"
  TARGET_CLOCK="5"
else
  echo "Error: Platform not supported. Please choose between pynqZ1-Z2 and ultra96."
  exit 1
fi



# We add a test network for each overlay (csim testbench in Vivado HLS)
if [[ ("$NETWORK" == "W1A2") ]]; then
  NETWORK_JSON=$XILINX_QNN_ROOT/../params/dorefanet-layers.json
elif [[ ("$NETWORK" == "W1A3") ]]; then
  NETWORK_JSON=$XILINX_QNN_ROOT/../params/tinier-yolo-layers.json
fi

# regenerate HLS jam if requested
if [[ ("$MODE" == "h") || ("$MODE" == "a")  ]]; then
  mkdir -p $HLS_OUT_DIR
  mkdir -p $REPORT_OUT_DIR
  OLDDIR=$(pwd)
  echo "Calling Vivado HLS for hardware synthesis..."
  cd $HLS_OUT_DIR/..
  vivado_hls -f $HLS_SCRIPT -tclargs $NETWORK $PLATFORM $HLS_SRC_DIR $PLATFORM_PART $TARGET_CLOCK $NETWORK_JSON
  if cat $VIVADO_HLS_LOG | grep "ERROR"; then
    echo "Error in Vivado_HLS"
    exit 1	
  fi
  if cat $VIVADO_HLS_LOG | grep "CRITICAL WARNING"; then
    echo "Critical warning in Vivado_HLS"
    exit 1	
  fi
  cat $HLS_REPORT_PATH | grep "Utilization Estimates" -A 20 > $REPORT_OUT_DIR/hls.txt
  cat $REPORT_OUT_DIR/hls.txt
  echo "HLS synthesis complete"
  echo "HLS-generated IP is at $HLS_IP_REPO"
  cd $OLDDIR
fi

# generate bitstream if requested

TARGET_NAME="$NETWORK-$PLATFORM"
VIVADO_OUT_DIR="$QNN_PATH/output/vivado/$TARGET_NAME"
BITSTREAM_PATH="$QNN_PATH/output/bitstream"
TARGET_BITSTREAM="$BITSTREAM_PATH/$NETWORK-$PLATFORM.bit"
TARGET_TCL="$BITSTREAM_PATH/$NETWORK-$PLATFORM.tcl"


if [[ ("$MODE" == "b") || ("$MODE" == "a")  ]]; then
  mkdir -p "$QNN_PATH/output/vivado"
  mkdir -p $BITSTREAM_PATH
  echo "Setting up Vivado project..."
  if [ -d "$VIVADO_OUT_DIR" ]; then
  read -p "Remove existing project at $VIVADO_OUT_DIR (y/n)? " -n 1 -r
  echo    # (optional) move to a new line
    if [[ $REPLY =~ ^[Nn]$ ]]
    then
      echo "Cancelled"
      exit 1
    fi
  rm -rf $VIVADO_OUT_DIR
  fi
  vivado -mode batch -source $VIVADO_SCRIPT -tclargs $HLS_IP_REPO $TARGET_NAME $VIVADO_OUT_DIR $VIVADO_SCRIPT_DIR
  cp -f "$VIVADO_OUT_DIR/$TARGET_NAME.runs/impl_1/procsys_wrapper.bit" $TARGET_BITSTREAM
  cp -f "$VIVADO_OUT_DIR/procsys.tcl" $TARGET_TCL

  # extract parts of the post-implementation reports
  cat "$VIVADO_OUT_DIR/$TARGET_NAME.runs/impl_1/procsys_wrapper_timing_summary_routed.rpt" | grep "| Design Timing Summary" -B 3 -A 10 > $REPORT_OUT_DIR/vivado.txt
  cat "$VIVADO_OUT_DIR/$TARGET_NAME.runs/impl_1/procsys_wrapper_utilization_placed.rpt" | grep "| Slice LUTs" -B 3 -A 11 >> $REPORT_OUT_DIR/vivado.txt
  cat "$VIVADO_OUT_DIR/$TARGET_NAME.runs/impl_1/procsys_wrapper_utilization_placed.rpt" | grep "| CLB LUTs" -B 3 -A 11 >> $REPORT_OUT_DIR/vivado.txt
  cat "$VIVADO_OUT_DIR/$TARGET_NAME.runs/impl_1/procsys_wrapper_utilization_placed.rpt" |  grep "| Block RAM Tile" -B 3 -A 5 >> $REPORT_OUT_DIR/vivado.txt
  cat "$VIVADO_OUT_DIR/$TARGET_NAME.runs/impl_1/procsys_wrapper_utilization_placed.rpt" |  grep "| DSPs" -B 3 -A 3 >> $REPORT_OUT_DIR/vivado.txt



  echo "Bitstream copied to $TARGET_BITSTREAM"
fi

echo "Done!"
