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

#include "offload-utils.h"

unsigned long long OffloadUtils::_wrongPixels = 0;

void OffloadUtils::padTo(char * bufferPadded, size_t const outputSize, char * bufferUnpadded, size_t const inputSize, unsigned int const elements) {
    unsigned int const inputElementBits = (inputSize * 8) / (elements);
    unsigned int const outputElementBits = (outputSize * 8) / (elements);
    unsigned int const copyBits = (inputElementBits < outputElementBits) ? inputElementBits : outputElementBits;
    OffloadUtils::memset(bufferPadded, 0, outputSize);
    for (unsigned int i = 0; i < elements; i++) {
        OffloadUtils::bitcpy(bufferPadded, i * outputElementBits, bufferUnpadded, i * inputElementBits, copyBits);
    }
}

/**
 * Custom memcpy function for hardware buffers. If the size is not a multiple
 * of 8 the normal memcpy function will fail. This memcpy copies as much as it
 * can with the normal memcpy and the rest byte by byte.
 * @param to   buffer to write
 * @param from buffer to read
 * @param size bytes to copy
 */
void OffloadUtils::memcpy(char *to, char *from, size_t size) {
    size_t const unalignedTo = ((unsigned long long) to) % 0x8;
    size_t const unalignedFrom = ((unsigned long long) from) % 0x8;
    size_t const alignedBytes = (unalignedTo != unalignedFrom) ? 0 : ((size - unalignedTo) / 8) * 8;
    if (alignedBytes) {
        // We can copy aligned bytes effectivly with std::memcpy
        if (unalignedTo > 0) {
            // But we have to fix unaligned addresses
            // debug_info("Fix alignment, unaligned copy %lu bytes from %p to %p...\n", (8 - unalignedTo), from, to);
            for (size_t s = 0; s < (8 - unalignedTo); s++, to++, from++, size--) {
                *to = *from;
            }
        }
        // debug_info("Copy %lu bytes from %p to %p through memcpy...\n", alignedBytes, from, to);
        std::memcpy(to, from, alignedBytes);
    }
    // Copy left over unaligned bytes
    // debug_info("Unaligned copy of %lu bytes from %p to %p...\n", size - alignedBytes, from, to);
    for (size_t s = alignedBytes; s < size; s++) {
        to[s] = from[s];
    }
}


void OffloadUtils::memset(ExtMemWord *to, char val, size_t const size) {
    for (size_t s = 0; s < (size / sizeof(ExtMemWord)); s++) {
        to[s] = val;
    }
}

void OffloadUtils::memset(char *to, char val, size_t size) {
    for (size_t s = 0; s < size; s++) {
        // printf("Setting %p to %d \n", &to[s], val);
        to[s] = val;
    }
}

void OffloadUtils::naiveMemcpy(char *to, char *from, size_t size) {
    for (size_t s = 0; s < size; s++) {
        to[s] = from[s];
    }
}

void OffloadUtils::bitcpy(char *target, size_t dstOffset, char *source, size_t srcOffset, size_t bits) {
    //Normalize
    source += (srcOffset / 64) * 8;
    target += (dstOffset / 64) * 8;
    srcOffset = srcOffset % 64;
    dstOffset = dstOffset % 64;
    // Handle dstOffset
    if (dstOffset > 0) {
        uint64_t const offsetCorrection = (64 - dstOffset);
        uint64_t const bitsCopy = (offsetCorrection < bits) ? offsetCorrection : bits;
        uint64_t *target64 = (uint64_t *) (target);
        uint64_t *source64 = (uint64_t *) (source);
        uint64_t mask = ((uint64_t) (-1)) << dstOffset << (64 - (bitsCopy + dstOffset)) >> (64 - (bitsCopy + dstOffset));
        uint64_t dst = target64[0] & ~mask;
        uint64_t src = source64[0];
        if (bitsCopy > (64 - srcOffset)) {
            // debug_info("<dstHandle><nxt> copy %lu bits\n",bitsCopy);
            uint64_t nxt = source64[1];
            target64[0] = dst | (((src >> srcOffset << dstOffset) | (nxt << dstOffset + (64 - srcOffset))) & mask);
        } else {
            // debug_info("<dstHandle> copy %lu bits\n",bitsCopy);
            target64[0] = dst | ((src >> srcOffset << dstOffset) & mask);
        }
        target += sizeof(uint64_t);
        srcOffset += (64 - dstOffset);
        source += (srcOffset / 64) * 8;
        srcOffset = srcOffset % 64;
        bits -= bitsCopy;
    }

    size_t const byteCopy = (srcOffset) ? 0 : ((bits / 64) * 8);
    size_t const bitsCopy = (srcOffset) ? bits : (bits % 64);
    if (byteCopy) {
        OffloadUtils::memcpy(target, source, byteCopy);
        // debug_info("<memcpy> copied %lu bytes\n", byteCopy);
    }
    if (!bitsCopy)
        return;

    uint64_t *target64 = (uint64_t *) (target + byteCopy);
    uint64_t *source64 = (uint64_t *) (source + byteCopy);
    size_t const rounds = (bitsCopy / 64);
    size_t const bitsLeft = (bitsCopy % 64);
    uint64_t src = source64[0];
    // if (rounds > 0) {
    //     debug_info("<mainloop> copy %lu rounds of 64 bits with srcOffset of %lu\n", rounds, srcOffset);
    // }
    for (size_t r = 0; r < rounds; r++) {
        uint64_t nxt = source64[r+1];
        target64[r] = (src >> srcOffset) | (nxt << (64 - srcOffset));
        src = nxt;
    }
    if (!bitsLeft)
        return;

    uint64_t dst = target64[rounds] >> bitsLeft << bitsLeft;
    if (bitsLeft > (64 - srcOffset)) {
        // debug_info("<bitsleft><nxt> copy left over %lu bits\n", bitsLeft);
        uint64_t nxt = source64[rounds+1];
        target64[rounds] = dst | (src >> srcOffset) | (nxt << (64 - srcOffset) + (64 - bitsLeft) >> ( 64 - bitsLeft));
    } else {
        // debug_info("<bitsleft> copy left over %lu bits\n", bitsLeft);
        target64[rounds] = dst | ((src >> srcOffset) << (64 - bitsLeft) >> (64 - bitsLeft));
    }
}

