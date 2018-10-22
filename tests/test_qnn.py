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

import os, pickle
from time import sleep
import pytest
from pynq import MMIO
from pynq import Overlay
#from pynq import general_const
import numpy as np
import cv2
import qnn
import sys
from qnn import utils
from qnn import Dorefanet
from qnn import TinierYolo
from pynq import Xlnk
from PIL import Image
sys.path.append("/opt/darknet/python/")
import ctypes
from darknet import *
import filecmp
# Testing W1A2 with dorefanet on imagenet

def test_dorefanet():

	TEST_ROOT_DIR = os.path.dirname(os.path.realpath(__file__))

	QNN_ROOT_DIR = os.path.join(TEST_ROOT_DIR, '../' )
	test_image = os.path.join(TEST_ROOT_DIR, 'Test_image', 'n01484850_0.jpg')
	print(test_image)
	classifier = Dorefanet()
	classifier.init_accelerator()
	net = classifier.load_network(json_layer=os.path.join(QNN_ROOT_DIR, 'qnn', 'params', 'dorefanet-layers.json'))

	conv0_weights = np.load(os.path.join(QNN_ROOT_DIR, 'qnn', 'params', 'dorefanet-conv0.npy'), encoding="latin1").item()
	fc_weights = np.load('/usr/local/lib/python3.6/dist-packages/qnn/params/dorefanet-fc-normalized.npy', encoding='latin1').item()
	with open(os.path.join(QNN_ROOT_DIR, 'notebooks', 'imagenet-classes.pkl'), 'rb') as f:
		classes = pickle.load(f)
		names = dict((k, classes[k][1].split(',')[0]) for k in classes.keys())
		synsets = dict((classes[k][0], classes[k][1].split(',')[0]) for k in classes.keys())

	img, img_class = classifier.load_image(test_image)

	conv0_W = conv0_weights['conv0/W']
	conv0_T = conv0_weights['conv0/T']

	# 1st convolutional layer execution, having as input the image and the trained parameters (weights)
	conv0 = utils.conv_layer(img, conv0_W, stride=4)
	# The result in then quantized to 2 bits representation for the subsequent HW offload
	conv0 = utils.threshold(conv0, conv0_T)

	# Compute offloaded convolutional layers
	in_dim = net['conv0']['output'][1]
	in_ch = net['conv0']['output'][0]
	out_dim = net['merge4']['output_dim']
	out_ch = net['merge4']['output_channels']

	conv_output = classifier.get_accel_buffer(out_ch, out_dim)
	conv_input = classifier.prepare_buffer(conv0)
	classifier.inference(conv_input, conv_output)
	conv_output = classifier.postprocess_buffer(conv_output)
	fc_input = conv_output / np.max(conv_output)

	fc0_W = fc_weights['fc0/Wn']
	fc0_b = fc_weights['fc0/bn']

	fc0_out = utils.fully_connected(fc_input, fc0_W, fc0_b)
	fc0_out = utils.qrelu(fc0_out)
	fc0_out = utils.quantize(fc0_out, 2)

	# FC Layer 1
	fc1_W = fc_weights['fc1/Wn']
	fc1_b = fc_weights['fc1/bn']

	fc1_out = utils.fully_connected(fc0_out, fc1_W, fc1_b)
	fc1_out = utils.qrelu(fc1_out)

	# FC Layer 2
	fct_W = fc_weights['fct/W']
	fct_b = np.zeros((fct_W.shape[1], ))

	fct_out = utils.fully_connected(fc1_out, fct_W, fct_b)
	# Softmax
	out = utils.softmax(fct_out)
	# Top-5 results
	topn =  utils.get_topn_indexes(out, 5)
	topn_golden = np.array([  2, 359, 250, 333, 227])
	assert np.array_equal(topn,topn_golden), 'Dorefanet test failed'
	classifier.deinit_accelerator()
	xlnk = Xlnk();
	xlnk.xlnk_reset()


# Testing W1A3 with tinier-yolo on PASCAL-VOC

