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

#include "thresholding.h"


template<
        // convolution parameters
          unsigned int MaxKernelDim,			// e.g 12 for a 12x12 conv kernel (assumed square)
          unsigned int MaxIFMChannels,			// max number of input feature maps
          unsigned int MaxIFMDim,					// max width of input feature map (assumed square)
          unsigned int MaxOFMChannels,			// max number of output feature maps
          unsigned int MaxOFMDim,					// MaxIFMDim-MaxKernelDim+1 or less

        // matrix-vector unit parameters
          unsigned int SIMDWidth,					// number of SIMD lanes
          unsigned int PECount,					// number of PEs
          unsigned int PopCountWidth,				// number of bits for popcount
        unsigned int WMemCount,					// entries in each PEs weight memory
        unsigned int TMemCount					// entries in each PEs threshold memory
        >
void StreamingConvLayer(
        hls::stream<ap_uint<MaxIFMChannels> > &in,
        hls::stream<ap_uint<MaxOFMChannels> > &out,
        const ap_uint<SIMDWidth> weightMem[PECount][WMemCount],
        const ap_uint<PopCountWidth> thresMem[PECount][TMemCount],
        const unsigned int KernelDim,
        const unsigned int IFMCh,
        const unsigned int OFMCh,
        const unsigned int IFMDim,
        const unsigned int PaddedDim,
        const unsigned int OFMDim,
        const unsigned int Stride)
{
#pragma HLS INLINE

    // set FIFO size on input stream to keep the streams running
    // number of cycles with no reads on the "in" stream
    // const unsigned int inNoReadCycles = KernelDim * KernelDim * OFMDim * OFMDim;
    // // expected production during the no-read phase
    // const unsigned int inFIFOSize = inNoReadCycles / MinCyclesPerInput;
    // set FIFO size on incoming stream
    //#pragma HLS STREAM variable=in depth=inFIFOSize

    hls::stream<ap_uint<MaxIFMChannels> > swgIn("StreamingConvLayer.swgIn");
    hls::stream<ap_uint<MaxIFMChannels> > swgOut("StreamingConvLayer.swgOut");
    hls::stream<ap_uint<SIMDWidth> > mvIn("StreamingConvLayer.mvIn");
    hls::stream<ap_uint<PECount> > mvOut("StreamingConvLayer.mvOut");

#pragma HLS STREAM variable=swgIn depth=1
#pragma HLS STREAM variable=swgOut depth=1
#pragma HLS STREAM variable=mvIn depth=1
#pragma HLS STREAM variable=mvOut depth=1

    // compute weight matrix dimension from conv params
    // TODO this needs to respect the synapse padding rules!
    // if the Python script generates one matrixW/matrixH and this calculates
    // another, we'll be in trouble
    const unsigned int MatrixW = KernelDim * KernelDim * IFMCh;
    const unsigned int MatrixH = OFMCh;

    StreamPad<MaxIFMChannels> (in, swgIn, IFMDim, PaddedDim);

    CircularStreamingConvolutionInputGenerator<MaxKernelDim, MaxIFMChannels, MaxIFMDim, MaxOFMDim>
            (swgIn, swgOut, KernelDim, PaddedDim, OFMDim, Stride);

    // FastStreamingConvolutionInputGenerator<MaxIFMChannels, MaxIFMDim, MaxKernelDim, MaxOFMDim>
    //         (swgIn, swgOut, PaddedDim, KernelDim, OFMDim);

    StreamingDataWidthConverter<MaxOFMDim * MaxOFMDim * MaxKernelDim * MaxKernelDim, MaxIFMChannels, SIMDWidth>
            (swgOut, mvIn, OFMDim * OFMDim * KernelDim * KernelDim, IFMCh, SIMDWidth);

    StreamingMatrixVector<MaxKernelDim * MaxKernelDim * MaxIFMChannels, MaxOFMChannels, MaxOFMDim*MaxOFMDim, SIMDWidth, PECount, PopCountWidth, WMemCount, TMemCount>
            (mvIn, mvOut, weightMem, thresMem, MatrixH, MatrixW, OFMDim * OFMDim);

    StreamingDataWidthConverter<MaxOFMDim * MaxOFMDim * (MaxOFMChannels / PECount), PECount, MaxOFMChannels>
            (mvOut, out, OFMDim * OFMDim * (MatrixH / PECount), PECount, OFMCh);
}

