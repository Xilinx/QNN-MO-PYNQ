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

#ifndef OFFLOADADAPTER_H
#define OFFLOADADAPTER_H

#include <cmath>
#include <functional>
#include <memory>
#include <iostream>
#include <fstream>
#include <vector>
#include <mutex>
#include <list>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <algorithm>
#include "network.h"
#include "layers.h"
#include "platform.h"

#define DEBUG 1
#include "debug.h"

#define EXTMEMBUFFER_HARDWARE       false
#define EXTMEMBUFFER_LOCAL          true
#define EXTMEMBUFFER_MAX_BUFFERS    100

class OffloadAdapter {
    public:
        struct ExtMemBuffer {
            private:
                OffloadAdapter *_parent;
                bool _local;
                std::atomic<unsigned int> _pending;
            public:
                ExtMemWord *buffer;
                std::mutex lock;
                std::condition_variable cond;

                ExtMemBuffer(OffloadAdapter *parent, ExtMemWord *const  buf, bool local = false)
                 : _parent(parent), _local(local), _pending(0), buffer(buf) {}

                ~ExtMemBuffer() {
                    if (this->_local) {
                        delete [] this->buffer;
                    } else {
                        this->_parent->free(this->buffer);
                    }
                }

                void release() {
                    this->_pending = 0;
                    this->_parent->releaseBuffer(*this);
                }

                void setTarget(unsigned int t) {
                    this->_pending = t;
                }

                bool down() {
                    if (this->_pending > 0) {
                        this->_pending--;
                    }
                    return this->_pending == 0;
                }

                void wait() {
                    std::unique_lock<std::mutex> lk(this->lock);
                }

                bool isPending() {
                    return this->_pending > 0;
                }

                void waitPending() {
                    if (this->_pending > 0) {
                        std::unique_lock<std::mutex> lk(this->lock);
                        cond.wait(lk, [this]() { return (this->_pending == 0); });
                    }
                }

                bool inUse() {
                    std::unique_lock<std::mutex> lk(this->lock, std::try_to_lock);
                    return !lk.owns_lock();
                }

                bool isLocal() {
                    return this->_local;
                }

                size_t size() {
                    return this->_parent->getBufferSize();
                }
        };

        OffloadAdapter(std::string const &, unsigned int, size_t);
        ~OffloadAdapter();

        void offloadWeights(Layers::Layer const &, unsigned int const=0);
        void offload(ExtMemBuffer &, ExtMemBuffer &, Layers::Layer const &);

        unsigned int reserveBuffers(unsigned int num = 1, bool local = false) {
            for (unsigned int i = 0; i < num; i++) {
                try {
                    if (local) {
                        this->_localBuffers.emplace_back(this, new ExtMemWord[this->_bufferSize / sizeof(ExtMemWord)], true);
                        this->_localUnusedBuffers.emplace_back(&this->_localBuffers.back());
                    } else {
                        this->_buffers.emplace_back(this, this->malloc(this->_bufferSize));
                        this->_unusedBuffers.emplace_back(&this->_buffers.back());
                    }
                } catch(...) {
                    return i;
                }
            }
            return num;
        }

        OffloadAdapter::ExtMemBuffer &getBuffer(bool local = false) {
            std::unique_lock<std::mutex> locker(this->_bufferLock);
            bool done = this->_bufferCondition.wait_until(locker, std::chrono::system_clock::now() + std::chrono::seconds(5), [this, local](){
                            bool result = (local  && this->_localUnusedBuffers.size() > 0) || (!local && this->_unusedBuffers.size() > 0);
                            if (!result) {
                                return this->reserveBuffers(1, local) == 1;
                            } else {
                                return true;
                            }
                        });
            if (!done) {
                throw std::runtime_error("Timeout waiting for a buffer releases!");
            }
            OffloadAdapter::ExtMemBuffer *buf;
            if (local) {
                buf = this->_localUnusedBuffers.back();
                this->_localUnusedBuffers.pop_back();
            } else {
                buf = this->_unusedBuffers.back();
                this->_unusedBuffers.pop_back();
            }
            return *buf;
        }

        // void flushBuffer(OffloadAdapter::ExtMemBuffer &buf) {
        //     if (!buf.isLocal() && this->_isHardware) {
        //         this->_platform.flushCache(this->_platform.getPhys((void *)buf.buffer), this->_bufferSize);
        //     }
        // }
        //
        // void invalidateBuffer(OffloadAdapter::ExtMemBuffer &buf) {
        //     if (!buf.isLocal() && this->_isHardware) {
        //         this->_platform.invalidateCache(this->_platform.getPhys((void *)buf.buffer), this->_bufferSize);
        //     }
        // }

        void releaseBuffer(OffloadAdapter::ExtMemBuffer &buf) {
            std::unique_lock<std::mutex> locker(this->_bufferLock);
            if (buf.isLocal()) {
                this->_localUnusedBuffers.push_back(&buf);
            } else {
                this->_unusedBuffers.push_back(&buf);
            }
            locker.unlock();
            this->_bufferCondition.notify_all();
        }

