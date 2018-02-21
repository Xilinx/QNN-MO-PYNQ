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

#include "ap_int.h"

#define NO_THRESHOLDS 0
#define BINARY_THRESHOLDS 1
#define FULL_THRESHOLDS 2


template<
		  unsigned int OutputPrecision,
		  unsigned int MacPrecision,
		  unsigned int ThresholdPrecision,
		  unsigned int Thresholds
		>
ap_uint<OutputPrecision> ReducedPrecision_Threshold(
		ap_int<MacPrecision> value,
		ap_int<ThresholdPrecision * (Thresholds+1)> thresholds)
{
#pragma HLS PIPELINE II=1

	ap_uint<OutputPrecision> outputValue = 0;
	ap_uint<1> comparisonResult[Thresholds];
#pragma HLS ARRAY_PARTITION variable=comparisonResult complete dim=1

	ap_uint<1> invertResult = thresholds(ThresholdPrecision*(Thresholds), ThresholdPrecision*(Thresholds));

	// Compare against all threshold
	for (unsigned int t = 0; t < Thresholds; t++){
#pragma HLS UNROLL
		ap_int<ThresholdPrecision> curThreshold = thresholds(ThresholdPrecision * (t + 1) - 1, ThresholdPrecision * (t));
		comparisonResult[t] = value >= curThreshold;
	}

	// The quantized value is given by the sum of the comparators responses
	for (unsigned int t = 0; t < Thresholds; t++)
#pragma HLS UNROLL
		outputValue = outputValue + comparisonResult[t];

	if (invertResult)
		for(unsigned int b = 0; b < OutputPrecision; b++){
#pragma HLS UNROLL
			outputValue(b, b) = !outputValue(b, b);
		}

	return outputValue;
}


template<
		  unsigned int OutputPrecision,
		  unsigned int MacPrecision,
		  unsigned int ThresholdPrecision
		>
ap_uint<OutputPrecision> Binary_Threshold(ap_int<MacPrecision> value, ap_int<ThresholdPrecision> threshold){
#pragma HLS PIPELINE II=1

	ap_uint<OutputPrecision> outputValue;
	outputValue(OutputPrecision - 1, 0) = value > threshold ? ap_int<OutputPrecision>(1) : ap_int<OutputPrecision>(-1);

	return outputValue;
}
