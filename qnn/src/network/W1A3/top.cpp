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

#define AP_INT_MAX_W 4096
#include "qnn-library.h"
#include "config.h"


static ap_uint<MAX_SIMD> convWeightMem[MAX_PE_CONV][MAX_CONV_WMEM];
static ap_uint<THRESHOLDS_BITS> convThresMem[MAX_PE_CONV][MAX_CONV_TMEM];

unsigned int paddedSizeHW(unsigned int in, unsigned int padTo) {
    if(in % padTo == 0)
        return in;
    else
        return in + padTo - (in % padTo);
}

void StreamingDoMemInit(ap_uint<DATAWIDTH> *in1, ap_uint<DATAWIDTH> *in2, const short unsigned int KernelDim) {
#pragma HLS DATAFLOW

    hls::stream<ap_uint<DATAWIDTH> > streamIn1("streamInMem1");
    hls::stream<ap_uint<DATAWIDTH> > streamIn2("streamInMem2");

#pragma HLS STREAM variable=streamIn1 depth=16
#pragma HLS STREAM variable=streamIn2 depth=16

    const unsigned int convWMemWidth = ((KernelDim*KernelDim * MAX_OFM_CH * MAX_IFM_CH) / (MAX_PE_CONV * MAX_SIMD));
    const unsigned int convTMemWidth = 2 * MAX_OFM_CH / MAX_PE_CONV;
    const unsigned int convMemBits = DATAWIDTH * MAX_PE_CONV * (convWMemWidth + convTMemWidth);

    Mem2Stream<DATAWIDTH, (CONV_MEM_BITS/MEM_CHANNELS) / 8> (in1, streamIn1, (convMemBits/MEM_CHANNELS) / 8);

    Mem2Stream<DATAWIDTH, (CONV_MEM_BITS/MEM_CHANNELS) / 8> (in2, streamIn2, (convMemBits/MEM_CHANNELS) / 8);

    StreamingInitMemory_Precision<DATAWIDTH, THRESHOLDS_BITS, MAX_SIMD, MAX_PE_CONV, 0, MAX_PE_CONV/2, MAX_CONV_WMEM, MAX_CONV_TMEM>
            (streamIn1, convWeightMem, convThresMem, convWMemWidth, MAX_CONV_TMEM);

    StreamingInitMemory_Precision<DATAWIDTH, THRESHOLDS_BITS, MAX_SIMD, MAX_PE_CONV, MAX_PE_CONV/2, MAX_PE_CONV, MAX_CONV_WMEM, MAX_CONV_TMEM>
            (streamIn2, convWeightMem, convThresMem, convWMemWidth, MAX_CONV_TMEM);
}

void DoCompute(ap_uint<DATAWIDTH> * in,	ap_uint<DATAWIDTH> * out,
        const unsigned int KernelDim, const unsigned int Stride,
        const unsigned int IFMCh, const unsigned int OFMCh,
        const unsigned int IFMDim, const unsigned int PaddedDim,
        const unsigned int OFMDim, const unsigned int PoolInDim,
        const unsigned int PoolOutDim, const unsigned int PoolStride,
        const ap_uint<1> enablePool) {
#pragma HLS DATAFLOW

    hls::stream<ap_uint<DATAWIDTH> > memInStream("memInStream");
    hls::stream<ap_uint<ACTIVATION_BITS * MAX_IFM_CH> > convStream("convStream");
    hls::stream<ap_uint<ACTIVATION_BITS * MAX_OFM_CH> > poolStream("poolStream");
    hls::stream<ap_uint<ACTIVATION_BITS * MAX_OFM_CH> > poolPadStream("poolPadStream");
    hls::stream<ap_uint<ACTIVATION_BITS * MAX_OFM_CH> > netOutStream("netOutStream");
    hls::stream<ap_uint<ACTIVATION_BITS * MAX_IFM_CH> > netOutStream_padded("netOutStream_padded");
    hls::stream<ap_uint<DATAWIDTH> > memOutStream("memOutStream");

#pragma HLS STREAM variable=memInStream depth=1
#pragma HLS STREAM variable=convStream depth=1
#pragma HLS STREAM variable=poolStream depth=1
#pragma HLS STREAM variable=poolPadStream depth=1
#pragma HLS STREAM variable=netOutStream depth=1
#pragma HLS STREAM variable=memOutStream depth=1

#pragma HLS RESOURCE variable=memInStream core=FIFO_LUTRAM
#pragma HLS RESOURCE variable=convStream core=FIFO_LUTRAM
#pragma HLS RESOURCE variable=poolStream core=FIFO_LUTRAM
#pragma HLS RESOURCE variable=poolPadStream core=FIFO_LUTRAM
#pragma HLS RESOURCE variable=netOutStream core=FIFO_LUTRAM
#pragma HLS RESOURCE variable=memOutStream core=FIFO_LUTRAM

    const unsigned int inBits = ACTIVATION_BITS * IFMDim * IFMDim * MAX_IFM_CH;
    const unsigned int outBits = ACTIVATION_BITS * PoolOutDim * PoolOutDim * MAX_IFM_CH;
    const unsigned int paddedIFMCh = paddedSizeHW(IFMCh, MAX_SIMD);
    const unsigned int paddedOFMCh = paddedSizeHW(OFMCh, MAX_PE_CONV);

    Mem2Stream<DATAWIDTH, ACTIVATION_BITS*MAX_IFM_DIM*MAX_IFM_DIM*MAX_IFM_CH / 8> (in, memInStream, inBits / 8);
    StreamingDataWidthConverter<ACTIVATION_BITS*MAX_IFM_DIM*MAX_IFM_DIM*MAX_IFM_CH / DATAWIDTH, DATAWIDTH, ACTIVATION_BITS * MAX_IFM_CH>
            (memInStream, convStream, inBits / DATAWIDTH, DATAWIDTH, ACTIVATION_BITS*MAX_IFM_CH);
    //logStringStream<ACTIVATION_BITS * MAX_IFM_CH>("conv1_in_loopback.txt",convStream);
    StreamingConvLayer_Precision_SIMD_faster <MAX_K,MAX_IFM_CH,MAX_IFM_DIM,MAX_OFM_CH,MAX_OFM_DIM,MAX_SIMD,MAX_PE_CONV,WEIGHTS_BITS,THRESHOLDS_BITS,ACTIVATION_BITS,ACTIVATION_BITS,MACC_BITS,MAX_CONV_WMEM,MAX_CONV_TMEM,FULL_THRESHOLDS>
            (convStream, poolStream, convWeightMem, convThresMem, KernelDim, IFMCh, paddedIFMCh, paddedOFMCh, IFMDim, PaddedDim, OFMDim, Stride);
    //logStringStream<ACTIVATION_BITS * MAX_OFM_CH>("conv1_out_loopback.txt",poolStream);
    StreamPad<ACTIVATION_BITS * MAX_OFM_CH> (poolStream, poolPadStream, OFMDim, PoolInDim);

    StreamingMaxPool_Precision<MAX_OFM_DIM, MAX_POOL_SIZE, MAX_POOL_STRIDE, MAX_OFM_CH, ACTIVATION_BITS>
            (poolPadStream, netOutStream, PoolInDim, PoolOutDim, MAX_POOL_SIZE, PoolStride, enablePool);
    //logStringStream<ACTIVATION_BITS * MAX_OFM_CH>("pool1_out_loopback.txt",netOutStream);
    StreamPadChannels<ACTIVATION_BITS * MAX_OFM_CH, ACTIVATION_BITS * MAX_IFM_CH>(netOutStream, netOutStream_padded, PoolOutDim*PoolOutDim);
    StreamingDataWidthConverter<MAX_OFM_DIM*MAX_OFM_DIM, ACTIVATION_BITS * MAX_IFM_CH, DATAWIDTH>
            (netOutStream_padded, memOutStream, PoolOutDim*PoolOutDim, ACTIVATION_BITS * MAX_IFM_CH, DATAWIDTH);

    Stream2Mem<DATAWIDTH, ACTIVATION_BITS*MAX_OFM_DIM*MAX_OFM_DIM*MAX_IFM_CH / 8> (memOutStream, out, outBits / 8);
}

