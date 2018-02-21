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

#ifndef LAYERS_H
#define LAYERS_H

#include <cmath>
#include <fstream>
#include <vector>
#include <string>
#include <limits.h>
#include <stdlib.h>
#include <iostream>

#include "rapidjson/document.h"
#include "general-utils.h"
#include "network.h"
#include "platform.h"

class Layers {
    public:
        // The next values are used by hardware offload, they are fixed internal
        static unsigned int const hw_fc = 0;
        static unsigned int const hw_conv = 1;
        static unsigned int const hw_convpool = 2;

        // The next values are used to distinguish between layers
        static unsigned int const none      = 0;
        static unsigned int const conv      = 1;
        static unsigned int const max_pool  = 2;
        static unsigned int const split     = 4;
        static unsigned int const merge     = 8;
        static unsigned int const fc        = 16;

        struct Layer {
            Layer(Layers &layers, Network &network) : parent(layers), network(network),
            layer(0), type(Layers::none), poolInDim(0), poolOutDim(0), poolStride(0),
            kernelDim(0), stride(0), log2stride(0), OFMCh(0), OFMDim(0),
            IFMCh(0), IFMDim(0), padding(0), paddedDim(0), convWMem(0), convTMem(0),
            convMemBits(0), convMem(0), inDim(0), inCh(0), outDim(0), outCh(0),
            outSize(0), inSize(0), inSplit(false), weightIndex(0), iterations(1),
            split(0), merge(0), input(0), output(0) {};
            Layers &parent;
            Network &network;
            std::string function;
            unsigned int layer;
            //for pooling layers
            unsigned int type;
            unsigned int poolInDim;
            unsigned int poolOutDim;
            unsigned int poolStride;
            //for all layers
            unsigned int kernelDim;
            unsigned int stride;
            unsigned int log2stride;
            unsigned int OFMCh;
            unsigned int OFMDim;
            unsigned int IFMCh;
            unsigned int IFMDim;
            double padding;
            unsigned int paddedDim;
            unsigned int convWMem;
            unsigned int convTMem;
            unsigned int convMemBits;
            unsigned int convMem;
            unsigned int inDim;
            unsigned int inCh;
            unsigned int outDim;
            unsigned int outCh;
            unsigned int outSize;
            unsigned int inSize;
            bool inSplit;
            //which weights should be used for this layer
            unsigned int weightIndex;
            //multi iteration layer
            unsigned int iterations;
            //for split layers
            unsigned int split;
            //for merge layers
            unsigned int merge;
            //for fc layers
            unsigned int input;
            unsigned int output;
        };

        Layers(Network &, std::string const &, std::vector<char> const &);
        Layers(Network &, std::string const &);
        ~Layers();




        std::size_t size();
        std::vector<Layers::Layer>::const_iterator begin() const;
        std::vector<Layers::Layer>::const_iterator end() const;
        std::vector<Layers::Layer>::const_iterator getNext(std::vector<Layers::Layer>::const_iterator &, unsigned int) const;
        std::vector<Layers::Layer>::const_iterator getNext(unsigned int) const;
        struct Layers::Layer &getLayer(unsigned int);
        bool useBinparams();
        unsigned int getMaxBufferSize();
        unsigned int getMaxIterations();
        unsigned int getMaxSplit();
        unsigned int getInCh();
        unsigned int getInDim();
        unsigned int getInWords();
        unsigned int getInMem();
        unsigned int getOutCh();
        unsigned int getOutDim();
        unsigned int getOutWords();
        unsigned int getOutMem();
        std::string getNetwork();
        std::string getInputImagePath();
        std::string getVerificationImagePath();
        std::string getBinparamPath();
        unsigned int getBinparamSkip();
        unsigned int getLayersSkip();

#define DEBUG 1
#ifdef DEBUG
#if DEBUG == 1
        void dump() {
            for (auto &layer: (*this)) {
                std::cout << "function:     " << layer.function << std::endl;
                std::cout << "layer:        " << layer.layer << std::endl;
                std::cout << "type:         " << layer.type << std::endl;
                std::cout << "poolInDim:    " << layer.poolInDim << std::endl;
                std::cout << "poolOutDim:   " << layer.poolOutDim << std::endl;
                std::cout << "poolStride:   " << layer.poolStride << std::endl;
                std::cout << "kernelDim:    " << layer.kernelDim << std::endl;
                std::cout << "stride:       " << layer.stride << std::endl;
                std::cout << "log2stride:   " << layer.log2stride << std::endl;
                std::cout << "OFMCh:        " << layer.OFMCh << std::endl;
                std::cout << "OFMDim:       " << layer.OFMDim << std::endl;
                std::cout << "IFMCh:        " << layer.IFMCh << std::endl;
                std::cout << "IFMDim:       " << layer.IFMDim << std::endl;
                std::cout << "padding:      " << layer.padding << std::endl;
                std::cout << "paddedDim:    " << layer.paddedDim << std::endl;
                std::cout << "convWMem:     " << layer.convWMem << std::endl;
                std::cout << "convTMem:     " << layer.convTMem << std::endl;
                std::cout << "convMemBits:  " << layer.convMemBits << std::endl;
                std::cout << "convMem:      " << layer.convMem << std::endl;
                std::cout << "inDim:        " << layer.inDim << std::endl;
                std::cout << "inCh:         " << layer.inCh << std::endl;
                std::cout << "outDim:       " << layer.outDim << std::endl;
                std::cout << "outCh:        " << layer.outCh << std::endl;
                std::cout << "outSize:      " << layer.outSize << std::endl;
                std::cout << "inSize:       " << layer.inSize << std::endl;
                std::cout << "weightIndex:  " << layer.weightIndex << std::endl;
                std::cout << "iterations:   " << layer.iterations << std::endl;
                std::cout << "split:        " << layer.split << std::endl;
                std::cout << "merge:        " << layer.merge << std::endl;
                std::cout << "input:        " << layer.input << std::endl;
                std::cout << "output:       " << layer.output << std::endl;
            }
        }
#endif
#endif
        //Use this layer to indicate a non existing layer
        const Layers::Layer &getNoneLayer() {
            return this->_noneLayer;
        }

    private:
        Layers::Layer _noneLayer;
        Network &_network;
        std::string _jsonFolder;
        rapidjson::Document _layerJson;
        unsigned int _outCh;
        unsigned int _outDim;
        unsigned int _outWords;
        unsigned int _outMem;
        unsigned int _inCh;
        unsigned int _inDim;
        unsigned int _inWords;
        unsigned int _inMem;
        unsigned int _maxBufferSize;
        unsigned int _maxIterations;
        unsigned int _maxSplit;

        std::vector<Layers::Layer> _layers;

        void _parseLayers();
        bool _validateJson();
        bool _validateLayer(unsigned int);
};
#endif
