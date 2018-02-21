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

import numpy as np

def _imageToColumnIndexes(imageShape, kernelHeight, kernelWidth, padding=1, stride=1):
    cols = imageShape[1]
    height = imageShape[2]
    width = imageShape[3]

    outputHeight = int((height + 2 * padding - kernelHeight) / stride + 1)
    outputWidth = int((width + 2 * padding - kernelWidth) / stride + 1)

    indexes0 = np.repeat(np.arange(cols), kernelHeight * kernelWidth).reshape(-1, 1)

    indexes1 = np.tile(np.repeat(np.arange(kernelHeight), kernelWidth), cols).reshape(-1, 1)
    indexes1 = indexes1 + (stride * np.repeat(np.arange(outputHeight), outputWidth)).reshape(1, -1)

    indexes2 = np.tile(np.arange(kernelWidth), kernelHeight * cols).reshape(-1, 1)
    indexes2 = indexes2 + (stride * np.tile(np.arange(outputWidth), outputHeight)).reshape(1, -1)

    return (indexes0.astype(int), indexes1.astype(int), indexes2.astype(int))

def imageToColumn(image, kernelHeight, kernelWidth, padding=1, stride=1):
    indexes = _imageToColumnIndexes(image.shape, kernelHeight, kernelWidth, padding, stride)

    # Zero-pad the input
    if padding > 0:
        paddedImage = np.pad(image, ((0, 0), (0, 0), (padding, padding), (padding, padding)), mode='constant')
    else:
        paddedImage = image

    columns = paddedImage[:, indexes[0], indexes[1], indexes[2]]
    return columns.transpose(1, 2, 0).reshape(kernelHeight * kernelWidth * image.shape[1], -1)

def columnToImage(columns, imageShape, kernelHeight=3, kernelWidth=3, padding=1, stride=1):
    indexes = _imageToColumnIndexes(imageShape, kernelHeight, kernelWidth, padding, stride)
    num = imageShape[0]
    cols = imageShape[1]
    height = imageShape[2] + 2 * padding
    width = imageShape[3] + 2 * padding

    columns = columns.reshape(cols * kernelHeight * kernelWidth, -1, num).transpose(2, 0, 1)

    paddedImage = np.zeros((num, cols, height, width), dtype=columns.dtype)

    np.add.at(paddedImage, (slice(None), indexes[0], indexes[1], indexes[2]), columns)

    if padding > 0:
        return paddedImage[:, :, padding:-padding, padding:-padding]
    else:
        return paddedImage

def conv_layer(x, W, b=None, stride=1, padding=0):
        x = x.transpose(0, 3, 1, 2)
        ofm_ch, ifm_ch, ker_dim, _ = W.shape
        ifm_dim = x.shape[-1]
        ofm_dim = int((ifm_dim - ker_dim + 2 * padding) / stride + 1)
        x_col = imageToColumn(x, ker_dim, ker_dim, padding=padding, stride=stride)
        W_col = W.reshape(ofm_ch, -1)
        if b is not None:
            out = np.matmul(W_col, x_col) + b
        else:
            out = np.matmul(W_col, x_col)
        out = out.reshape(ofm_ch, ofm_dim, ofm_dim)
        return out

def threshold(x, t):
        out = np.sum(np.greater(x.flatten(), t[:,np.newaxis]), 0)
        return out.reshape(x.shape)


def threshold_hard(x, t):
        old_shape = x.shape
        x = x.flatten()
        t = np.array(t).squeeze()
        out = np.zeros(x.size)

        for i, x_i in enumerate(x):
            for t_i in t:
                out[i] += float(x_i >= t_i)

        out = out.reshape(old_shape)

        return out

def softmax(x):
    e_x = np.exp(x - np.max(x))
    return e_x / e_x.sum()


def get_topn_indexes(scores, n):
    return np.argsort(scores)[-n:][::-1]


def quantize(x, k):
    n = float(2**k - 1)
    x = np.round(x * n)
    return x / n


def fully_connected(x, W, b):
    return np.dot(x, W) + b


def qrelu(x):
    return x.clip(0.0, 1.0)
