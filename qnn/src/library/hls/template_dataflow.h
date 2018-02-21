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

#define MAX(x, y) (((x) > (y)) ? (x) : (y));
#define MIN(x, y) (((x) < (y)) ? (y) : (x));

#include "thresholding.h"

template<typename InT, typename OutT>
void Cast(stream<InT> & in, stream<OutT> & out, unsigned int numReps) {
  for(unsigned int i = 0; i < numReps; i++) {
#pragma HLS PIPELINE II=1
    out.write((OutT) in.read());
  }
}

template<unsigned int InWidth,		// width of input stream
		unsigned int OutWidth,		// width of output stream
		unsigned int NumInWords		// number of input words to process
>
void DataWidthConverter_fixed(stream<ap_uint<InWidth> > & in,
		stream<ap_uint<OutWidth> > & out) {
	if (InWidth > OutWidth) {
		// emit multiple output words per input word read
		const unsigned int outPerIn = InWidth / OutWidth;
		for (unsigned int i = 0; i < NumInWords; i++) {
			ap_uint<InWidth> ei = in.read();
			for (unsigned int o = 0; o < outPerIn; o++) {
				ap_uint<OutWidth> eo = ei(OutWidth - 1, 0);
				out.write(eo);
				ei = ei >> OutWidth;
			}
		}
	} else if (InWidth == OutWidth) {
		// straight-through copy
		for (unsigned int i = 0; i < NumInWords; i++) {
			ap_uint<InWidth> e = in.read();
			out.write(e);
		}

	} else { // InWidth < OutWidth
		// read multiple input words per output word emitted
		const unsigned int inPerOut = OutWidth / InWidth;
		for (unsigned int o = 0; o < NumInWords / inPerOut; o++) {
			ap_uint<OutWidth> eo = 0;
			for (unsigned int i = 0; i < inPerOut; i++) {
				ap_uint<InWidth> ei = in.read();
				eo = eo >> InWidth;
				eo(OutWidth - 1, OutWidth - InWidth) = ei;
			}
			out.write(eo);
		}
	}
}

template<unsigned int DataWidth, unsigned int numBytes>
void Mem2Stream_fixed(ap_uint<DataWidth> * in, stream<ap_uint<DataWidth> > & out) {
	const unsigned int numWords = numBytes / (DataWidth / 8);
	for (unsigned int i = 0; i < numWords; i++) {
		ap_uint<DataWidth> e = in[i];
		out.write(e);
	}

}

template<unsigned int DataWidth, unsigned int numBytes>
void Stream2Mem_fixed(stream<ap_uint<DataWidth> > & in, ap_uint<DataWidth> * out) {

	const unsigned int numWords = numBytes / (DataWidth / 8);
	for (unsigned int i = 0; i < numWords; i++) {
		ap_uint<DataWidth> e = in.read();
		out[i] = e;
	}
}


// Reshape input stream to output only useful data when padding is same:
// Might add 0s at left, right, upper, lower side of the input
// Pad with 0
template<	unsigned int ImgDim,
			unsigned int KernelDim,
			unsigned int Stride,
			unsigned int NumChannels,
			unsigned int Precision>
void SameResize(stream<ap_uint<NumChannels* Precision> > &in,
		stream<ap_uint<NumChannels* Precision> > &out){

	// Number of "same" windows over the input data
	constexpr unsigned int SameWindows = (ImgDim) / Stride + ((ImgDim % Stride) > 0);

	// Number of elements to generate as output per dimension
	constexpr unsigned int OutputDim = KernelDim + Stride * (SameWindows - 1);

	// Padding
	constexpr unsigned int Padding = OutputDim - ImgDim;

	// Padding Up and Left
	constexpr unsigned int PaddingDown = Padding/2;
	constexpr unsigned int PaddingRight = Padding/2;

	// Padding Down and Right (might be 1 element more than up and left in case of odd padding)
	constexpr unsigned int PaddingUp = Padding - PaddingDown;
	constexpr unsigned int PaddingLeft = Padding - PaddingRight;

	ap_uint<NumChannels* Precision> outData, inData;

	for(unsigned int y = 0; y<OutputDim; y++){
		for(unsigned int x=0; x < OutputDim; x++){
#pragma HLS PIPELINE II=1

			// Padding Rows
			if(y < PaddingUp || y >= (OutputDim - PaddingDown)){
				outData = 0;
			}
			// Padding Cols
			else if(x < PaddingLeft || x >= (OutputDim - PaddingRight)){
				outData = 0;
			}
			// No Padding
			else{
				inData = in.read();
				outData = inData;
			}

			out.write(outData);
		}
	}
}


