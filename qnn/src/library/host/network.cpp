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

#include "network.h"

#define debug 1
#include "debug.h"

Network::Network(std::vector<char> const &jsonContent) {
    std::string jsonString(jsonContent.begin(), jsonContent.end());
    this->_networkJson.Parse(jsonString.c_str());
    if (!this->_validateJson()) {
        throw std::runtime_error("Network json is no compatible json file");
    }
}

Network::Network(std::string const &jsonFilepath) : Network(GeneralUtils::readBinaryFile(jsonFilepath)) {}

Network::~Network() {};

bool Network::_validateJson() {
    bool result = this->_networkJson.IsObject();
    if (!result) return false;
    result = result &&
        this->_networkJson.HasMember("parameters");
    if (!result)
        return false;
    result = result &&
        this->_networkJson["parameters"].IsObject();
    if (!result)
        return false;
    result = result &&
        this->_networkJson["parameters"]["MAX_K"].IsInt() &&
        this->_networkJson["parameters"]["MAX_IFM_CH"].IsInt() &&
        this->_networkJson["parameters"]["MAX_IFM_DIM"].IsInt() &&
        this->_networkJson["parameters"]["MAX_OFM_CH"].IsInt() &&
        this->_networkJson["parameters"]["MAX_OFM_DIM"].IsInt() &&
        this->_networkJson["parameters"]["MAX_POOL_SIZE"].IsInt() &&
        this->_networkJson["parameters"]["MAX_POOL_STRIDE"].IsInt() &&
        this->_networkJson["parameters"]["MAX_SIMD"].IsInt() &&
        this->_networkJson["parameters"]["MAX_PE_CONV"].IsInt() &&
        this->_networkJson["parameters"]["MAX_PE_FC"].IsInt() &&
        this->_networkJson["parameters"]["MEM_CHANNELS"].IsInt() &&
        this->_networkJson["parameters"]["ACTIVATION_BITS"].IsInt() &&
        this->_networkJson["parameters"]["WEIGHTS_BITS"].IsInt() &&
        this->_networkJson["parameters"]["THRESHOLDS_BITS"].IsInt() &&
        this->_networkJson["parameters"]["MACC_BITS"].IsInt() &&
        this->_networkJson["parameters"]["DATAWIDTH"].IsInt();
    return result;
}

unsigned int Network::getMaxK() {
    return this->_networkJson["parameters"]["MAX_K"].GetInt();
}

unsigned int Network::getMaxIFMCh() {
    return this->_networkJson["parameters"]["MAX_IFM_CH"].GetInt();
}

unsigned int Network::getMaxIFMDim() {
    return this->_networkJson["parameters"]["MAX_IFM_DIM"].GetInt();
}

unsigned int Network::getMaxOFMCh() {
    return this->_networkJson["parameters"]["MAX_OFM_CH"].GetInt();
}

unsigned int Network::getMaxOFMDim() {
    return this->_networkJson["parameters"]["MAX_OFM_DIM"].GetInt();
}

unsigned int Network::getMaxPoolSize() {
    return this->_networkJson["parameters"]["MAX_POOL_SIZE"].GetInt();
}

unsigned int Network::getMaxPoolStride() {
    return this->_networkJson["parameters"]["MAX_POOL_STRIDE"].GetInt();
}

unsigned int Network::getMaxSIMD() {
    return this->_networkJson["parameters"]["MAX_SIMD"].GetInt();
}

unsigned int Network::getMaxPEConv() {
    return this->_networkJson["parameters"]["MAX_PE_CONV"].GetInt();
}

unsigned int Network::getMaxPEFC() {
    return this->_networkJson["parameters"]["MAX_PE_FC"].GetInt();
}

unsigned int Network::getMemChannels() {
    return this->_networkJson["parameters"]["MEM_CHANNELS"].GetInt();
}

unsigned int Network::getActivationBits() {
    return this->_networkJson["parameters"]["ACTIVATION_BITS"].GetInt();
}

unsigned int Network::getWeightsBits() {
    return this->_networkJson["parameters"]["WEIGHTS_BITS"].GetInt();
}

unsigned int Network::getTreshholdsBits() {
    return this->_networkJson["parameters"]["THRESHOLDS_BITS"].GetInt();
}

unsigned int Network::getMACCBits() {
    return this->_networkJson["parameters"]["MACC_BITS"].GetInt();
}

unsigned int Network::getDatawidth() {
    return this->_networkJson["parameters"]["DATAWIDTH"].GetInt();
}
