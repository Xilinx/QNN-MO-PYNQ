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
#include <assert.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))

#define CASSERT_DATAFLOW(x) ;


template<
        unsigned int MaxImgDim,
        unsigned int MaxPoolDim,
        unsigned int MaxNumChannels
        >
void StreamingMaxPool(
    hls::stream<ap_uint<MaxNumChannels> > &in,
    hls::stream<ap_uint<MaxNumChannels> > &out,
    const unsigned int ImgDim,
    const unsigned int PoolDim,
    const ap_uint<1> enable) {

    if (enable) {
        CASSERT_DATAFLOW(ImgDim % PoolDim == 0);

        // need buffer space for a single maxpooled row of the image
        ap_uint<MaxNumChannels> buf[MaxImgDim / MaxPoolDim];
        assert((ImgDim / PoolDim) <= (MaxImgDim/MaxPoolDim));
        for(unsigned int i = 0; i < ImgDim / PoolDim; i++) {
#pragma HLS UNROLL
            buf[i] = 0;
        }

        // assert on loop boundary to optimize HLS
        assert((ImgDim / PoolDim) <= (MaxImgDim/MaxPoolDim));
        for (unsigned int yp = 0; yp < ImgDim / PoolDim; yp++) {
            assert(PoolDim <= MaxPoolDim);
            for (unsigned int ky = 0; ky < PoolDim; ky++) {
                assert((ImgDim / PoolDim) <= (MaxImgDim/MaxPoolDim));
                for (unsigned int xp = 0; xp < ImgDim / PoolDim; xp++) {
#pragma HLS PIPELINE II=1
                    ap_uint<MaxNumChannels> acc = 0;
                    for (unsigned int kx = 0; kx < PoolDim; kx++) {
                        acc = acc | in.read();
                    }
                    // pool with old value in row buffer
                    buf[xp] |= acc;
                }
            }

            assert((ImgDim / PoolDim) <= (MaxImgDim/MaxPoolDim));
            for (unsigned int outpix = 0; outpix < ImgDim / PoolDim; outpix++) {
#pragma HLS PIPELINE II=1
                out.write(buf[outpix]);
                // get buffer ready for next use
                buf[outpix] = 0;
            }
        }

        const unsigned int remaining = ImgDim*ImgDim - (ImgDim/PoolDim)*(ImgDim/PoolDim);

        assert(remaining <= (MaxImgDim*MaxImgDim-(MaxImgDim/MaxPoolDim)*(MaxImgDim/MaxPoolDim)));
        for (unsigned int i = 0; i < remaining; i++){
#pragma HLS PIPELINE II=1
            out.write(0);
        }

    } else {
        assert(ImgDim*ImgDim <= MaxImgDim*MaxImgDim);
        for (unsigned int i = 0; i < ImgDim*ImgDim; i++){
#pragma HLS PIPELINE II=1
            out << in.read();
        }
    }
}


template<
        unsigned int MaxImgInDim,
        unsigned int MaxPoolDim,
        unsigned int MaxNumChannels,
        unsigned int MinStride,
        unsigned int MaxStride,
        unsigned int Precision
        >
void StreamingMaxPool_InputGenerator(
    hls::stream<ap_uint<MaxNumChannels * Precision> > &in,
    hls::stream<ap_uint<MaxNumChannels * Precision> > &out,
    const unsigned int ImgInDim,
    const unsigned int ImgOutDim,
    const unsigned int PoolDim,
    const unsigned int Stride,
    const ap_uint<1> enable){

    if (enable){
        ap_uint<MaxNumChannels * Precision> inputBuf[MaxImgInDim * MaxImgInDim];
#pragma HLS DEPENDENCE variable=inputBuf inter false

        const unsigned int additional_lines = ((ImgInDim * ImgInDim) << Stride) / (ImgOutDim * PoolDim * PoolDim);
        const unsigned int Initial_lines =  ((ImgInDim << Stride) < (ImgOutDim * PoolDim * PoolDim) ? (PoolDim + (1 << Stride)) : (PoolDim + (1 << Stride) + additional_lines - ImgInDim));
        const unsigned int Initial_buffer = MIN(Initial_lines * (ImgInDim), ImgInDim * ImgInDim - 1);
        const unsigned int baseIter = Initial_buffer + (ImgOutDim * ImgOutDim * PoolDim * PoolDim);

        unsigned int inp = 0, oy = 0, ox = 0, ky = 0, kx = 0;
#pragma HLS reset variable=inp

        for (unsigned int i = 0; i < baseIter; i++) {
#pragma HLS PIPELINE II=1

            if (inp < ImgInDim * ImgInDim) {
                inputBuf[inp] = in.read();
                inp++;
            }
            if (inp > Initial_buffer) {
                unsigned int input_base = (oy << Stride) * ImgInDim + (ox << Stride);
                unsigned int input_ind = input_base + ky * ImgInDim + kx;

                ap_uint<MaxNumChannels * Precision> inElem = inputBuf[input_ind];
                out.write(inElem);
                kx++;
                if (kx == PoolDim) {
                    kx = 0;
                    ky++;
                    if (ky == PoolDim) {
                        ky = 0;
                        ox++;
                        if (ox == ImgOutDim) {
                            ox = 0;
                            oy++;
                            if (oy == ImgOutDim) {
                                oy = 0;
                                inp = 0;
                            }
                        }
                    }
                }
            }
        }
    }
    else
    {
        for (unsigned int i = 0; i < ImgInDim*ImgInDim; i++){
#pragma HLS PIPELINE II=1
            out << in.read();
        }
    }
}