template<unsigned int SIMDWidth, 		// number of SIMD lanes per PE
		unsigned int PECount,			// number of PEs
		unsigned int WeightsPrecision, 	// Number of bits in thresholds
		unsigned int ThresholdPrecision, // Number of bits in thresholds
		unsigned int MatrixW,			// width of matrix, multiple of SIMDWidth
		unsigned int MatrixH,			// height of matrix, multiple of PECount
		unsigned int WMemCount,			// entries in weight memory
		unsigned int TMemCount,			// entries in threshold memory
		unsigned int Precision,			// Input data bitwidth
		unsigned int ActivationPrecision, // Precisions for the activation (Output precision)
		unsigned int MacPrecision,		// Precision of MAC registers
		unsigned int ActivationType = 0,	// Don't use activation
		template<int> class type_input = ap_uint		// For first layer use int value
>
void MatrixVector_Precision_dsp(stream<ap_uint<SIMDWidth * Precision> > & in,
		stream<ap_uint<PECount * ActivationPrecision> > & out,
		const ap_uint<SIMDWidth * WeightsPrecision> weightMem[PECount][WMemCount],
		const ap_uint<ThresholdPrecision> thresMem[PECount][TMemCount], const unsigned int numReps)
{

	// how many different rows each neuron will compute
	// alternatively: number of vertical matrix chunks
	const unsigned int neuronFold = MatrixH / PECount;

	// how many synapse groups each row is split into
	// alternatively: number of horizontal matrix chunks
	const unsigned int synapseFold = MatrixW / SIMDWidth;

	// input vector buffer
	ap_uint<Precision * SIMDWidth> inputBuf[synapseFold];

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
	const unsigned int totalFold = neuronFold * synapseFold;

	for (unsigned int i = 0; i < totalFold * numReps; i++)
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
				// ap_int<WeightsPrecision * SIMDWidth> weightArray = weightMem[pe][nm * synapseFold + sf];
				ap_int<WeightsPrecision * SIMDWidth> weightArray = memWeight;

				// Low and high bit for each input channel
				unsigned int lowBit = simd * Precision;
				unsigned int highBit = (simd+1) * Precision - 1;

				// Low and high bit for weight channel
				unsigned int lowBitWeight = simd * WeightsPrecision;
				unsigned int highBitWeight = (simd+1) * WeightsPrecision - 1;

				// Get weight for the channel
				type_input<Precision> dataUnsigned = inElem(highBit, lowBit);
				ap_uint<WeightsPrecision> weightUnsigned = weightArray(highBitWeight, lowBitWeight);
				ap_int<WeightsPrecision> weight = weightUnsigned(WeightsPrecision-1, 0);
				// MAC Operation
				ap_int<MacPrecision> tmpMul = dataUnsigned * weight;
#pragma HLS RESOURCE variable=tmpMul core=DSP48		//Implement in LUTs

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

				// TODO: Reducing precision check is used onl because the compiler tries to compile
				// this code even when ActivationType!=FULL_THRESHOLDS.
				// Need to find a way to remove this and set NumberOfThresholds = 1 << ActivationPrecision
				constexpr unsigned int reducingPrecision = Precision >= ActivationPrecision;
				constexpr unsigned int NumberOfThresholds = reducingPrecision ? (1 << ActivationPrecision) : 2;
				ap_int<ThresholdPrecision> thresholdPe;
				thresholdPe(ThresholdPrecision - 1, 0) = thresMem[pe][nm](ThresholdPrecision - 1, 0);
				outputPe = ReducedPrecision_Threshold<ActivationPrecision, MacPrecision, ThresholdPrecision/NumberOfThresholds, NumberOfThresholds-1>(macRegisters[pe], thresholdPe);

				// Assign to right bits of output buffers
				unsigned int lowBit = pe * ActivationPrecision;
				unsigned int highBit = (pe+1) * ActivationPrecision - 1;
				outElem(highBit, lowBit) = outputPe(ActivationPrecision-1, 0);

				macRegisters[pe] = 0;	// clear the accumulator
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

template<
        unsigned int ConvKernelDim,
		unsigned int IFMChannels,
		unsigned int Input_precision,
		unsigned int IFMDim,
		unsigned int OFMDim,
		unsigned int Stride = 1
        >
void ConvolutionInputGenerator(
    stream<ap_uint<IFMChannels*Input_precision> > &in,
    stream<ap_uint<IFMChannels*Input_precision> > & out,
	const unsigned int numReps = 1
    ){
	constexpr unsigned int number_blocks = ConvKernelDim + Stride ;
	constexpr unsigned int cycles_write_block = (OFMDim * ConvKernelDim * ConvKernelDim);
	constexpr unsigned int cycles_read_block = IFMDim*Stride;
	constexpr unsigned int max_cycles = MAX(cycles_write_block, cycles_read_block);
	constexpr unsigned int baseIter = IFMDim * ConvKernelDim + OFMDim * max_cycles;
	constexpr unsigned int initial_buffer_cycles = IFMDim * ConvKernelDim;

	unsigned int counter_internal_block = 0;
	unsigned int current_block_write = 0;
	unsigned int previous_block_write = 0;
	unsigned int next_block_write = 0;
	unsigned int current_line = 0;
	unsigned int read_block = 0;


	unsigned int inp = 0, ofm_y = 0, ofm_x = 0, k_y = 0, k_x = 0, current_k_y = 0;

	ap_uint<IFMChannels*Input_precision> inputBuf[number_blocks][IFMDim];
#pragma HLS ARRAY_PARTITION variable=inputBuf complete dim=1

#pragma HLS RESET variable=read_block
#pragma HLS RESET variable=inp

#pragma HLS DEPENDENCE variable=current_block_write intra false
#pragma HLS DEPENDENCE variable=inputBuf inter false
#pragma HLS DEPENDENCE variable=inputBuf intra false

// #pragma HLS RESOURCE variable inputBuf core=RAM_2P_LUTRAM

	for (unsigned int i = 0; i < baseIter; i++) {
#pragma HLS PIPELINE II=1
		if (inp < initial_buffer_cycles) // Initial buffer of PoolDim lines
		{
			ap_uint<IFMChannels*Input_precision> inElem;
			inElem = in.read();
			inputBuf[current_block_write][current_line] = inElem;
			current_line++;
			inp++;
			if (current_line == IFMDim)
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
				unsigned int current_block_read = (ofm_y*Stride + k_y)%number_blocks;
				unsigned int current_line_in_block = ofm_x * Stride + k_x;
				ap_uint<IFMChannels*Input_precision> outElem = inputBuf[current_block_read][current_line_in_block];
				out.write(outElem);
				k_x++;
				if (k_x == ConvKernelDim) {
					k_x = 0;
					k_y++;
					if (k_y == ConvKernelDim) {
						k_y = 0;
						ofm_x ++;
						if (ofm_x == OFMDim) {
							ofm_x = 0;
							ofm_y++;
							if (ofm_y == OFMDim) {
								ofm_y = 0;
								inp = 0;
							}
						}
					}
				}
			}
			if ((counter_internal_block < cycles_read_block - 1) && (read_block<IFMDim)) // In parallel we write in the buffer, in the current block write if we still need to
			{
				ap_uint<IFMChannels*Input_precision> inElem;
				inElem = in.read();

				inputBuf[current_block_write][current_line] = inElem;
	#pragma HLS DEPENDENCE variable=inputBuf intra false
	#pragma HLS DEPENDENCE variable=inputBuf inter false
				current_line++;
				if (current_line == IFMDim) // We read the whole block, we change the next block in which we want to we
				{ // We filled up a block, let's not read until
					current_line = 0;
					read_block++;
					current_block_write++;
					if (current_block_write == number_blocks)
						current_block_write = 0;
#pragma HLS DEPENDENCE variable=current_block_write intra false
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

template<
		// convolution parameters
		unsigned int ConvKernelDim,		// e.g 3 for a 3x3 conv kernel (assumed square)
		unsigned int IFMChannels,		// number of input feature maps
		unsigned int IFMDim,			// width of input feature map (assumed square)
		unsigned int OFMChannels,		// number of output feature maps
		// unsigned int OFMDim,			// IFMDim-ConvKernelDim+1 or less
		unsigned int Stride,

		// matrix-vector unit parameters
		unsigned int SIMDWidth,			// number of SIMD lanes
		unsigned int PECount,			// number of PEs
		unsigned int WMemCount,			// entries in each PEs weight memory
		unsigned int TMemCount,			// entries in each PEs threshold memory

		// precision parameters
		unsigned int WeightsPrecision,	// Number of bits in thresholds
		unsigned int ThresholdPrecision,// Number of bits in thresholds
		unsigned int MacPrecision,		// MAC bitwidth
		unsigned int Input_precision,			// Input data bitwidth
		unsigned int ActivationPrecision,	//Output data bitwidth
		unsigned int ActivationType=0,
		template<int> class type_input 	= ap_uint	// For first layer use int value
>
void ConvolutionalLayer_Same_dsp(stream<ap_uint<IFMChannels * Input_precision> > & in,
		stream<ap_uint<OFMChannels * ActivationPrecision> > & out,
		const ap_uint<SIMDWidth * WeightsPrecision> weightMem[PECount][WMemCount],
		const ap_uint<ThresholdPrecision> threshMem[PECount][TMemCount]) {
#pragma HLS INLINE

	// Number of output windows
	constexpr unsigned int OFMDim = 1 + (IFMDim - Stride) / Stride + (((IFMDim - Stride) % Stride) > 0);

	// Output dimensions of the resize stage
	constexpr unsigned int intermediateDimension = ConvKernelDim + Stride * (OFMDim - 1);

	// compute weight matrix dimension from conv params
	const unsigned int MatrixW = ConvKernelDim * ConvKernelDim * IFMChannels;
	const unsigned int MatrixH = OFMChannels;

	stream<ap_uint<IFMChannels * Input_precision> > resizedInput;
	stream<ap_uint<IFMChannels * Input_precision> > convInp;
	stream<ap_uint<SIMDWidth * Input_precision> > mvIn;
	stream<ap_uint<PECount * ActivationPrecision> > mvOut;

	SameResize<IFMDim, ConvKernelDim, Stride, IFMChannels, Input_precision>(in, resizedInput);
	ConvolutionInputGenerator<ConvKernelDim, IFMChannels, Input_precision, intermediateDimension,
			OFMDim, Stride>(resizedInput, convInp);
	DataWidthConverter_fixed<IFMChannels * Input_precision, SIMDWidth * Input_precision,
			OFMDim * OFMDim * ConvKernelDim * ConvKernelDim>(convInp, mvIn);
	MatrixVector_Precision_dsp<SIMDWidth, PECount,
			WeightsPrecision, ThresholdPrecision, MatrixW, MatrixH, WMemCount, TMemCount, Input_precision, ActivationPrecision, MacPrecision, ActivationType, type_input>(mvIn, mvOut, weightMem,
			threshMem, OFMDim * OFMDim);
	DataWidthConverter_fixed<PECount * ActivationPrecision, OFMChannels * ActivationPrecision,
			OFMDim * OFMDim * (MatrixH / PECount)>(mvOut, out);
}
