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

#pragma once

#include <hls_stream.h>
#include "ap_int.h"
#include <iostream>
#include <fstream>
#include <string>


// TODO reintroduce compile-time checks
#define CASSERT_DATAFLOW(x) ;

// use monitorable streams when simulating, regular hls::stream otherwise
#ifdef MONSTREAM
    #warning "Using monitorable HLS streams for hls::stream"
    #define QNNSTREAM monstream
#else
    //#warning "Using regular HLS streams for hls::stream"
    #define QNNSTREAM hls::stream
#endif

// monitorable stream class
// if doMon=true in constructor, prints every stream transaction to std::cout
template <typename SDT>
class monstream: public hls::stream<SDT> {
    public:
        monstream(const std::string name="", bool doMon=false) : hls::stream<SDT>(name) {
            _doMon = doMon;
        }

        SDT read() {
            SDT ret = hls::stream<SDT>::read();
            if(_doMon)
                std::cout << "stream " << hls::stream<SDT>::_name << ": read " << ret << std::endl;
            return ret;
        }

        void write(const SDT& tail) {
            hls::stream<SDT>::write(tail);

            if(_doMon)
                std::cout << "stream " << hls::stream<SDT>::_name << ": wrote " << tail << std::endl;
        }

    protected:
        bool _doMon;    // enable monitoring transactions
};

template < unsigned int BitWidth >
void logStream(const char *layer_name, hls::stream<ap_uint<BitWidth> > &log){
    FILE *fp = fopen(layer_name, "wb");
    // if (!fp) throw "Error creating log file";

    hls::stream<ap_uint<BitWidth> > tmp_stream;

    while(!log.empty()){
        ap_uint<BitWidth> tmp = (ap_uint<BitWidth>) log.read();
        fwrite(&tmp, sizeof(ap_uint<BitWidth>), 1, fp);
        tmp_stream.write(tmp);
    }

    while(!tmp_stream.empty()){
        ap_uint<BitWidth> tmp = tmp_stream.read();
        log.write((ap_uint<BitWidth>) tmp);
    }

    fclose(fp);
}

template < unsigned int BitWidth >
void logStringStream(const char *layer_name, hls::stream<ap_uint<BitWidth> > &log){
    std::ofstream ofs(layer_name);
    hls::stream<ap_uint<BitWidth> > tmp_stream;

    while(!log.empty()){
        ap_uint<BitWidth> tmp = (ap_uint<BitWidth>) log.read();
        ofs << std::hex << tmp << std::endl;
        tmp_stream.write(tmp);
    }

    while(!tmp_stream.empty()){
        ap_uint<BitWidth> tmp = tmp_stream.read();
        log.write((ap_uint<BitWidth>) tmp);
    }

    ofs.close();
}

#include "streamtools.h"
#include "dma.h"
#include "matrixvector.h"
#include "slidingwindow.h"
#include "maxpool.h"
#include "fclayer.h"
#include "convlayer.h"
#include "streaming_mem_loading.h"
#include "thresholding.h"