template<
        // convolution parameters
          unsigned int MaxKernelDim,        // e.g 12 for a 12x12 conv kernel (assumed square)
          unsigned int MaxIFMChannels,          // max number of input feature maps
          unsigned int MaxIFMDim,               // max width of input feature map (assumed square)
          unsigned int MaxOFMChannels,          // max number of output feature maps
          unsigned int MaxOFMDim,               // MaxIFMDim-MaxKernelDim+1 or less

        // matrix-vector unit parameters
          unsigned int SIMDWidth,               // number of SIMD lanes
          unsigned int PECount,                 // number of PEs
          unsigned int WeightsPrecision,        // Number of bits in weights
          unsigned int ThresholdPrecision,      // Number of bits in thresholds
          unsigned int Precision,               // Input data bitwidth
          unsigned int ActivationPrecision,     // Precisions for the activation (Output precision)
          unsigned int MacPrecision,            // Precision of MAC registers
        unsigned int WMemCount,                     // entries in each PEs weight memory
        unsigned int TMemCount,                     // entries in each PEs threshold memory
        unsigned int ActivationType=BINARY_THRESHOLDS
        >
void StreamingConvLayer_Precision(
        hls::stream<ap_uint<MaxIFMChannels*Precision> > &in,
        hls::stream<ap_uint<MaxOFMChannels*ActivationPrecision> > &out,
        const ap_uint<SIMDWidth> weightMem[PECount][WMemCount],
        const ap_uint<ThresholdPrecision> thresMem[PECount][TMemCount],
        const unsigned int KernelDim,
        const unsigned int IFMCh,
        const unsigned int PaddedIFMCh,
        const unsigned int OFMCh,
        const unsigned int IFMDim,
        const unsigned int PaddedDim,
        const unsigned int OFMDim,
        const unsigned int Stride)
{
#pragma HLS INLINE

    hls::stream<ap_uint<MaxIFMChannels*Precision> > swgIn("StreamingConvLayer.swgIn");
    hls::stream<ap_uint<MaxIFMChannels*Precision> > swgOut("StreamingConvLayer.swgOut");
    hls::stream<ap_uint<SIMDWidth*Precision> > mvIn("StreamingConvLayer.mvIn");
    hls::stream<ap_uint<PECount*ActivationPrecision> > mvOut("StreamingConvLayer.mvOut");

#pragma HLS STREAM variable=swgIn depth=1
#pragma HLS STREAM variable=swgOut depth=1
#pragma HLS STREAM variable=mvIn depth=1
#pragma HLS STREAM variable=mvOut depth=1

#pragma HLS RESOURCE variable=swgIn core=FIFO_LUTRAM
#pragma HLS RESOURCE variable=swgOut core=FIFO_LUTRAM
#pragma HLS RESOURCE variable=mvIn core=FIFO_LUTRAM
#pragma HLS RESOURCE variable=mvOut core=FIFO_LUTRAM

    const unsigned int MatrixW = KernelDim * KernelDim * PaddedIFMCh;
    const unsigned int MatrixH = OFMCh;

    StreamPad<MaxIFMChannels*Precision> (in, swgIn, IFMDim, PaddedDim);

    CircularStreamingConvolutionInputGenerator<MaxKernelDim, MaxIFMChannels, MaxIFMDim, MaxOFMDim, Precision, STRIDE_MAX>
            (swgIn, swgOut, KernelDim, PaddedDim, OFMDim, Stride);

    StreamingDataWidthConverter<MaxOFMDim * MaxOFMDim * MaxKernelDim * MaxKernelDim, MaxIFMChannels*Precision, SIMDWidth*Precision>
            (swgOut, mvIn, OFMDim * OFMDim * KernelDim * KernelDim, PaddedIFMCh*Precision, SIMDWidth*Precision);

    StreamingMatrixVector_Precision<MaxKernelDim * MaxKernelDim * MaxIFMChannels, MaxOFMChannels, MaxOFMDim*MaxOFMDim, SIMDWidth, PECount, WeightsPrecision, ThresholdPrecision, Precision, ActivationPrecision, MacPrecision, WMemCount, TMemCount, ActivationType>
            (mvIn, mvOut, weightMem, thresMem, MatrixH, MatrixW, KernelDim * KernelDim * IFMCh, OFMDim * OFMDim);


    StreamingDataWidthConverter<MaxOFMDim * MaxOFMDim * (MaxOFMChannels / PECount), PECount*ActivationPrecision, MaxOFMChannels*ActivationPrecision>
            (mvOut, out, OFMDim * OFMDim * (MatrixH / PECount), PECount*ActivationPrecision, OFMCh*ActivationPrecision);
}

