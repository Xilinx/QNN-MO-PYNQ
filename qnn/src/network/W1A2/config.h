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

// hardware config
#define MAX_SIMD                64
#define MAX_PE_CONV             32
#define MAX_PE_FC               2

// layer types
#define FC_LAYER                0
#define CONV_LAYER              1
#define CONVPOOL_LAYER          2

// conv layer dimension boundaries
#define MAX_K                   5
#define MAX_IFM_CH              384
#define MAX_IFM_DIM             64
#define MAX_OFM_CH              384
#define MAX_OFM_DIM             64
#define MAX_POOL_SIZE           3
#define MAX_POOL_STRIDE         2
// conv layer memory dimension boundaries
#define MAX_CONV_WMEM           ((MAX_K*MAX_K * MAX_OFM_CH * MAX_IFM_CH) / (MAX_PE_CONV * MAX_SIMD))
#define MAX_CONV_TMEM           (MAX_OFM_CH / MAX_PE_CONV)
#define MAX_CONV_NUM_WORDS      (MAX_PE_CONV * (MAX_CONV_WMEM + MAX_CONV_TMEM))

// fully-conn layer dimension boundaries
#define MAX_MH                  512
#define MAX_MW                  1024

// define popcount width >= log2(MAX_K*MAX_K*MAX_IFM_CH)
#define POPCOUNT_WIDTH          16

// fully-conn layer memory dimension boundaries
#define MAX_FC_WMEM             ((MAX_MW * MAX_MH) / (MAX_PE_FC * MAX_SIMD))
#define MAX_FC_TMEM             (MAX_MH / MAX_PE_FC)
#define MAX_FC_NUM_WORDS        (MAX_PE_FC * (MAX_FC_WMEM + MAX_FC_TMEM))

// Memory dimensions
#define DATAWIDTH               64
#define CONV_MEM_BITS           MAX_CONV_NUM_WORDS * DATAWIDTH
#define FC_MEM_BITS             MAX_FC_NUM_WORDS * DATAWIDTH


#define MEM_CHANNELS            2

#define INPUT_BITS              8
#define ACTIVATION_BITS         2
#define WEIGHTS_BITS            1
#define THRESHOLDS_BITS         16 * 4
#define MACC_BITS               16

#define BITS_PER_EXTMEMWORD     64
