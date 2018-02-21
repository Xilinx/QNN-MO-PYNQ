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


// essentially small DMA generators, moving data between mem-mapped arrays and
// streams
template<unsigned int DataWidth, unsigned int MaxBytes>
void Mem2Stream(
		ap_uint<DataWidth> *in,
		hls::stream<ap_uint<DataWidth> > &out,
		const unsigned int numBytes) {

	CASSERT_DATAFLOW(DataWidth % 8 == 0);
	const unsigned int numWords = numBytes / (DataWidth / 8);
	CASSERT_DATAFLOW(numWords != 0);

	assert(numWords <= (MaxBytes / (DataWidth / 8)));
	for (unsigned int i = 0; i < numWords; i++) {
#pragma HLS PIPELINE II=1
		ap_uint<DataWidth> e = in[i];
		out.write(e);
	}
}

template<unsigned int DataWidth, unsigned int MaxBytes>
void MemBuf2Stream(
		ap_uint<DataWidth> *in,
		ap_uint<DataWidth> *buffer,
		hls::stream<ap_uint<DataWidth> > &out,
		const unsigned int numBytes, const bool useBuf) {

	CASSERT_DATAFLOW(DataWidth % 8 == 0);
	const unsigned int numWords = numBytes / (DataWidth / 8);
	CASSERT_DATAFLOW(numWords != 0);

#ifndef HWOFFLOAD
	if (useBuf){
		std::cout << "using bram buffer for input" << std::endl;
	}
#endif

	assert(numWords <= (MaxBytes / (DataWidth / 8)));
	for (unsigned int i = 0; i < numWords; i++) {
#pragma HLS PIPELINE II=1
		ap_uint<DataWidth> e;

		if(useBuf)
		{
			e = buffer[i];
		}
		else
		{
			e = in[i];
		}

		out.write(e);
	}
}

template<unsigned int DataWidth, unsigned int MaxBytes>
void Stream2Mem(
		hls::stream<ap_uint<DataWidth> > &in,
		ap_uint<DataWidth> *out,
		const unsigned int numBytes) {

	CASSERT_DATAFLOW(DataWidth % 8 == 0);

	const unsigned int numWords = numBytes / (DataWidth / 8);
	CASSERT_DATAFLOW(numWords != 0);

	assert(numWords <= (MaxBytes / (DataWidth / 8)));
	for (unsigned int i = 0; i < numWords; i++) {
#pragma HLS PIPELINE II=1
		ap_uint<DataWidth> e = in.read();
		out[i] = e;
	}
}

template<unsigned int DataWidth, unsigned int MaxBytes>
void Stream2MemBuf(
		hls::stream<ap_uint<DataWidth> > &in,
		ap_uint<DataWidth> *out,
		ap_uint<DataWidth> *buffer,
		const unsigned int numBytes, const bool useBuf) {

	CASSERT_DATAFLOW(DataWidth % 8 == 0);
	const unsigned int numWords = numBytes / (DataWidth / 8);
	CASSERT_DATAFLOW(numWords != 0);

#ifndef HWOFFLOAD
	if (useBuf){
		std::cout << "using bram buffer for output" << std::endl;
	}
#endif

	assert(numWords <= (MaxBytes / (DataWidth / 8)));
	for (unsigned int i = 0; i < numWords; i++) {
#pragma HLS PIPELINE II=1
		ap_uint<DataWidth> e = in.read();

		if (useBuf)
		{
			buffer[i] = e;
		}
		else
		{
			out[i] = e;
		}
	}
}