template<
        // convolution parameters
        short unsigned int MaxKernelDim,        // e.g 12 for a 12x12 conv kernel (assumed square)
        short unsigned int MaxIFMChannels,          // max number of input feature maps
        short unsigned int MaxIFMDim,               // max width of input feature map (assumed square)
        short unsigned int MaxOFMChannels,          // max number of output feature maps
        short unsigned int MaxOFMDim,               // MaxIFMDim-MaxKernelDim+1 or less

        // matrix-vector unit parameters
        short unsigned int SIMDWidth,               // number of SIMD lanes
        short unsigned int PECount,                 // number of PEs
        short unsigned int WeightsPrecision,        // Number of bits in weights
        short unsigned int ThresholdPrecision,      // Number of bits in thresholds
        short unsigned int Precision,               // Input data bitwidth
        short unsigned int ActivationPrecision,     // Precisions for the activation (Output precision)
        short unsigned int MacPrecision,            // Precision of MAC registers
        unsigned int WMemCount,                     // entries in each PEs weight memory
        unsigned int TMemCount,                     // entries in each PEs threshold memory
        unsigned int ActivationType=BINARY_THRESHOLDS
        >
void StreamingConvLayer_Precision_SIMD_faster(
        hls::stream<ap_uint<MaxIFMChannels*Precision> > &in,
        hls::stream<ap_uint<MaxOFMChannels*ActivationPrecision> > &out,
        const ap_uint<SIMDWidth> weightMem[PECount][WMemCount],
        const ap_uint<ThresholdPrecision> thresMem[PECount][TMemCount],
        const short unsigned int KernelDim,
        const short unsigned int IFMCh,
        const short unsigned int PaddedIFMCh,
        const short unsigned int OFMCh,
        const short unsigned int IFMDim,
        const short unsigned int PaddedDim,
        const short unsigned int OFMDim,
        const short unsigned int Stride)
{
#pragma HLS INLINE
	hls::stream<ap_uint<MaxIFMChannels*Precision> > in_padded("StreamingConvLayer.in_padded");
    hls::stream<ap_uint<SIMDWidth*Precision> > swgIn("StreamingConvLayer.swgIn");
    hls::stream<ap_uint<SIMDWidth*Precision> > swgOut("StreamingConvLayer.swgOut");
    hls::stream<ap_uint<PECount*ActivationPrecision> > mvOut("StreamingConvLayer.mvOut");

#pragma HLS STREAM variable=swgIn depth=1
#pragma HLS STREAM variable=swgOut depth=1
#pragma HLS STREAM variable=mvOut depth=1

#pragma HLS RESOURCE variable=swgIn core=FIFO_LUTRAM
#pragma HLS RESOURCE variable=swgOut core=FIFO_LUTRAM
#pragma HLS RESOURCE variable=mvOut core=FIFO_LUTRAM

    const short unsigned int MatrixW = KernelDim * KernelDim * PaddedIFMCh;
    const short unsigned int MatrixH = OFMCh;

    StreamPad<MaxIFMChannels*Precision> (in, in_padded, IFMDim, PaddedDim); // Add padding on the side for the kernel

	StreamingDataWidthConverter_fixed_size<MaxIFMChannels*Precision, SIMDWidth*Precision>(in_padded, swgIn, PaddedDim*PaddedDim, IFMCh*Precision);

    CircularStreamingConvolutionInputGenerator_SIMD<MaxKernelDim, MaxIFMChannels, MaxIFMDim, MaxOFMDim, SIMDWidth, Precision, STRIDE_MAX>
            (swgIn, swgOut, KernelDim, PaddedDim, OFMDim, Stride, IFMCh);

    StreamingMatrixVector_Precision<MaxKernelDim * MaxKernelDim * MaxIFMChannels, MaxOFMChannels, MaxOFMDim*MaxOFMDim, SIMDWidth, PECount, WeightsPrecision, ThresholdPrecision, Precision, ActivationPrecision, MacPrecision, WMemCount, TMemCount, ActivationType>
            (swgOut, mvOut, weightMem, thresMem, MatrixH, MatrixW, KernelDim * KernelDim * IFMCh, OFMDim * OFMDim);

    StreamingDataWidthConverter<MaxOFMDim * MaxOFMDim * (MaxOFMChannels / PECount), PECount*ActivationPrecision, MaxOFMChannels*ActivationPrecision>
            (mvOut, out, OFMDim * OFMDim * (MatrixH / PECount), PECount*ActivationPrecision, OFMCh*ActivationPrecision);
}
