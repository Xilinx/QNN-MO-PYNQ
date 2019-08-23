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
from ctypes import c_size_t
from pynq import Overlay, Xlnk

if os.environ['BOARD'] == 'Ultra96':
	PLATFORM="ultra96"
elif os.environ['BOARD'] == 'Pynq-Z1' or os.environ['BOARD'] == 'Pynq-Z2':
	PLATFORM="pynqZ1-Z2"
else:
	raise RuntimeError("Board not supported")


QNN_ROOT_DIR = os.path.dirname(os.path.realpath(__file__))
ILSVRC_PIXEL_MEAN_FILE = os.path.join(QNN_ROOT_DIR, "params/ilsvrc_pp_mean.npy")
BITSTREAM_NAME="W1A2-{0}.bit".format(PLATFORM)
DOREFANET_BIT_FILE = os.path.join(QNN_ROOT_DIR, "bitstreams", PLATFORM, BITSTREAM_NAME)
CONV_WEIGHTS_FOLDER = os.path.join(QNN_ROOT_DIR, "params/binparam-dorefanet/")
HW_LIBPATH = os.path.join(QNN_ROOT_DIR, "libraries", PLATFORM, "lib_hw.so")
SW_LIBPATH = os.path.join(QNN_ROOT_DIR, "libraries", PLATFORM, "lib_sw_W1A2.so")
W1A2_JSON = os.path.join(QNN_ROOT_DIR, "bitstreams", PLATFORM, "W1A2-overlay.json")
DOREFANET_LAYER_JSON = os.path.join(QNN_ROOT_DIR, "params/dorefanet-layers.json")
init = False
net = dict()

RUNTIME_HW = "python_hw"
RUNTIME_SW = "python_sw"

_ffi = cffi.FFI()
_ffi.cdef("void initParameters(unsigned int const batch, unsigned int const threads);")
_ffi.cdef("void initAccelerator(char const *networkJson, char const *layerJson);")
_ffi.cdef("void singleInference(char *in, size_t const inSize, char *out, size_t const outSize);")
_ffi.cdef("void deinitAccelerator();")



_libraries = {}

class Dorefanet:

    def __init__(self, runtime=RUNTIME_HW):
        self.init = False
        if runtime == RUNTIME_HW:
            self.lib = _ffi.dlopen(HW_LIBPATH)
        else:
            self.lib = _ffi.dlopen(SW_LIBPATH)

    def load_network(self, json_layer=DOREFANET_LAYER_JSON):
        """ Parse network topology from JSON description. """

        with open(json_layer, 'r') as f:
            layer_json = json.load(f)

        # Create a new dictionary with conv and fc layers only
        for layer in layer_json['layers']:
            name = layer['name']
            layer.pop('name', None)
            net[name] = layer

        return net


    def init_accelerator(self, bit_file=DOREFANET_BIT_FILE, json_network=W1A2_JSON, json_layer=DOREFANET_LAYER_JSON):
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




    def load_image(self, imgfile):
        """ Load image from file and apply transformation. """

        imgfile = os.path.abspath(imgfile)
        if not os.path.exists(imgfile) or not os.path.isfile(imgfile):
            raise IOError("Image file does not exist")

        img = cv2.imread(imgfile).astype('float32')
        img = self.transform(img)

        img_class = os.path.basename(imgfile).split('_')[0]

        return img, img_class


    def center_crop(self, img, shape):
        """ Crop image to specified shape. """

        orig_shape = img.shape
        h = int((orig_shape[0] - shape[0]) * 0.5)
        w = int((orig_shape[1] - shape[1]) * 0.5)

        return img[h : h + shape[0], w : w + shape[1]]


    def resize(self, img, shape):
        """ Resize image to specified shape. """

        orig_shape = img.shape[:2]
        scale = 256.0 / min(orig_shape)
        h = int(max(shape[0], min(orig_shape[1], scale * orig_shape[1])))
        w = int(max(shape[1], min(orig_shape[0], scale * orig_shape[0])))
        img = cv2.resize(img, (h, w), interpolation=cv2.INTER_CUBIC)

        return img


    # TODO:
    #   - shape != (224, 224)
    #   - quantization to n bits
    def transform(self, img, shape=(224, 224)):
        """ Transform image with ILSVRC dataset metadata. """

        if shape != (224, 224):
            raise NotImplementedError("To be implemented")

        # Get ILSVRC metadata
        pp_mean = np.load(ILSVRC_PIXEL_MEAN_FILE, allow_pickle=True)
        pp_mean_224 = pp_mean[16:-16, 16:-16, :]

        # Crop and rescale image to 224x224
        img = self.resize(img, shape)
        img = self.center_crop(img, shape)
        img = img - pp_mean_224
        img = img[np.newaxis, :, :, :]

        # Normalize pixel values in range [0, 1.0]
        img = img / 255.0


        # Trasform [0, 1] floating point values into integer values (8-bit)
        # img = img * (2**7 - 1)
        # img[img < 0] = img[img < 0] + 255
        # img = np.floor(img)
        # img = img.astype(np.uint8)

        return img
