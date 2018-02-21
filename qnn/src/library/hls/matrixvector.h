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

// TODO Change this to allow differnt strides
#define STRIDE_MAX 1

/*
 * Popcount implemented as unsigned 1-bit add.
 * HLS automatically balances this into an adder tree.
 */
template<
          unsigned int SIMDWidth,
          unsigned int PopCountWidth
        >
ap_uint<PopCountWidth> NaivePopCount(ap_uint<SIMDWidth> val) {
    ap_uint<PopCountWidth> pct = 0;
    for (  unsigned int i = 0; i < SIMDWidth; i++){
        pct += val(i, i);
    }
    return pct;
}

/*
 * Streaming matrix-vector multiply component with binarized activation:
 * binarized inputs, binarized weights, binarized outputs.
 */
template<
          unsigned int MaxWidth,
          unsigned int MaxHeight,
          unsigned int MaxKerShift,
          unsigned int SIMDWidth,           // number of SIMD lanes per PE
          unsigned int PECount,             // number of PEs
          unsigned int PopCountWidth,       // number of bits in popcount accumulator
        unsigned int WMemCount,                 // entries in weight memory
        unsigned int TMemCount
        >
void StreamingMatrixVector(
        hls::stream<ap_uint<SIMDWidth> > &in,
        hls::stream<ap_uint<PECount> > &out,
        const ap_uint<SIMDWidth> weightMem[PECount][WMemCount],
        const ap_uint<PopCountWidth> thresMem[PECount][TMemCount],
        const unsigned int MatrixH,        // height of matrix, multiple of PECount
        const unsigned int MatrixW,        // width of matrix, multiple of SIMDWidth
        const unsigned int KerShift = 1)   // optional number of kernel shifts. 1 corresponds to fully-connected layer
{
    // how many different rows each neuron will compute
    // alternatively: number of vertical matrix chunks
    const unsigned int neuronFold = MatrixH / PECount;
    // how many synapse groups each row is split into
    // alternatively: number of horizontal matrix chunks
    const unsigned int synapseFold = MatrixW / SIMDWidth;

    // PE accumulator registers, initialized to zero on first call to function
    // why not defined as static? then different calls to StreamingMatrixVector
    // with the same template parameters would share these accumulator registers
    ap_uint<PopCountWidth> accPopCount[PECount];
#pragma HLS ARRAY_PARTITION variable=accPopCount complete dim=1

    for (  unsigned int i = 0; i < PECount; i++){
#pragma HLS UNROLL
        accPopCount[i] = 0;
    }

    // input vector buffer
    ap_uint<SIMDWidth> inputBuf[MaxWidth / SIMDWidth];
#pragma HLS RESOURCE variable inputBuf core=RAM_S2P_LUTRAM

    const unsigned int totalFold = neuronFold * synapseFold * KerShift;
      unsigned int nf = 0;
      unsigned int sf = 0;

    // everything merged into a common iteration space (one "big" loop instead
    // of smaller nested loops) to get the pipelinening the way we want

    for (unsigned int i = 0; i < totalFold; i++) {
#pragma HLS PIPELINE II=1
        ap_uint<SIMDWidth> inElem;

        if (nf == 0) {
            // read input from stream
            inElem = in.read();
            // buffer for reuse
            inputBuf[sf] = inElem;
        } else {
            // reuse buffered input
            inElem = inputBuf[sf];
        }

        // compute matrix-vector product for each processing element
        for (  unsigned int pe = 0; pe < PECount; pe++) {
#pragma HLS UNROLL
            ap_uint<SIMDWidth> weight = weightMem[pe][nf * synapseFold + sf];
            ap_uint<SIMDWidth> masked = ~(weight ^ inElem);
            accPopCount[pe] += NaivePopCount<SIMDWidth, PopCountWidth>(masked);
        }
        // keep track of which folded synapse/neuron we are processing
        sf++;

        if (sf == synapseFold) {
            // produce output and clear accumulators
            ap_uint<PECount> outElem = 0;

            for (  unsigned int pe = 0; pe < PECount; pe++) {
#pragma HLS UNROLL
                outElem(pe, pe) = accPopCount[pe] > thresMem[pe][nf] ? 1 : 0;
                accPopCount[pe] = 0;    // clear the accumulator
            }

            out.write(outElem);
            // next folded neuron
            sf = 0;
            nf++;
        }

        if (nf == neuronFold) {
            // next image
            nf = 0;
        }
    }
}


/*
 * Streaming matrix-vector multiply component with rp activation:
 * rp inputs, rp weights, rp outputs
 */
template<
          unsigned int MaxWidth,
          unsigned int MaxHeight,
          unsigned int MaxKerShift,
          unsigned int SIMDWidth,                   // number of SIMD lanes per PE
          unsigned int PECount,                     // number of PEs
          unsigned int WeightsPrecision,            // Number of bits in thresholds
          unsigned int ThresholdPrecision,          // Number of bits in thresholds
          unsigned int Precision,                   // Input data bitwidth
          unsigned int ActivationPrecision,         // Precisions for the activation (Output precision)
          unsigned int MacPrecision,                // Precision of MAC registers
        unsigned int WMemCount,                         // entries in weight memory
        unsigned int TMemCount,                         // entries in threshold memory
        unsigned int ActivationType=FULL_THRESHOLDS,
        template<int> class type_input = ap_uint
        >