template<
        unsigned int MaxImgInDim,
        unsigned int MaxPoolDim,
        unsigned int MaxNumChannels,
        unsigned int MinStride,
        unsigned int MaxStride,
        unsigned int Precision
        >
void CircularStreamingMaxPool_InputGenerator(
    hls::stream<ap_uint<MaxNumChannels * Precision> > &in,
    hls::stream<ap_uint<MaxNumChannels * Precision> > &out,
    const unsigned int ImgInDim,
    const unsigned int ImgOutDim,
    const unsigned int PoolDim,
    const unsigned int Stride,
    const ap_uint<1> enable){

    if (enable){
        // const unsigned int number_blocks = (PoolDim >> Stride) + 1;
        const unsigned int number_blocks = PoolDim + (1 << Stride);
        const unsigned int cycles_write_block = (ImgOutDim * PoolDim * PoolDim);
        const unsigned int cycles_read_block = ImgInDim << Stride;
        const unsigned int max_cycles = MAX(cycles_write_block, cycles_read_block);
        const unsigned int baseIter = ImgInDim * PoolDim + ImgOutDim * max_cycles;
        const unsigned int initial_buffer_cycles = ImgInDim * (PoolDim);

        unsigned int counter_internal_block = 0;
        unsigned int current_block_write = 0;
        unsigned int previous_block_write = 0;
        unsigned int next_block_write = 0;
        unsigned int current_line = 0;
        unsigned int read_block = 0;
        unsigned int count_stride = 0;

        unsigned int inp = 0, ofm_y = 0, ofm_x = 0, k_y = 0, k_x = 0, current_k_y = 0;

        ap_uint<MaxNumChannels * Precision> inputBuf[MaxPoolDim + MaxStride][MaxImgInDim];
//#pragma HLS RESOURCE variable inputBuf core=RAM_2P_LUTRAM
#pragma HLS RESOURCE variable inputBuf core=RAM_2P_BRAM
#pragma HLS RESET variable=read_block
#pragma HLS RESET variable=inp

#pragma HLS DEPENDENCE variable=current_block_write intra false
#pragma HLS DEPENDENCE variable=inputBuf inter false
#pragma HLS DEPENDENCE variable=inputBuf intra false

#pragma HLS ARRAY_PARTITION variable=inputBuf complete dim=1
// #pragma HLS RESOURCE variable inputBuf core=RAM_2P_LUTRAM

        for (unsigned int i = 0; i < baseIter; i++) {
#pragma HLS PIPELINE II=1
            if (inp < initial_buffer_cycles) // Initial buffer of PoolDim lines
            {
                ap_uint<MaxNumChannels * Precision> inElem;
                inElem = in.read();
                inputBuf[current_block_write][current_line] = inElem;
                current_line++;
                inp++;
                if (current_line == ImgInDim)
                {
                    current_line = 0;
                    current_block_write++;
                    if (current_block_write == number_blocks)
                        current_block_write = 0;
                    previous_block_write = current_block_write;
                    read_block++;
                    counter_internal_block = 0;
                }
            }
            else
            {
                if (counter_internal_block < cycles_write_block-1) // We are writing output, MMV IFMChan per cycle
                {
                    unsigned int current_block_read = (previous_block_write + (1 << Stride) + k_y);
                    if (current_block_read >= number_blocks)
                        current_block_read-= number_blocks;
                    unsigned int current_line_in_block = (ofm_x << Stride) + k_x;
                    ap_uint<MaxNumChannels * Precision> outElem = inputBuf[current_block_read][current_line_in_block];
                    out.write(outElem);
                    k_x++;
                    if (k_x == PoolDim) {
                        k_x = 0;
                        k_y++;
                        if (k_y == PoolDim) {
                            k_y = 0;
                            ofm_x ++;
                            if (ofm_x == ImgOutDim) {
                                ofm_x = 0;
                                ofm_y++;
                                if (ofm_y == ImgOutDim) {
                                    ofm_y = 0;
                                    inp = 0;
                                }
                            }
                        }
                    }
                }
                if ((counter_internal_block < cycles_read_block - 1) && (read_block<ImgInDim)) // In parallel we write in the buffer, in the current block write if we still need to
                {
                    ap_uint<MaxNumChannels * Precision> inElem;
                    inElem = in.read();
                    inputBuf[current_block_write][current_line] = inElem;
#pragma HLS DEPENDENCE variable=inputBuf intra false
#pragma HLS DEPENDENCE variable=inputBuf inter false
                    current_line++;
                    if (current_line == ImgInDim) // We read the whole block, we change the next block in which we want to we
                    { // We filled up a block, let's not read until
                        count_stride++;
                        current_line = 0;
                        read_block++;
                        current_block_write++;
                        if (current_block_write == number_blocks)
                            current_block_write = 0;
#pragma HLS DEPENDENCE variable=current_block_write intra false
                        if (count_stride == (1 << Stride))
                        {
                            previous_block_write = current_block_write;
                            count_stride = 0;
                        }
                    }
                }
                counter_internal_block++; // = (counter_internal_block +1) % max_cycles;
                if (counter_internal_block == (max_cycles-1))
                {
                    counter_internal_block = 0;
                }
            }
        } // End base_iter
    }
    else
    {
        for (unsigned int i = 0; i < ImgInDim*ImgInDim; i++){
#pragma HLS PIPELINE II=1
            out << in.read();
        }
    }
}


