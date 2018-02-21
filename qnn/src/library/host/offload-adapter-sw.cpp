/*
    Copyright (c) 2018, Xilinx, Inc.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    1.  Redistributions of source code must retain the above copyright notice,
        this list of conditions and the following disclaimer.

    2.  Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

    3.  Neither the name of the copyright holder nor the names of its
        contributors may be used to endorse or promote products derived from
        this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
    PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
    CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
    EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
    OR BUSINESS INTERRUPTION). HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
    OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
    ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#undef HWADDRESS
#include "offload-adapter.h"

#define AP_INT_MAX_W 4096
#include "ap_int.h"
#include "hls_stream.h"

void WhiteBoxJam(ap_uint<64> * in, ap_uint<64> * out);

// DoReFa-Net top function
void BlackBoxJam(ap_uint<64> * in1, ap_uint<64> * in2, ap_uint<64> * out,
        bool doInit, unsigned int layerType,
        const unsigned int KernelDim, const unsigned int Stride,
        const unsigned int IFMCh, const unsigned int OFMCh,
        const unsigned int IFMDim, const unsigned int PaddedDim,
        const unsigned int OFMDim, const unsigned int PoolInDim,
        const unsigned int PoolOutDim, const unsigned int PoolStride);

// CONV and FC top function
void BlackBoxJamFC(ap_uint<64> * in, ap_uint<64> * out,
    bool doInit, unsigned int layerType,
    const unsigned int ConvKernelDim,
    const unsigned int IFMCh, const unsigned int OFMCh,
    const unsigned int IFMDim, const unsigned int OFMDim,
    const unsigned int MatrixH, const unsigned int MatrixW, ap_uint<1> activation);

// CONV-only top function
void BlackBoxJam(ap_uint<64> * in1, ap_uint<64> * in2, ap_uint<64> * out,
        bool doInit, unsigned int layerType, const unsigned int ConvKernelDim,
        const unsigned int IFMCh, const unsigned int OFMCh,
        const unsigned int IFMDim, const unsigned int PaddedDim,
        const unsigned int OFMDim);

std::list<OffloadAdapter *> OffloadAdapter::_instances(0);

OffloadAdapter::OffloadAdapter(std::string const &platformName, unsigned int memoryChannel, size_t bufferSize) :
    _running(false), _isHardware(false), _bufferSize(bufferSize), _weightBuffers(memoryChannel) {
        OffloadAdapter::_instances.push_back(this);
};

OffloadAdapter::~OffloadAdapter() {
    OffloadAdapter::_instances.remove(this);
    this->_buffers.clear();
    this->_localBuffers.clear();
    this->_weightBuffers.clear();
};

void OffloadAdapter::free(ExtMemWord *buffer) {
    delete [] buffer;
}

ExtMemWord *OffloadAdapter::malloc(unsigned int const byteSize) {
    ExtMemWord *buf = new ExtMemWord[byteSize / sizeof(ExtMemWord)];
    if (!buf) {
        throw std::runtime_error("Could not allocate hardware buffer");
    }
    return buf;
}

bool OffloadAdapter::running() {
    return this->_running;
}

void OffloadAdapter::sync() {
    if (!this->_syncData.synced) {
        OffloadAdapter::ExtMemBuffer &inputBuffer = *this->_syncData.input;
        OffloadAdapter::ExtMemBuffer &outputBuffer = *this->_syncData.output;
        inputBuffer.lock.unlock();
        outputBuffer.lock.unlock();
        inputBuffer.cond.notify_all();
        outputBuffer.cond.notify_all();
        this->_syncData.synced = true;
    }
}

void OffloadAdapter::wait() {}
void OffloadAdapter::reset() {}
void OffloadAdapter::execAsync() {}

void OffloadAdapter::offloadWeights(Layers::Layer const &layer, unsigned int weightOffset) {
    this->_running = true;
    OffloadAdapter::ExtMemBuffer &w1 = *std::next(this->_weightBuffers[0].begin(), layer.weightIndex + weightOffset);
    OffloadAdapter::ExtMemBuffer &w2 = *std::next(this->_weightBuffers[1].begin(), layer.weightIndex + weightOffset);
    BlackBoxJam((ap_uint<64> *) w1.buffer, (ap_uint<64> *) w2.buffer,
        NULL, true,  Layers::hw_conv, layer.kernelDim, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    this->_running = false;
}


void OffloadAdapter::offload(OffloadAdapter::ExtMemBuffer &inputBuffer, OffloadAdapter::ExtMemBuffer &outputBuffer, Layers::Layer const &layer) {
    this->_running = true;
    this->_syncData.synced = false;
    this->_syncData.input = &inputBuffer;
    this->_syncData.output = &outputBuffer;
    BlackBoxJam((ap_uint<64> *) inputBuffer.buffer, NULL, (ap_uint<64> *) outputBuffer.buffer, false,
        layer.type, layer.kernelDim, layer.log2stride, layer.IFMCh, layer.OFMCh, layer.IFMDim,
        layer.paddedDim, layer.OFMDim, layer.poolInDim, layer.poolOutDim, layer.poolStride);
    this->_running = false;
}
