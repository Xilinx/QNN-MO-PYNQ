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
// convert a stream of bits of given word width to another width
// the greater width must be evenly divisable by the smaller width
template<unsigned int InWidth,		// width of input stream
		unsigned int OutWidth		// width of output stream
>
void StreamingDataWidthConverter_fixed_size(hls::stream<ap_uint<InWidth> > & in,
		hls::stream<ap_uint<OutWidth> > & out,
		unsigned int NumInWords,		// number of input words to process
		unsigned int IFMChReal
	) {
		// emit multiple output words per input word read
	const unsigned int outPerIn = (IFMChReal+OutWidth-1) / OutWidth;
	for (unsigned int i = 0; i < NumInWords; i++) {
		ap_uint<InWidth> ei = in.read();
		for (unsigned int o = 0; o < outPerIn; o++) {
			ap_uint<OutWidth> eo = ei(OutWidth - 1, 0);
			out.write(eo);
			ei = ei >> OutWidth;
		}
	}
}

template<
        unsigned int MaxNumWords,
        unsigned int MaxInWidth,			// width of input stream
        unsigned int MaxOutWidth		    // width of output stream
        >
void StreamingDataWidthConverter(
        hls::stream<ap_uint<MaxInWidth> > &in,
        hls::stream<ap_uint<MaxOutWidth> > &out,
        const unsigned int NumWords, const unsigned int InWidth, const unsigned int OutWidth) {

    if (MaxInWidth > MaxOutWidth) {
        // emit multiple output words per read input word

        // const unsigned int out_per_in = (InWidth + MaxOutWidth - 1) / MaxOutWidth;
        //const unsigned int num_iters = NumWords * out_per_in;

        // assert on loop boundary to optimize HLS
        for (unsigned int inw = 0; inw < NumWords; inw++){
            ap_uint<MaxInWidth> inWord = in.read();
            for (unsigned int outw = 0; outw < InWidth / MaxOutWidth; outw++){
#pragma HLS PIPELINE II=1
                ap_uint<MaxOutWidth> outWord = inWord(MaxOutWidth - 1, 0);
                out.write(outWord);
                inWord = inWord >> MaxOutWidth;
            }
        }
    }
    else if (MaxInWidth == MaxOutWidth) {
        // straight-through copy
        for (unsigned int w = 0; w < NumWords; w++){
#pragma HLS PIPELINE II=1
            ap_uint<MaxInWidth> val = in.read();
            out.write(val);
        }
    }
    else {
        // read multiple input words per output word emitted

        //const unsigned int num_packets = NumWords / (OutWidth/MaxInWidth);
        for (unsigned int p = 0; p < NumWords / (OutWidth/MaxInWidth); p++){
            ap_uint<MaxOutWidth> outWord = 0;
            for (unsigned int inw = 0; inw < OutWidth/MaxInWidth; inw++){
#pragma HLS PIPELINE II=1
            	// Giá ottimizzato per farci stare dorefanet
                ap_uint<MaxOutWidth> inWord = (ap_uint<MaxOutWidth>) in.read();
                inWord = inWord << (OutWidth - MaxInWidth);
                outWord = outWord >> MaxInWidth;
                outWord = outWord | inWord;
                // Versione precedente
                // outWord(OutWidth - 1, OutWidth - MaxInWidth) = inWord;
            }
            out.write(outWord);
        }
    }
}

// Reshape input stream to output only useful data when padding is same:
// Might add 0s at left, right, upper, lower side of the input
// Pad with 0
template<
        unsigned int NumChannels,
        unsigned int Precision=1
        >
void StreamPad(hls::stream<ap_uint<NumChannels* Precision> > &in,
        hls::stream<ap_uint<NumChannels* Precision> > &out,
        const unsigned int ImgDim,
        const unsigned int PaddedDim){

    // Padding
    const unsigned int Padding = PaddedDim - ImgDim;
    // Padding Up and Left
    const unsigned int PaddingUp = Padding / 2;
    const unsigned int PaddingLeft = Padding / 2;
    // Padding Down and Right (might be 1 element more than up and left in case of odd padding)
    const unsigned int PaddingDown = Padding - PaddingUp;
    const unsigned int PaddingRight = Padding - PaddingLeft;

    ap_uint<NumChannels * Precision> outData, inData;

    for(unsigned int y = 0; y < PaddedDim; y++){
        for(unsigned int x = 0; x < PaddedDim; x++){
#pragma HLS PIPELINE II=1

            // Padding Rows
            if(y < PaddingUp || y >= (PaddedDim - PaddingDown))
            {
                outData = 0;
            }
            // Padding Cols
            else if(x < PaddingLeft || x >= (PaddedDim - PaddingRight))
            {
                outData = 0;
            }
            // No Padding
            else
            {
                inData = in.read();
                outData = inData;
            }

            out.write(outData);
        }
    }
}