void OffloadUtils::concatBuffer(ExtMemWord *targetBuffer, ExtMemWord *channelOutput, Layers::Layer const &layer, unsigned int const concatIndex) {
    unsigned int const activationBits = layer.network.getActivationBits();
    unsigned int const maxIFMCh = layer.network.getMaxIFMCh();
    unsigned int const outBitSize = activationBits * layer.OFMCh;
    unsigned int const maxIFMSize = GeneralUtils::padTo(std::ceil((float)(activationBits * maxIFMCh) / 8), apintPadding);
    for (unsigned int i = 0; i < layer.outDim * layer.outDim; i++) {
        unsigned int const offset = maxIFMSize * i;
        OffloadUtils::bitcpy(&((char *)targetBuffer)[offset], concatIndex * outBitSize,  &((char *)channelOutput)[offset], 0, outBitSize);
    }
}

void OffloadUtils::concat(OffloadAdapter::ExtMemBuffer &targetBuffer, OffloadAdapter::ExtMemBuffer &buffer, Layers::Layer const &layer, unsigned int concatIndex) {
    // do not lock the target buffer, so we can concat parallel
    OffloadUtils::concatBuffer(targetBuffer.buffer, buffer.buffer, layer, concatIndex);
}

void OffloadUtils::splitBuffer(ExtMemWord *splitBuffer, ExtMemWord *buffer, Layers::Layer const &layer, unsigned int splitIndex) {
    unsigned int const activationBits = layer.network.getActivationBits();
    unsigned int const maxIFMCh = layer.network.getMaxIFMCh();
    unsigned int const maxIFMSize = GeneralUtils::padTo((activationBits * maxIFMCh) / 8, apintPadding);
    unsigned int const outBits = activationBits * layer.outCh;
    unsigned int const clearOffset = (outBits / 8);//(outBits / 8);//(outBits / 8) - ((outBits / 8) % 8);
    unsigned int const clearBytes = (maxIFMSize - clearOffset);
    unsigned int const bitOffset = splitIndex * outBits;
    for (unsigned int i = 0; i < layer.inDim * layer.inDim; i++) {
        if (clearOffset) {
            OffloadUtils::memset(&((char *) splitBuffer)[(i*maxIFMSize) + clearOffset] , 0, clearBytes);
        }
        OffloadUtils::bitcpy( &((char *) splitBuffer)[i*maxIFMSize], 0,  &((char *) buffer)[i*maxIFMSize], bitOffset, outBits);
        // debug_info("For split %u, copy %u bytes from %p to %p\n",splitIndex, outSize, &((char *) buffer.buffer)[(i*maxIFMSize) + (splitIndex * outSize)], &((char *) splitBuffer.buffer)[(i*maxIFMSize)]);
    }
}

void OffloadUtils::split(OffloadAdapter::ExtMemBuffer &targetBuffer, OffloadAdapter::ExtMemBuffer &buffer, Layers::Layer const &layer, unsigned int splitIndex) {
    std::unique_lock<std::mutex> l1(targetBuffer.lock);
    OffloadUtils::splitBuffer(targetBuffer.buffer, buffer.buffer, layer, splitIndex);
}