        void waitForBuffer(ExtMemBuffer &buf) {
            std::lock_guard<std::mutex> lock(buf.lock);
        }

        size_t getBufferSize() {
            return this->_bufferSize;
        }

        void reset();

        void free(ExtMemWord *buffer);
        ExtMemWord *malloc(unsigned int const);

        /**
         * to detect from main application if we are in hardware or not
         * @return true if hardware, false else
         */
        unsigned int isHardware() {
            return this->_isHardware;
        }

        /**
         * Loads the weight and treshhold files for the given layers. Does the same for
         * HW and SW no need to differentiate here has some private helper function
         * just for nice code
         * ! split layers with multiple iterations not supported !
         * @param layers   reference to the Layers Class object with alle the layer informations
         * @param dataRoot path to the data root where the weights are
         */
        void loadWeights(Network &network, Layers &layers) {
            if (layers.useBinparams()) {
                unsigned int const maxSIMD = network.getMaxSIMD();
                unsigned int const maxPEConv = network.getMaxPEConv();
                unsigned int const maxPeIndex = (unsigned int) std::floor(maxPEConv / this->_weightBuffers.size());
                unsigned int weightFileIndex = layers.getBinparamSkip();
                unsigned int multiple = 1;
                std::string const dataRoot = layers.getBinparamPath();
                bool split = false;
                for (auto const &layer : layers) {
                    if (layer.layer & Layers::split) {
                        multiple = layer.split;
                        split = true;
                    } else if(layer.layer & Layers::merge) {
                        multiple = 1;
                        split = false;
                    } else if (layer.layer & Layers::conv) {
                        if (split) {
                            if (layer.iterations > 1) {
                                std::cerr << "Current implementation doesn't allow multi iteration layers in split mode!" << std::endl;
                                throw std::runtime_error("Multi iteration layer embedded in split mode!");
                            }
                        } else {
                            multiple = layer.iterations;
                        }
                        unsigned long treshholdOffset = maxPeIndex * layer.convWMem;
                        for (unsigned int i = 0; i < multiple; i++) { //for splits or multi iteration layers
                            for (unsigned int memoryChannel = 0; memoryChannel < this->_weightBuffers.size(); memoryChannel++) { //for every memory channel
                                ExtMemWord *work = this->malloc(layer.convMem);
                                if(!work) {
                                    throw std::runtime_error("Could not allocate contiguous memory!");
                                }
                                try {
                                    for (unsigned int peIndex = 0; peIndex < maxPeIndex; peIndex++) { // for every weight file
                                        // debug_info("Load file %s\n", std::string(dataRoot + "/" + std::to_string(weightFileIndex) + "-" + std::to_string(peIndex + (memoryChannel * maxPeIndex)) + "-weights.bin").c_str());
                                        std::string weightFilename(dataRoot + "/" + std::to_string(weightFileIndex) + "-" + std::to_string(peIndex + (memoryChannel * maxPeIndex)) + "-weights.bin");
                                        // debug_info("Load file %s\n", std::string(dataRoot + "/" + std::to_string(weightFileIndex) + "-" + std::to_string(peIndex + (memoryChannel * maxPeIndex)) + "-thres.bin").c_str());
                                        std::string treshholdFilename(dataRoot + "/" + std::to_string(weightFileIndex) + "-" + std::to_string(peIndex + (memoryChannel * maxPeIndex)) + "-thres.bin");
                                        std::ifstream weightFile(weightFilename, std::ios::binary | std::ios::in);
                                        if (!weightFile.is_open()) {
                                            throw std::runtime_error("Could not open " + weightFilename);
                                        }
                                        std::ifstream treshholdFile(treshholdFilename, std::ios::binary | std::ios::in);
                                        if (!treshholdFile.is_open()) {
                                            throw std::runtime_error("Could not open " + treshholdFilename);
                                        }
                                        if (weightFileIndex == 0 && (layer.IFMCh * layer.stride) < maxSIMD) {
                                            this->_loadFirstLayerWeights(work, 0, peIndex, weightFile, layer);
                                        } else {
                                            this->_loadLayerWeights(work, 0, peIndex, weightFile, layer);
                                        }
                                        this->_loadLayerTreshholds(work, treshholdOffset, peIndex, treshholdFile, layer);
                                        weightFile.close();
                                        treshholdFile.close();
                                    } //for each PEIndex
                                } catch(...) {
                                    this->free(work);
                                    throw;
                                }
                                // this->_platform.flushCache(this->_platform.getPhys((void *)work), layer.convMem);
                                this->_weightBuffers[memoryChannel].emplace_back(this, work, false);
                            } // for each memory channel
                            weightFileIndex++;
                        } // for multiple weightFiles
                    }
                }
            }
        };

        void execAsync();
        void sync();
        void wait();
        bool running();

        void exec() {
            this->execAsync();
            this->sync();
        }

        static void clean(int) {
            for (auto &inst : OffloadAdapter::_instances) {
                inst->~OffloadAdapter();
            }
        }