void StreamingMatrixVector_Precision(
        hls::stream<ap_uint<SIMDWidth * Precision> > & in,
        hls::stream<ap_uint<PECount * ActivationPrecision> > & out,
        const ap_uint<SIMDWidth * WeightsPrecision> weightMem[PECount][WMemCount],
        const ap_uint<ThresholdPrecision> thresMem[PECount][TMemCount],
        const unsigned int MatrixH,           // height of matrix, multiple of PECount
        const unsigned int MatrixW,           // width of matrix, multiple of SIMDWidth
        const unsigned int realMatrixW,
        const unsigned int KerShift = 1 )      // optional number of kernel shifts. 1 corresponds to fully-connected layer
{
    CASSERT_DATAFLOW(MatrixW % SIMDWidth == 0);
    CASSERT_DATAFLOW(MatrixH % PECount == 0);

    // how many different rows each neuron will compute
    // alternatively: number of vertical matrix chunks
    const unsigned int neuronFold = MatrixH / PECount;

    // how many synapse groups each row is split into
    // alternatively: number of horizontal matrix chunks
    const unsigned int synapseFold = MatrixW / SIMDWidth;

    // input vector buffer
    ap_uint<Precision * SIMDWidth> inputBuf[MaxWidth / SIMDWidth];

    // PE accumulator registers, initialized to zero on first call to function
    // why not defined as static? then different calls to StreamingMatrixVector
    // with the same template parameters would share these accumulator registers
    ap_int<MacPrecision> macRegisters[PECount];
#pragma HLS ARRAY_PARTITION variable=macRegisters complete dim=1

    for(unsigned int i = 0; i < PECount; i++) {
#pragma HLS UNROLL
        macRegisters[i] = 0;
    }

    unsigned int nm = 0;
    unsigned int sf = 0;
    const unsigned int totalFold = neuronFold * synapseFold * KerShift;

    for (unsigned int i = 0; i < totalFold; i++)
    {
#pragma HLS PIPELINE II=1
        ap_uint<SIMDWidth * Precision> inElem;
        if (nm == 0) {
            // read input from stream
            inElem = in.read();
            // buffer for reuse
            inputBuf[sf] = inElem;
        } else {
            // reuse buffered input
            inElem = inputBuf[sf];
        }

        // compute matrix-vector product for each processing element
        for (unsigned int pe = 0; pe < PECount; pe++) {
#pragma HLS UNROLL

            ap_int<WeightsPrecision * SIMDWidth> memWeight =  weightMem[pe][nm * synapseFold + sf];
            ap_int<MacPrecision> tmpMac = macRegisters[pe];

            for(unsigned int simd = 0; simd < SIMDWidth; simd++){
#pragma HLS UNROLL
                // Fetch weights
                ap_int<WeightsPrecision * SIMDWidth> weightArray = memWeight;
                // Low and high bit for each input channel
                unsigned int lowBit = simd * Precision;
                unsigned int highBit = (simd + 1) * Precision - 1;
                // Low and high bit for weight channel
                unsigned int lowBitWeight = simd * WeightsPrecision;
                unsigned int highBitWeight = (simd + 1) * WeightsPrecision - 1;

                type_input<Precision> dataUnsigned = inElem(highBit, lowBit);
                ap_int<WeightsPrecision + Precision + 1> tmpMul;

                if (WeightsPrecision == 1)
                {
                    ap_uint<WeightsPrecision> weight = weightArray(highBitWeight, lowBitWeight);

                    if (weight == 1)
                        tmpMul = -dataUnsigned;
                    else
                        tmpMul = dataUnsigned;
                }
                else
                {
                    ap_uint<WeightsPrecision> weightUnsigned = weightArray(highBitWeight, lowBitWeight);
                    // Convert to signed data type
                    ap_int<WeightsPrecision> weightCompressed = weightUnsigned(WeightsPrecision - 1, 0);
                    ap_int<WeightsPrecision + 1> weightExtended = weightCompressed;
                    ap_int<WeightsPrecision + 1> weight = 2 * weightExtended + 1;

                    // MAC Operation
                    tmpMul = dataUnsigned * weight;
#pragma HLS RESOURCE variable=tmpMul core=Mul_LUT       //Implement in LUTs
                }

                tmpMac += tmpMul;
            }

            macRegisters[pe] = tmpMac;
        }


        sf++;
        if(sf == synapseFold) {
            ap_uint<PECount * ActivationPrecision> outElem = 0;

            for (unsigned int pe = 0; pe < PECount; pe++) {
#pragma HLS UNROLL

                ap_uint<ActivationPrecision> outputPe;

                constexpr unsigned int reducingPrecision = Precision >= ActivationPrecision;
                constexpr unsigned int NumberOfThresholds = reducingPrecision ? (1 << ActivationPrecision) : 2;
                ap_int<ThresholdPrecision> thresholdPe;

                thresholdPe(ThresholdPrecision - 1, 0) = thresMem[pe][nm](ThresholdPrecision - 1, 0);
                outputPe = ReducedPrecision_Threshold<ActivationPrecision, MacPrecision, ThresholdPrecision / NumberOfThresholds, NumberOfThresholds - 1>(macRegisters[pe], thresholdPe);

                // Assign to right bits of output buffers
                unsigned int lowBit = pe * ActivationPrecision;
                unsigned int highBit = (pe + 1) * ActivationPrecision - 1;
                outElem(highBit, lowBit) = outputPe(ActivationPrecision-1, 0);
                macRegisters[pe] = 0;   // clear the accumulator
            }

            out.write(outElem);

            sf = 0;
            nm++;
        }

        if (nm == neuronFold) {
            // next image
            nm = 0;
        }
    }
}


