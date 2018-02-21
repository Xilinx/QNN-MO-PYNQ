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

// Check GCC
#undef ZYNQMP
#undef ZYNQ

#if __GNUC__
#if __x86_64__ || 	__aarch64__
//AMD, Intel and ARM
#define ZYNQMP
#elif __i386__ || __arm__
#define ZYNQ
#endif
#endif

#if !defined(ZYNQMP) && !defined(ZYNQ)
#pragma message("WARNING: Enviroment could not be detected and will be set to ZYNQMP!")
#define ZYNQMP
#endif

#include "offload-adapter.h"
#include "xlnkdriver.hpp"
#define DEBUG 1
#include "debug.h"

#ifdef ZYNQMP
#define HWADDRESS 0xA0000000
#else
#ifdef ZYNQ
#define HWADDRESS 0x43C00000
#endif
#endif

std::list<OffloadAdapter *> OffloadAdapter::_instances(0);

OffloadAdapter::OffloadAdapter(std::string const &platformName, unsigned int memoryChannels, size_t bufferSize) :
    _running(false), _isHardware(true), _bufferSize(bufferSize), _weightBuffers(memoryChannels)   {
        assert(this->_bufferSize > 0);
        this->_platform = (void *) new XlnkDriver(HWADDRESS, 64 * 1024);
        XlnkDriver *platform = (XlnkDriver *) this->_platform;
        platform->attach(platformName.c_str());
        OffloadAdapter::_instances.push_back(this);
};

OffloadAdapter::~OffloadAdapter() {
    XlnkDriver *platform = (XlnkDriver *) this->_platform;
    // debug_info("start\n");
    OffloadAdapter::_instances.remove(this);
    // debug_info("1\n");
    this->_buffers.clear();
    // debug_info("2\n");
    this->_localBuffers.clear();
    // debug_info("3\n");
    this->_weightBuffers.clear();
    // debug_info("end\n");
    delete platform;
};

void OffloadAdapter::free(ExtMemWord *buffer) {
    XlnkDriver *platform = (XlnkDriver *) this->_platform;
    // debug_info("free virt %p phys %p\n", buffer, platform->getPhys((void *)buffer));
    platform->deallocAccelBuffer(platform->getPhys((void *)buffer));
    // debug_info("done\n");
}

ExtMemWord * OffloadAdapter::malloc(unsigned int const byteSize) {
    XlnkDriver *platform = (XlnkDriver *) this->_platform;
    // debug_info("allocating %u bytes\n", byteSize);
    void *accelBuf = platform->allocAccelBuffer(byteSize, false);
    if (!accelBuf) {
        throw std::runtime_error("Could not allocate hardware buffer");
    }
    // debug_info("allocated phys %p\n", accelBuf);
    return (ExtMemWord *) platform->getVirt(accelBuf);
}

void OffloadAdapter::execAsync() {
    XlnkDriver *platform = (XlnkDriver *) this->_platform;
    platform->writeJamRegAddr(0x00, 1);
    //debug_register(0x00, "Control", 1);
}

void OffloadAdapter::sync() {
    if (this->_running) {
        this->wait();
    }
    if (!this->_syncData.synced){
        OffloadAdapter::ExtMemBuffer &inputBuffer = *this->_syncData.input;
        OffloadAdapter::ExtMemBuffer &outputBuffer = *this->_syncData.output;
        inputBuffer.lock.unlock();
        outputBuffer.lock.unlock();
        inputBuffer.cond.notify_all();
        outputBuffer.cond.notify_all();
        this->_syncData.synced = true;
    }
}

void OffloadAdapter::wait() {
    XlnkDriver *platform = (XlnkDriver *) this->_platform;
    while ((platform->readJamRegAddr(0x00) & 0x2) == 0) {};
    this->_running = false;
}

bool OffloadAdapter::running() {
    if (this->_running) {
        XlnkDriver *platform = (XlnkDriver *) this->_platform;
        if ((platform->readJamRegAddr(0x00) & 0x2) != 0) {
            this->_running = false;
        }
    }
    return this->_running;
}

void OffloadAdapter::reset() {
    XlnkDriver *platform = (XlnkDriver *) this->_platform;
    platform->writeJamRegAddr(0x34, true);
    //debug_register(0x34, "doInit", true);
}