void BlackBoxJam(ap_uint<64> * in1, ap_uint<64> * in2, ap_uint<64> * out,
        bool doInit, unsigned int layerType,
        const unsigned int KernelDim, const unsigned int Stride,
        const unsigned int IFMCh, const unsigned int OFMCh,
        const unsigned int IFMDim, const unsigned int PaddedDim,
        const unsigned int OFMDim, const unsigned int PoolInDim,
        const unsigned int PoolOutDim, const unsigned int PoolStride)
{
    // signals to be mapped to the AXI Lite slave port
#pragma HLS INTERFACE s_axilite port=return bundle=control
#pragma HLS INTERFACE s_axilite port=doInit bundle=control
#pragma HLS INTERFACE s_axilite port=layerType bundle=control
#pragma HLS INTERFACE s_axilite port=KernelDim bundle=control
#pragma HLS INTERFACE s_axilite port=Stride bundle=control
#pragma HLS INTERFACE s_axilite port=IFMCh bundle=control
#pragma HLS INTERFACE s_axilite port=OFMCh bundle=control
#pragma HLS INTERFACE s_axilite port=IFMDim bundle=control
#pragma HLS INTERFACE s_axilite port=PaddedDim bundle=control
#pragma HLS INTERFACE s_axilite port=OFMDim bundle=control
#pragma HLS INTERFACE s_axilite port=PoolInDim bundle=control
#pragma HLS INTERFACE s_axilite port=PoolOutDim bundle=control
#pragma HLS INTERFACE s_axilite port=PoolStride bundle=control
    // signals to be mapped to the AXI master port (hostmem1, hostmem2)
#pragma HLS INTERFACE m_axi offset=slave port=in1 bundle=hostmem1 depth=1
#pragma HLS INTERFACE s_axilite port=in1 bundle=control
#pragma HLS INTERFACE m_axi offset=slave port=out bundle=hostmem1 depth=1
#pragma HLS INTERFACE s_axilite port=out bundle=control
#pragma HLS INTERFACE m_axi offset=slave port=in2 bundle=hostmem2 depth=1
#pragma HLS INTERFACE s_axilite port=in2 bundle=control
    // partition PE arrays
#pragma HLS ARRAY_PARTITION variable=convWeightMem complete dim=1
#pragma HLS ARRAY_PARTITION variable=convThresMem complete dim=1
#pragma HLS RESOURCE variable=convThresMem core=RAM_2P_LUTRAM

    if (doInit) {
        StreamingDoMemInit(in1, in2, KernelDim);
    } else {
        if (layerType == CONV_LAYER){
            DoCompute(in1, out, KernelDim, Stride, IFMCh, OFMCh, IFMDim, PaddedDim, OFMDim, OFMDim, OFMDim, 0, 0);
        } else {
            DoCompute(in1, out, KernelDim, Stride, IFMCh, OFMCh, IFMDim, PaddedDim, OFMDim, PoolInDim, PoolOutDim, PoolStride, 1);
        }
    }
}