template<
        unsigned int MaxImgOutDim,
        unsigned int MinPoolDim,
        unsigned int MaxNumChannels,
        unsigned int Precision,
        template<int> class type_input = ap_uint    // For first layer use int value
        >
void StreamingMaxPool_ReducedPrecision(
    hls::stream<ap_uint<MaxNumChannels * Precision> > &in,
    hls::stream<ap_uint<MaxNumChannels * Precision> > &out,
    const unsigned int ImgInDim,
    const unsigned int ImgDimOut,
    const unsigned int PoolDim,
    const ap_uint<1> enable) {

    CASSERT_DATAFLOW(ImgDimOut % PoolDim == 0);
    if (enable){

        // need buffer space for a single maxpooled row of the image
        type_input<Precision> max[MaxNumChannels];
#pragma HLS ARRAY_PARTITION variable=max complete dim=1

        for(unsigned int ch = 0; ch < MaxNumChannels; ch++){
#pragma HLS UNROLL
            max[ch] = 0;
        }

        ap_uint<MaxNumChannels * Precision> inputData;
        ap_uint<MaxNumChannels * Precision> outputData;

        for (unsigned int i = 0; i < ImgDimOut * ImgDimOut; i++) {
            for (unsigned int k = 0; k < PoolDim * PoolDim; k++) {
#pragma HLS PIPELINE II=1
                inputData = in.read();
                for(unsigned int ch = 0; ch < MaxNumChannels; ch++){
#pragma HLS UNROLL
                    unsigned int lowBit = ch * Precision;
                    unsigned int highBit = (ch+1) * Precision - 1;
                    type_input<Precision> channeldata = inputData(highBit, lowBit);

                    if (channeldata > max[ch])
                        max[ch] = channeldata;
                }
            }

            for(unsigned int ch = 0; ch < MaxNumChannels; ch++){
#pragma HLS UNROLL
                unsigned int lowBit = ch * Precision;
                unsigned int highBit = (ch+1) * Precision - 1;
                outputData(highBit, lowBit) = max[ch];

                // get buffer ready for next use
                max[ch] = 0;
            }
            out.write(outputData);
        }
    }
    else
    {
        for (unsigned int i = 0; i < ImgInDim * ImgInDim; i++){
#pragma HLS PIPELINE II=1
            out << in.read();
        }
    }
}


template<
        unsigned int MaxImgDim,
        unsigned int MaxPoolDim,
        unsigned int MaxStride,
        unsigned int MaxNumChannels,
        unsigned int Precision
        >
void StreamingMaxPool_Precision(
    hls::stream<ap_uint<MaxNumChannels * Precision> > &in,
    hls::stream<ap_uint<MaxNumChannels * Precision> > &out,
    const unsigned int PoolInDim, const unsigned int PoolOutDim,
    const unsigned int PoolDim, const unsigned int Stride,
    const ap_uint<1> enable){
#pragma HLS INLINE

    hls::stream<ap_uint<MaxNumChannels * Precision> > poolSwgOut("poolSwgOut");
#pragma HLS STREAM variable=poolSwgOut depth=1
#pragma HLS RESOURCE variable=poolSwgOut core=FIFO_LUTRAM

    // StreamingMaxPool_InputGenerator<MaxImgDim, MaxPoolDim, MaxNumChannels, 1, MaxStride, Precision>(in, poolSwgOut, PoolInDim, PoolOutDim, PoolDim, Stride, enable);
    CircularStreamingMaxPool_InputGenerator<MaxImgDim, MaxPoolDim, MaxNumChannels, 1, MaxStride, Precision>(in, poolSwgOut, PoolInDim, PoolOutDim, PoolDim, Stride, enable);
    StreamingMaxPool_ReducedPrecision<MaxImgDim, 2, MaxNumChannels, Precision, ap_uint>(poolSwgOut, out, PoolInDim, PoolOutDim, PoolDim, enable);
}