void OffloadAdapter::offloadWeights(Layers::Layer const &layer, unsigned int const weightOffset) {
    XlnkDriver *platform = (XlnkDriver *) this->_platform;
    this->_running = true;
    //assert(this->memoryChannelCount() == 2);
    OffloadAdapter::ExtMemBuffer &w1 = *std::next(this->_weightBuffers[0].begin(), layer.weightIndex + weightOffset);
    OffloadAdapter::ExtMemBuffer &w2 = *std::next(this->_weightBuffers[1].begin(), layer.weightIndex + weightOffset);
    //debug_info("Loading weights for index %u\n", layer.weightIndex + weightOffset);

    platform->write64BitJamRegAddr(0x10, (AccelDblReg) platform->getPhys((void *) w1.buffer));
    //debug_register(0x10, "memBuf1", platform->getPhys((void *) w1.buffer));
    platform->write64BitJamRegAddr(0x1c, (AccelDblReg) platform->getPhys((void *) w2.buffer));
    //debug_register(0x1c, "memBuf2", platform->getPhys((void *) w2.buffer));
    platform->writeJamRegAddr(0x34, true);
    //debug_register(0x34, "doInit", true);
    platform->writeJamRegAddr(0x3c, Layers::hw_conv);
    //debug_register(0x3c, "LayerType", CONV_LAYER);
    platform->writeJamRegAddr(0x44, layer.kernelDim);
    //debug_register(0x44, "ConvKernelDim", layer.kernelDim);
}

void OffloadAdapter::offload(OffloadAdapter::ExtMemBuffer &inputBuffer, OffloadAdapter::ExtMemBuffer &outputBuffer, Layers::Layer const &layer) {
    XlnkDriver *platform = (XlnkDriver *) this->_platform;
    this->_running = true;
    this->_syncData.synced = false;
    this->_syncData.input = &inputBuffer;
    this->_syncData.output = &outputBuffer;
    //Lock buffers, they can only be savely unlocked on the sync call
    inputBuffer.lock.lock();
    outputBuffer.lock.lock();
    //inputBuffer.flush();
    // enable compute mode
    platform->writeJamRegAddr(0x34, false);
    //debug_register(0x34, "doInit", false);

    platform->write64BitJamRegAddr(0x10, (AccelDblReg) platform->getPhys((void *) inputBuffer.buffer));
    //debug_register(0x10, "accelBufIn", platform->getPhys((void *) inputBuffer));
    platform->write64BitJamRegAddr(0x28, (AccelDblReg) platform->getPhys((void *) outputBuffer.buffer) );
    //debug_register(0x28, "accelBufOut",  platform->getPhys((void *) outputBuffer));


    platform->writeJamRegAddr(0x3c, layer.type);
    //debug_register(0x3c, "LayerType", layer.type);
    platform->writeJamRegAddr(0x44, layer.kernelDim);
    //debug_register(0x44, "kernelDim", layer.kernelDim);
    platform->writeJamRegAddr(0x4c, layer.log2stride);
    //debug_register(0x4c, "Stride", layer.log2stride);
    platform->writeJamRegAddr(0x54, layer.IFMCh);
    //debug_register(0x54, "IFMCh", layer.IFMCh);
    platform->writeJamRegAddr(0x5c, layer.OFMCh);
    //debug_register(0x5c, "OFMCh", layer.OFMCh);
    platform->writeJamRegAddr(0x64, layer.IFMDim);
    //debug_register(0x64, "IFMDim", layer.IFMDim);
    platform->writeJamRegAddr(0x6c, layer.paddedDim);
    //debug_register(0x6c, "PaddedDim", layer.paddedDim);
    platform->writeJamRegAddr(0x74, layer.OFMDim);
    //debug_register(0x74, "OFMDim", layer.OFMDim);
    platform->writeJamRegAddr(0x7c, layer.poolInDim);
    //debug_register(0x7c, "PoolInDim", layer.poolInDim);
    platform->writeJamRegAddr(0x84, layer.poolOutDim);
    //debug_register(0x84, "PoolOutDim", layer.poolOutDim);
    platform->writeJamRegAddr(0x8c, layer.poolStride);
    //debug_register(0x8c, "PoolStride", layer.poolStride);
}