/*
 * Streaming matrix-vector multiply component with binarized activation:
 * binarized inputs, binarized weights, binarized outputs.
 */
template<
        unsigned int MaxWidth,
        unsigned int MaxHeight,
        unsigned int SIMDWidth,             // number of SIMD lanes per PE
        unsigned int PECount,               // number of PEs
        unsigned int PopCountWidth,         // number of bits in popcount accumulator (>=log2(fanin))
        unsigned int WMemCount,             // entries in weight memory
        unsigned int TMemCount
        >
void StreamingMatrixVector_activation(
        hls::stream<ap_uint<SIMDWidth> > &in,
        hls::stream<ap_uint<PopCountWidth> > &out,
        const ap_uint<SIMDWidth> weightMem[PECount][WMemCount],
        const ap_uint<PopCountWidth> thresMem[PECount][TMemCount],
        const unsigned int MatrixH,     // height of matrix, multiple of PECount
        const unsigned int MatrixW,     // width of matrix, multiple of SIMDWidth
        const ap_uint<1> activation = 1
        ) {

    // how many different rows each neuron will compute
    // alternatively: number of vertical matrix chunks
    const unsigned int neuronFold = MatrixH / PECount;
    // how many synapse groups each row is split into
    // alternatively: number of horizontal matrix chunks
    const unsigned int synapseFold = MatrixW / SIMDWidth;

    // PE accumulator registers, initialized to zero on first call to function
    // why not defined as static? then different calls to StreamingMatrixVector
    // with the same template parameters would share these accumulator registers
    ap_uint<PopCountWidth> accPopCount[PECount];
#pragma HLS ARRAY_PARTITION variable=accPopCount complete dim=1

    for (unsigned int i = 0; i < PECount; i++){
#pragma HLS UNROLL
        accPopCount[i] = 0;
    }

    // input vector buffer
    ap_uint<SIMDWidth> inputBuf[MaxWidth / SIMDWidth];

    const unsigned int totalFold = neuronFold * synapseFold;
    unsigned int nf = 0;
    unsigned int sf = 0;

    // everything merged into a common iteration space (one "big" loop instead
    // of smaller nested loops) to get the pipelinening the way we want

    for (unsigned int i = 0; i < totalFold; i++) {
#pragma HLS PIPELINE II=1
        ap_uint<SIMDWidth> inElem;

        if (nf == 0) {
            // read input from stream
            inElem = in.read();
            // buffer for reuse
            inputBuf[sf] = inElem;
        } else {
            // reuse buffered input
            inElem = inputBuf[sf];
        }

        // compute matrix-vector product for each processing element
        for (unsigned int pe = 0; pe < PECount; pe++) {
#pragma HLS UNROLL
            ap_uint<SIMDWidth> weight = weightMem[pe][nf * synapseFold + sf];
            ap_uint<SIMDWidth> masked = ~(weight ^ inElem);
            accPopCount[pe] += NaivePopCount<SIMDWidth, PopCountWidth>(masked);
        }
        // keep track of which folded synapse/neuron we are processing
        sf++;

        if (sf == synapseFold) {
            // produce output and clear accumulators
            if (activation){
                ap_uint<PopCountWidth> outElem = 0;
                for (unsigned int pe = 0; pe < PECount; pe++) {
#pragma HLS UNROLL
                    outElem(pe, pe) = accPopCount[pe] > thresMem[pe][nf] ? 1 : 0;
                    accPopCount[pe] = 0;    // clear the accumulator
                }
                out.write(outElem);
            } else {
                for (unsigned int pe = 0; pe < PECount; pe++) {
#pragma HLS PIPELINE II=1
                    out.write(accPopCount[pe]);
                    accPopCount[pe] = 0;    // clear the accumulator
                }
            }

            // next folded neuron
            sf = 0;
            nf++;
        }

        if (nf == neuronFold) {
            // next image
            nf = 0;
        }
    }
}