void OffloadUtils::mergeBuffer(ExtMemWord *targetbuffer, ExtMemWord *buffer, Layers::Layer const &layer, unsigned int mergeIndex) {
    unsigned int const activationBits = layer.network.getActivationBits();
    unsigned int const maxIFMCh = layer.network.getMaxIFMCh();
    unsigned int const maxIFMSize = GeneralUtils::padTo(std::ceil((float)(activationBits * maxIFMCh) / 8), apintPadding);
    unsigned int const inBits = activationBits * layer.inCh;
    unsigned int const clearOffset = (inBits % 64 == 0) ? 0 : (mergeIndex + 1) * (inBits / 8);
    for (unsigned int i = 0; i < layer.inDim * layer.inDim; i++) {
        if (clearOffset) {
            //If we copy not 8 byte aligned, we have to zero out the last 8 byte
            OffloadUtils::memset(&((char *) targetbuffer)[(i*maxIFMSize) + clearOffset], 0, 8);
        }
        OffloadUtils::bitcpy(&((char *) targetbuffer)[i*maxIFMSize], mergeIndex * inBits, &((char *) buffer)[i*maxIFMSize], 0 , inBits);
    }
}

void OffloadUtils::merge(OffloadAdapter::ExtMemBuffer &targetBuffer, OffloadAdapter::ExtMemBuffer &buffer, Layers::Layer const &layer, unsigned int mergeIndex) {
    std::unique_lock<std::mutex> l1(targetBuffer.lock);
    OffloadUtils::mergeBuffer(targetBuffer.buffer, buffer.buffer, layer, mergeIndex);
}

void OffloadUtils::memcpy(OffloadAdapter::ExtMemBuffer &targetBuffer, OffloadAdapter::ExtMemBuffer &buffer, size_t size) {
    std::unique_lock<std::mutex> l1(targetBuffer.lock);
    OffloadUtils::memcpy((char *) targetBuffer.buffer,(char *) buffer.buffer, size);
}

void OffloadUtils::swap(OffloadAdapter::ExtMemBuffer &targetBuffer, OffloadAdapter::ExtMemBuffer &buffer) {
    std::unique_lock<std::mutex> l1(targetBuffer.lock);
    std::swap(targetBuffer.buffer, buffer.buffer);
}

void OffloadUtils::swpcpy(OffloadAdapter::ExtMemBuffer &targetBuffer, OffloadAdapter::ExtMemBuffer &buffer, size_t size) {
    if (buffer.isLocal()) {
        OffloadUtils::memcpy(targetBuffer, buffer, size);
    } else {
        OffloadUtils::swap(targetBuffer, buffer);
    }
}



bool OffloadUtils::equal(char *buf1, size_t const sizeBuf1, char* buf2, size_t const sizeBuf2) {
    if (sizeBuf1 != sizeBuf2) {
        return false;
    }
    for (unsigned int i = 0; i < sizeBuf1; i++) {
        if (buf1[i] != buf2[i]) {
            return false;
        }
    }
    return true;
}

bool OffloadUtils::verifyBuffers(ExtMemWord *goldenBuffer, ExtMemWord *verifyBuffer, Network &network, unsigned int const outCh, unsigned int const outDim, Logger &output) {
    unsigned int const activationBits = network.getActivationBits();
    unsigned int const maxIFMCh = network.getMaxIFMCh();
    unsigned int const datawidth = network.getDatawidth();
    unsigned int const maxIFMSize = (activationBits * maxIFMCh) / 8;
    unsigned int const outSize = (activationBits * outCh) / 8;
    char *golden = (char *) goldenBuffer;
    char *verify = (char *) verifyBuffer;
    bool result = true;
    unsigned long long wrongBytes = 0;
    OffloadUtils::_wrongPixels = 0;
    for (unsigned int i = 0; i < outDim * outDim; i++){
        bool failed = false;
        for (unsigned int b = 0; b < outSize; b++) {
            if (golden[(i*maxIFMSize)+b] != verify[(i*maxIFMSize)+b]) {
                wrongBytes++;
                result = false;
                failed = true;
            }
        }
        OffloadUtils::_wrongPixels += (failed) ? 1 : 0;
        if (failed && output.active()) {
            output << "golden[" << i << "] ^ verify[" << i << "]:" << std::endl;
            for (unsigned int b = 0; b < outSize; b++) {
                if (golden[(i*maxIFMSize)+b] != verify[(i*maxIFMSize)+b]) {
                    output << std::hex << "\033[0;31m" << std::setfill('0') << std::setw(2) << (golden[(i*maxIFMSize)+b] ^ verify[(i*maxIFMSize)+b]) << std::dec << "\033[0m";
                } else {
                    output << "00" << std::dec;
                }
            }
            output << std::endl << std::endl;
        }
    }
    return result;
}

unsigned long long OffloadUtils::tellPixels() {
    return OffloadUtils::_wrongPixels;
}

bool OffloadUtils::down(OffloadAdapter::ExtMemBuffer &targetBuffer) {
    std::unique_lock<std::mutex> l(targetBuffer.lock);
    bool result = targetBuffer.down();
    l.unlock();
    if (result) {
        targetBuffer.cond.notify_all();
    }
    return result;
}

void waitOrWork(Jobber &jobber, OffloadAdapter::ExtMemBuffer &buffer) {
    while (buffer.isPending()) {
        if (!jobber.work()) {
            buffer.waitPending();
            return;
        }
    }
}
