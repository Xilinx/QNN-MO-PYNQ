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

#include <assert.h>


/*
unsigned int paddedSizeHW(unsigned int in, unsigned int padTo) {
  if(in % padTo == 0)
    return in;
  else
    return in + padTo - (in % padTo);
}
*/
unsigned int EvaluateStreamMemInit(unsigned int targetLayer);

/*
 * Function implementing the memory loading of the weights and thresholds of the accelerator.
 * The values are read from an input stream and written to the on-chip memory buffer.
 *
 * TODO: control when SIMDWidth > DataWidth (SIMD>64)
 */
template<
    unsigned int DataWidth,                                 // Stream datawidth
    unsigned int PopCountWidth,                             // PopCount datawidth
    unsigned int SIMDWidth,                                 // SIMDWidth of the memory
    unsigned int PECount,                                   // Number of PEs for the target layer
    unsigned int StartOffset,                                     // Number of PEs for the target layer
    unsigned int EndOffset,                                       // Number of PEs for the target layer
    unsigned int MaxWMemCount,                              // Dimension of the weights memory
    unsigned int MaxTMemCount                               // Dimension of the thresholds memory
>
void StreamingInitMemory(
        hls::stream<ap_uint<DataWidth> > &in_stream,
        ap_uint<SIMDWidth> wmem_buffer[PECount][MaxWMemCount],
        ap_uint<PopCountWidth> tmem_buffer[PECount][MaxTMemCount],
        const unsigned int WMemCount, const unsigned int TMemCount) {

    assert(WMemCount <= MaxWMemCount);
    for (unsigned int mem = StartOffset; mem < EndOffset; mem++){
        for (unsigned int index = 0; index < WMemCount; index++){
        #pragma HLS PIPELINE II=1
            ap_uint<DataWidth> mem_val = in_stream.read();
            wmem_buffer[mem][index] = mem_val;
        }
    }

    assert(TMemCount <= MaxTMemCount);
    for (unsigned int mem = StartOffset; mem < EndOffset; mem++){
        for (unsigned int index = 0; index < TMemCount; index++){
        #pragma HLS PIPELINE II=1
            ap_uint<DataWidth> mem_val = in_stream.read();
            tmem_buffer[mem][index] = mem_val;
        }
    }
}

/*
 * Function implementing the memory loading of the weights and thresholds of the accelerator.
 * The values are read from an input stream and written to the on-chip memory buffer.
 *
 * TODO: control when SIMDWidth > DataWidth (SIMD>64)
 */
template<
    unsigned int DataWidth,                                 // Stream datawidth
    unsigned int PopCountWidth,                             // PopCount datawidth
    unsigned int SIMDWidth,                                 // SIMDWidth of the memory
    unsigned int PECount,                                   // Number of PEs for the target layer
    unsigned int StartOffset,                                     // Number of PEs for the target layer
    unsigned int EndOffset,                                       // Number of PEs for the target layer
    unsigned int MaxWMemCount,                              // Dimension of the weights memory
    unsigned int MaxTMemCount                               // Dimension of the thresholds memory
>
void StreamingInitMemory_Precision(
        hls::stream<ap_uint<DataWidth> > &in_stream,
        ap_uint<SIMDWidth> wmem_buffer[PECount][MaxWMemCount],
        ap_uint<PopCountWidth> tmem_buffer[PECount][MaxTMemCount],
        const unsigned int WMemCount, const unsigned int TMemCount) {

    assert(WMemCount <= MaxWMemCount);
    for (unsigned int mem = StartOffset; mem < EndOffset; mem++){
        for (unsigned int index = 0; index < WMemCount; index++){
        #pragma HLS PIPELINE II=1
            ap_uint<DataWidth> mem_val = in_stream.read();
            wmem_buffer[mem][index] = mem_val;
        }
    }

    assert(TMemCount <= MaxTMemCount);
    for (unsigned int mem = StartOffset; mem < EndOffset; mem++){
        for (unsigned int index = 0; index < TMemCount; index++){
            for (unsigned int shift = 0; shift < PopCountWidth/DataWidth; shift++){
            #pragma HLS PIPELINE II=1
            tmem_buffer[mem][index] = tmem_buffer[mem][index] << DataWidth;
            ap_uint<DataWidth> mem_val = in_stream.read();
            tmem_buffer[mem][index](DataWidth-1,0) = mem_val;
            }
        }
    }
}