def test_tinier_yolo():

	TEST_ROOT_DIR = os.path.dirname(os.path.realpath(__file__))

	QNN_ROOT_DIR = os.path.join(TEST_ROOT_DIR, '../' )
	test_image = os.path.join(TEST_ROOT_DIR, 'Test_image', 'dog.jpg')
	print(test_image)
	classifier = TinierYolo()
	classifier.init_accelerator()
	net = classifier.load_network(json_layer=os.path.join(QNN_ROOT_DIR, 'qnn', 'params', 'tinier-yolo-layers.json'))
	conv0_weights = np.load('/usr/local/lib/python3.6/dist-packages/qnn/params/tinier-yolo-conv0-W.npy', encoding="latin1")
	conv0_weights_correct = np.transpose(conv0_weights, axes=(3, 2, 1, 0))
	conv8_weights = np.load('/usr/local/lib/python3.6/dist-packages/qnn/params/tinier-yolo-conv8-W.npy', encoding="latin1")
	conv8_weights_correct = np.transpose(conv8_weights, axes=(3, 2, 1, 0))
	conv0_bias = np.load('/usr/local/lib/python3.6/dist-packages/qnn/params/tinier-yolo-conv0-bias.npy', encoding="latin1")
	conv0_bias_broadcast = np.broadcast_to(conv0_bias[:,np.newaxis], (net['conv1']['input'][0],net['conv1']['input'][1]*net['conv1']['input'][1]))
	conv8_bias = np.load('/usr/local/lib/python3.6/dist-packages/qnn/params/tinier-yolo-conv8-bias.npy', encoding="latin1")
	conv8_bias_broadcast = np.broadcast_to(conv8_bias[:,np.newaxis], (125,13*13))

	file_name_cfg = c_char_p(os.path.join(QNN_ROOT_DIR, 'qnn', 'params', 'tinier-yolo-bwn-3bit-relu-nomaxpool.cfg').encode())
	net_darknet = lib.parse_network_cfg(file_name_cfg)

	file_name = c_char_p(test_image.encode())
	im = load_image(file_name,0,0)
	im_letterbox = letterbox_image(im,416,416)

	img_flatten = np.ctypeslib.as_array(im_letterbox.data, (3,416,416))
	img = np.copy(img_flatten)
	img = np.swapaxes(img, 0,2)
	if len(img.shape)<4:
		img = img[np.newaxis, :, :, :]
	conv0_ouput = utils.conv_layer(img,conv0_weights_correct,b=conv0_bias_broadcast,stride=2,padding=1)
	conv0_output_quant = conv0_ouput.clip(0.0,4.0)
	conv0_output_quant = utils.quantize(conv0_output_quant/4,3)
		
	out_dim = net['conv7']['output'][1]
	out_ch = net['conv7']['output'][0]

	conv_output = classifier.get_accel_buffer(out_ch, out_dim)
	conv_input = classifier.prepare_buffer(conv0_output_quant*7)
	classifier.inference(conv_input, conv_output)
	conv7_out = classifier.postprocess_buffer(conv_output)
	conv7_out = conv7_out.reshape(out_dim,out_dim,out_ch)
	conv7_out = np.swapaxes(conv7_out, 0, 1) # exp 1
	if len(conv7_out.shape)<4:
		conv7_out = conv7_out[np.newaxis, :, :, :] 
		
	conv8_ouput = utils.conv_layer(conv7_out,conv8_weights_correct,b=conv8_bias_broadcast,stride=1)  
	conv8_out = conv8_ouput.flatten().ctypes.data_as(ctypes.POINTER(ctypes.c_float))
	
	lib.forward_region_layer_pointer_nolayer(net_darknet,conv8_out)
	tresh = c_float(0.3)
	tresh_hier = c_float(0.5)
	file_name_out = c_char_p(os.path.join(TEST_ROOT_DIR, 'Test_image', 'dog-results').encode())
	file_name_probs = c_char_p(os.path.join(TEST_ROOT_DIR, 'Test_image', 'dog-probs.txt').encode())
	file_names_voc = c_char_p("/opt/darknet/data/voc.names".encode())
	darknet_path = c_char_p("/opt/darknet/".encode())
	lib.draw_detection_python(net_darknet, file_name, tresh, tresh_hier,file_names_voc, darknet_path, file_name_out,file_name_probs)
	golden_probs = os.path.join(TEST_ROOT_DIR, 'Test_image', 'tinier-yolo', 'golden_probs_dog.txt')
	current_probs = os.path.join(TEST_ROOT_DIR, 'Test_image', 'dog-probs.txt')
	
	assert filecmp.cmp(golden_probs,current_probs), 'Tinier-Yolo test failed'
	classifier.deinit_accelerator()
	xlnk = Xlnk();
	xlnk.xlnk_reset()