    private:
        struct SyncData {
            SyncData() : synced(true), input(NULL), output(NULL) {};
            std::atomic<bool> synced;
            OffloadAdapter::ExtMemBuffer *input;
            OffloadAdapter::ExtMemBuffer *output;
        };

        std::atomic<bool> _running;
        OffloadAdapter::SyncData _syncData;

        static std::list<OffloadAdapter *> _instances;
        bool _isHardware;
        size_t _bufferSize;
        std::list<OffloadAdapter::ExtMemBuffer> _buffers;
        std::list<OffloadAdapter::ExtMemBuffer> _localBuffers;
        std::vector<std::list<OffloadAdapter::ExtMemBuffer>> _weightBuffers;
        std::vector<OffloadAdapter::ExtMemBuffer *> _unusedBuffers;
        std::vector<OffloadAdapter::ExtMemBuffer *> _localUnusedBuffers;
        std::mutex _bufferLock;
        std::condition_variable _bufferCondition;
        void *_platform;

        /**
         * helper function for OffloadAdapter::loadWeights
         * implements the first layer weights with the packing of IFMCh * KerStride
         * values in one SIMDWidth word
         * TODO: untested!
         * @param dst     memory target
         * @param offset  memory offset
         * @param file    file to read from
         * @param layer   layer parameters
         */
        void _loadFirstLayerWeights(ExtMemWord *dst, unsigned long const initOffset, unsigned int const peIndex, std::ifstream &file, Layers::Layer const &layer) {
            // debug_info("writing 0x%lx bytes to %p + 0x%lx -> %p\n",  (layer.convWMem / layer.stride) * sizeof(ExtMemWord), dst, initOffset + (peIndex * layer.convWMem) , &dst[initOffset + (peIndex * layer.convWMem)]);
            unsigned int const maxSIMD = layer.network.getMaxSIMD();
            for (unsigned int i = 0; i < (layer.convWMem / layer.stride); i++) {
                ExtMemWord e = 0;
                file.read((char *) &e, sizeof(ExtMemWord));
                for (unsigned int stride = 0; stride < layer.stride - 1; stride++) {
                        ExtMemWord x = 0;
                        file.read((char *) &x, sizeof(ExtMemWord));
                        x = (x << (maxSIMD - layer.IFMCh)) >> (maxSIMD - layer.IFMCh);
                        e = (e << layer.IFMCh) | x;
                }
                dst[initOffset + (peIndex * layer.convWMem) + i] = e;
            }
        }

        /**
         * helper function for OffloadAdapter::loadWeights
         * reads the layer weight into a given memory
         * @param dst     memory target
         * @param offset  memory offset
         * @param file    file to read from
         * @param layer   layer parameters
         */
        void _loadLayerWeights(ExtMemWord *dst, unsigned long const initOffset, unsigned int const peIndex, std::ifstream &file, Layers::Layer const &layer){
            ExtMemWord buf[layer.convWMem];
            std::memset((char *) buf, 0, layer.convWMem * sizeof(ExtMemWord));
            file.read((char *) buf, layer.convWMem * sizeof(ExtMemWord));
            // debug_info("writing 0x%lx bytes to %p + 0x%lx -> %p\n",  layer.convWMem* sizeof(ExtMemWord), dst, initOffset + (peIndex * layer.convWMem), &(dst[initOffset + (peIndex * layer.convWMem)]));
            std::memcpy((void *) &dst[initOffset + (peIndex * layer.convWMem)], (void *) buf, layer.convWMem * sizeof(ExtMemWord));
        };

        /**
         * helper function for OffloadAdapter::loadWeights
         * TODO:    Why there is no shifting after a split layer?
         *          Same behaviour can be done through shift as a parameter
         *          with default value 1
         * reads the layer treshholds into a given memory
         * @param dst     memory target
         * @param offset  memory offset
         * @param file    file to read from
         * @param layer   layer parameters
         */
        void _loadLayerTreshholds(ExtMemWord *dst, unsigned long const initOffset, unsigned int const peIndex, std::ifstream &file, Layers::Layer const &layer){
            unsigned int treshholdsBits = layer.network.getTreshholdsBits();
            unsigned int datawidth = layer.network.getDatawidth();
            unsigned int const shift = std::ceil((float)treshholdsBits / (float)datawidth);
            unsigned long readCount = 0;
            // debug_info("writing 0x%lx bytes to %p + 0x%lx -> %p\n",  layer.convTMem * shift * sizeof(ExtMemWord), dst, initOffset, &dst[initOffset]);
            for(unsigned int l = 0; l < layer.convTMem; l++) {
                    for (unsigned int j = 0; j < shift; j++) {
                            ExtMemWord x = 0;
                            file.read((char *) &x, sizeof(ExtMemWord));
                            readCount+=file.gcount();
                            dst[initOffset + (((peIndex * layer.convTMem) + l) * shift) + j] = x;
                    }
            }
        };

};


#endif
