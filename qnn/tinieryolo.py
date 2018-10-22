#!/usr/bin/env python
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

import os, platform
import json
import numpy as np
import cv2
import cffi
import math
from pynq import Overlay, Xlnk
import subprocess
import time

if os.environ['BOARD'] == 'Ultra96':
	PLATFORM="ultra96"
elif os.environ['BOARD'] == 'Pynq-Z1' or os.environ['BOARD'] == 'Pynq-Z2':
	PLATFORM="pynqZ1-Z2"
else:
	raise RuntimeError("Board not supported")

QNN_ROOT_DIR = os.path.dirname(os.path.realpath(__file__))
BITSTREAM_NAME="W1A3-{0}.bit".format(PLATFORM)
TINIER_YOLO_BIT_FILE = os.path.join(QNN_ROOT_DIR,"bitstreams", PLATFORM, BITSTREAM_NAME)
CONV_WEIGHTS_FOLDER = os.path.join(QNN_ROOT_DIR,"params/binparam-tinier-yolo-nopool/")
HW_LIBPATH = os.path.join(QNN_ROOT_DIR, "libraries", PLATFORM, "lib_hw.so")
SW_LIBPATH = os.path.join(QNN_ROOT_DIR, "libraries", PLATFORM, "lib_sw_W1A3.so")
W1A3_JSON = os.path.join(QNN_ROOT_DIR, "bitstreams", PLATFORM, "W1A3-overlay.json")
JSON_TINIER_YOLO = os.path.join(QNN_ROOT_DIR,"params/tinier-yolo-layers.json")


RUNTIME_HW = "python_hw"
RUNTIME_SW = "python_sw"

net = dict()
init = False

_ffi = cffi.FFI()

_ffi.cdef("void initParameters(unsigned int const batch, unsigned int const threads);")
_ffi.cdef("void initAccelerator(char const *networkJson, char const *layerJson);")
_ffi.cdef("void singleInference(char *in, size_t const inSize, char *out, size_t const outSize);")
_ffi.cdef("void deinitAccelerator();")

_libraries = {}

class TinierYolo:

    def __init__(self, runtime=RUNTIME_HW):
        self.init = False
        if runtime == RUNTIME_HW:
            self.lib = _ffi.dlopen(HW_LIBPATH)
        else:
            self.lib = _ffi.dlopen(SW_LIBPATH)

    def load_network(self, json_layer=JSON_TINIER_YOLO):
        """ Parse network topology from JSON description. """

        with open(json_layer, 'r') as f:
            layer_json = json.load(f)

        # Create a new dictionary with conv and fc layers only
        for layer in layer_json['layers']:
            name = layer['name']
            layer.pop('name', None)
            net[name] = layer

        return net


    def init_accelerator(self, bit_file=TINIER_YOLO_BIT_FILE, json_network=W1A3_JSON, json_layer=JSON_TINIER_YOLO):
        """ Initialize accelerator memory and configuration. """

        if bit_file :
            ol = Overlay(bit_file)
            ol.download()

        if not self.init:
            with open(json_network, 'r') as f:
                self.network_json = json.load(f)

            with open(json_layer, 'r') as f:
                self.layer_json = json.load(f)

            self.lib.initParameters(1,0);
            self.lib.initAccelerator(json_network.encode(), json_layer.encode())
            self.init = True

    def inference(self, img, out):
        """ Load input image into accelerator memory. """

        if not self.init:
            raise IOError("Hardware need to be initialized before inference!")

        ffi = cffi.FFI()
        img_p = ffi.cast('char *', ffi.from_buffer(img))
        out_p = ffi.cast('char *', ffi.from_buffer(out))

        self.lib.singleInference(img_p, img.nbytes, out_p, out.nbytes);

    def deinit_accelerator(self):
        """ De-allocate accelerator memory. """

        if self.init:
            self.lib.deinitAccelerator()
            self.init = False

    def get_accel_buffer(self, channels, dim):
        if not self.init:
            raise IOError("Hardware need to be initialized for buffer preparation!")

        return np.zeros((dim,dim,channels), dtype=np.uint8)

    def prepare_buffer(self, buffer):
        if not self.init:
            raise IOError("Hardware need to be initialized for buffer preparation!")

        ACTIVATION_BITS = self.network_json['parameters']['ACTIVATION_BITS']

        if (ACTIVATION_BITS > 8):
            raise IOError("prepare buffer algorithm cannot handle more than 8 activation bits!")

        buffer = buffer.astype(np.uint8)
        dim = buffer.shape[1]
        #change shape to (dim,dim,chan)
        buffer = np.rollaxis(buffer,0,3)
        #transform channels to bits
        buffer = np.unpackbits(buffer, 2)
        #reshape to so that the fourth dimension always is one byte in bits
        buffer = buffer.reshape(dim, dim , -1, 8)
        #remove all the zero bits that we do not need, activation bits are left over
        buffer = np.delete(buffer, np.s_[0:(8-ACTIVATION_BITS)], 3)
        #flip left over bits, to fix endianness
        buffer = np.flip(buffer,3)
        #shape back to (dim, dim, chans in bits)
        buffer = np.reshape(buffer,(dim, dim, -1))
        #pad channels to multiple of 8
        if (buffer.shape[2] % 8):
            buffer = np.pad(buffer, [(0, 0), (0, 0), (0, 8 - (buffer.shape[2] % 8))], mode='constant')

        #fix endianness in 8 bits blocks
        buffer = np.reshape(buffer, (dim, dim, -1, 8))
        buffer = np.flip(buffer, 3)
        #pack bits together
        return np.packbits(buffer.reshape(dim, dim, -1), 2)

    def postprocess_buffer(self, buffer):
        if not self.init:
            raise IOError("Hardware need to be initialized for buffer preparation!")

        ACTIVATION_BITS = self.network_json['parameters']['ACTIVATION_BITS']
        if (ACTIVATION_BITS > 8):
            raise IOError("prepare buffer algorithm cannot handle more than 8 activation bits!")

        dim = buffer.shape[0]
        channels = buffer.shape[2]
        ele= math.ceil((channels * ACTIVATION_BITS) / 8)

        #delete entries that we don't need
        buffer = np.delete(buffer, np.s_[ele:], 2)
        #unpack bits
        buffer = np.unpackbits(buffer, 2)
        #fix endianness in 8 bits blocks
        buffer = buffer.reshape(dim,dim,-1,8)
        buffer = np.flip(buffer, 3)
        #delete bits that are left over
        if ((channels * ACTIVATION_BITS) % 8) != 0:
            buffer = np.delete(buffer, np.s_[channels * ACTIVATION_BITS:], 2)
        #reshape to that every channel value has its own value
        buffer = buffer.reshape(dim,dim,-1, ACTIVATION_BITS)
        #fix endianness of the single values
        buffer = np.flip(buffer,3)
        #packbits will append zeros at the end, so put them in front
        buffer = np.pad(buffer, [(0,0),(0,0),(0,0),((8-ACTIVATION_BITS),0)], mode='constant')
        #pack the bits to values again
        buffer = np.packbits(buffer,3)
        #fc layers are not intereseted in the shape
        return buffer.flatten().astype(np.float32)