template<
        unsigned int NumInChannels,
        unsigned int NumOutChannels
        >
void StreamPadChannels(hls::stream<ap_uint<NumInChannels> > &in, hls::stream<ap_uint<NumOutChannels> > &out, const unsigned int NumWords){
    for (unsigned int i = 0; i < NumWords; i++){
#pragma HLS PIPELINE II=1
        ap_uint<NumOutChannels> word = in.read();
        out.write((ap_uint<NumOutChannels>) word);
    }
}

// Reshape input stream to output only useful data when padding is VALID:
// Might drop lines and columns at right and bottom
template<
        unsigned int MaxImgDim,
        unsigned int MaxKernelDim,
        unsigned int MaxStride,
        unsigned int MaxNumChannels,
        unsigned int Precision
        >
void StreamValidResize(
        hls::stream<ap_uint<MaxNumChannels * Precision> > &in,
        hls::stream<ap_uint<MaxNumChannels * Precision> > &out,
        unsigned int ImgDim,
        unsigned int KernelDim,
        unsigned int Stride,
        const bool enable){
    if(enable){
        // Cols and Rows to drop when Padding is Valid
        // Note that all the operations are among unsigned int (i.e. divisions are floored)
        const unsigned int drop = ImgDim - (KernelDim + (ImgDim - KernelDim) / Stride * Stride);

        // Last valid Row/Col of the input data, everything else past this value has to be dropped
        const unsigned int dropAt = ImgDim - drop;

        for(unsigned int i = 0; i < dropAt; i++){
            for(unsigned int j = 0; j < ImgDim; j++){
    #pragma HLS PIPELINE II=1
                ap_uint<MaxNumChannels * Precision> data = in.read();

                if(j < dropAt)
                    out.write(data);
            }
        }

        // Consuming last lines to drop
        for(unsigned int i = 0; i < drop; i++){
            for(unsigned int j = 0; j < ImgDim; j++){
    #pragma HLS PIPELINE II=1
                in.read();
            }
        }
    }
    else
    {
        for (unsigned int i = 0; i < ImgDim*ImgDim; i++){
#pragma HLS PIPELINE II=1
            out << in.read();
        }
    }
}

template<
        unsigned int MaxNumWords,
        unsigned int PECount,
        unsigned int InWidth,           // width of input stream
        unsigned int MaxOutWidth        // width of output stream
        >
void StreamingDataWidthConverter_activation(
        hls::stream<ap_uint<InWidth> > &in,
        hls::stream<ap_uint<MaxOutWidth> > &out,
        const unsigned int NumWords, const unsigned int OutWidth, const ap_uint<1> activation) {

    // read multiple input words per output word emitted

    if (activation)
    {
        //const unsigned int num_packets = NumWords / (OutWidth/InWidth);
        for (unsigned int p = 0; p < NumWords / (OutWidth/PECount); p++){
            ap_uint<MaxOutWidth> outWord = 0;
            for (unsigned int inw = 0; inw < OutWidth/PECount; inw++){
#pragma HLS PIPELINE II=1
                ap_uint<InWidth> inWord = in.read();
                ap_uint<PECount> relevant = (ap_uint<PECount>) inWord(PECount, 0);
                outWord = outWord >> PECount;
                outWord(OutWidth - 1, OutWidth - PECount) = relevant;
            }
            out.write(outWord);
        }
    }
    else
    {
        //const unsigned int num_packets = NumWords / (OutWidth/InWidth);
        for (unsigned int p = 0; p < NumWords / (OutWidth/InWidth); p++){
            ap_uint<MaxOutWidth> outWord = 0;
            for (unsigned int inw = 0; inw < OutWidth/InWidth; inw++){
#pragma HLS PIPELINE II=1
                ap_uint<InWidth> inWord = in.read();
                outWord = outWord >> InWidth;
                outWord(OutWidth - 1, OutWidth - InWidth) = inWord;
            }
            out.write(outWord);
        }
    }
}

template<
        unsigned int SIMDWidth,
        unsigned int MaxNumChannels,
        unsigned int Precision
        >
void StreamRemovePaddedSIMD(
        hls::stream<ap_uint<SIMDWidth * Precision> > &in,
        hls::stream<ap_uint<SIMDWidth * Precision> > &out,
        unsigned int RealChannels,
        unsigned int NumWords){
	unsigned int words_to_send = RealChannels/SIMDWidth;
	constexpr unsigned int total_words = MaxNumChannels/SIMDWidth;
	unsigned int word=0;
	for (unsigned int j = 0 ; j< NumWords; j++)
	{
		if (word<words_to_send)
			out << in.read();
		else
			in.read();
		word++;
		if (word == total_words)
			word=0;
	}
}
