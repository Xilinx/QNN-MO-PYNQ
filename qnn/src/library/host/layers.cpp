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

#include "layers.h"

// Layers::Layers(Network &network, std::string const &jsonContent) :
// _network(network), _outDim(0), _inDim(0), _maxBufferSize(0), _maxIterations(1), _maxSplit(0) {
//     this->_layerJson.Parse(jsonContent.c_str());
//     if (!this->_validateJson()) {
//         throw std::runtime_error("Layers json is no layers compatible json file");
//     }
//     this->_parseLayers();
// }

Layers::Layers(Network &network, std::string const &filePath, std::vector<char> const &jsonContent) :
    _noneLayer(*this, network), _network(network), _outDim(0), _inDim(0), _maxBufferSize(0), _maxIterations(1), _maxSplit(0),
    _jsonFolder(GeneralUtils::dirname(GeneralUtils::abspath(filePath))) {
    std::string jsonString(jsonContent.begin(), jsonContent.end());
    this->_layerJson.Parse(jsonString.c_str());
    if (!this->_validateJson()) {
        throw std::runtime_error("Layers json is no layers compatible json file");
    }
    this->_parseLayers();
    this->_noneLayer.function = "none";
}

Layers::Layers(Network &network, std::string const &jsonFilepath) : Layers(network, jsonFilepath, GeneralUtils::readBinaryFile(jsonFilepath)) {}

Layers::~Layers() {};

bool Layers::_validateJson() {
    bool result = this->_layerJson.IsObject();
    if (!result) return false;
    result = result &&
        this->_layerJson.HasMember("network") &&
        this->_layerJson.HasMember("input_image") &&
        this->_layerJson.HasMember("verification_image") &&
        this->_layerJson.HasMember("use_binparams") &&
        this->_layerJson.HasMember("binparam") &&
        this->_layerJson.HasMember("binparam_skip") &&
        this->_layerJson.HasMember("layer_skip") &&
        this->_layerJson.HasMember("layers");
    if (!result)
        return false;
    result = result &&
        this->_layerJson["network"].IsString() &&
        this->_layerJson["input_image"].IsString() &&
        this->_layerJson["verification_image"].IsString() &&
        this->_layerJson["use_binparams"].IsBool() &&
        this->_layerJson["binparam"].IsString() &&
        this->_layerJson["binparam_skip"].IsInt() &&
        this->_layerJson["layer_skip"].IsInt() &&
        this->_layerJson["layers"].IsArray();
    return result;
}

bool Layers::_validateLayer(unsigned int index) {
    if (this->_layerJson["layers"].Size() <=  index)
        return false;
    if (!this->_layerJson["layers"][index].IsObject())
        return false;

    auto const layer = this->_layerJson["layers"][index].GetObject();
    if (!layer.HasMember("func") || !layer["func"].IsString())
        return false;
    std::string func(layer["func"].GetString());
    bool result = true;
    if (func == "maxpool_layer" || func == "conv_layer") {
        result = result &&
            layer.HasMember("kernel_shape") &&
            layer.HasMember("kernel_stride") &&
            layer.HasMember("output") &&
            layer.HasMember("input") &&
            layer.HasMember("padding");
        if (!result)
            return false;
        result = result &&
            layer["kernel_shape"].IsInt() &&
            layer["kernel_stride"].IsInt() &&
            (layer["padding"].IsDouble() || layer["padding"].IsInt()) &&
            layer["output"].IsArray() &&
            layer["input"].IsArray();
        if (!result)
            return false;
        result = result &&
            (layer["output"].Size() == 3) &&
            (layer["input"].Size() == 3);
        if (!result)
            return false;
    }
    if (func == "merge_layer") {
        result = result &&
            layer.HasMember("merge") &&
            layer.HasMember("output_channels") &&
            layer.HasMember("output_dim");
        if (!result)
            return false;
        result = result &&
            layer["merge"].IsInt() &&
            layer["output_channels"].IsInt() &&
            layer["output_dim"].IsInt();
        if (!result)
            return false;
    }
    if (func == "split_layer") {
        result = result &&
            layer.HasMember("split") &&
            layer.HasMember("input_channels") &&
            layer.HasMember("input_dim");
        if (!result)
            return false;
        result = result &&
            layer["split"].IsInt() &&
            layer["input_channels"].IsInt() &&
            layer["input_dim"].IsInt();
        if (!result)
            return false;
    }
    if (func == "fc_layer") {
        result = result &&
            layer.HasMember("input") &&
            layer.HasMember("output");
        if (!result)
            return false;
        result = result &&
            layer["input"].IsInt() &&
            layer["output"].IsInt();
        if (!result)
            return false;
    }
    return result;
}

void Layers::_parseLayers() {
    unsigned int weightIndex = 0;
    bool firstLayer = true;
    unsigned int const maxIFMCh         = this->_network.getMaxIFMCh();
    unsigned int const maxIFMDim        = this->_network.getMaxIFMDim();
    unsigned int const maxOFMCh         = this->_network.getMaxOFMCh();
    unsigned int const maxPEConv        = this->_network.getMaxPEConv();
    unsigned int const maxSIMD          = this->_network.getMaxSIMD();
    unsigned int const datawidth        = this->_network.getDatawidth();
    unsigned int const memChannls       = this->_network.getMemChannels();
    unsigned int const treshholdsBits   = this->_network.getTreshholdsBits();
    unsigned int const activationBits   = this->_network.getActivationBits();
    unsigned int const layerSkip        = this->getLayersSkip();

    unsigned int maxDim = 0;
    bool splitMode = false;
    bool add = false;
    unsigned int layerInSplit = 0;
    unsigned int currentSplit = 0;

    auto const layers = this->_layerJson["layers"].GetArray();
    try {
        for (rapidjson::SizeType i = layerSkip; i < layers.Size(); i++) {
            if (!this->_validateLayer(i)) {
                std::cerr << "Layer " << i << " cannot be parsed from json!" << std::endl;
                throw std::runtime_error("Malformed layer at index " + std::to_string(i));
            }
            struct Layer::Layer layer(*this, this->_network);
            add = false;
            layer.layer = Layers::none;
            layer.function = std::string(layers[i]["func"].GetString());

            if ((layer.function == "conv_layer") ||
                (layer.function == "maxpool_layer")) {
                layer.kernelDim = (unsigned int) layers[i]["kernel_shape"].GetInt();
                layer.stride = (unsigned int) layers[i]["kernel_stride"].GetInt();
                layer.log2stride = (unsigned int) std::log2((float) layer.stride);
                layer.OFMCh = (unsigned int) layers[i]["output"][0].GetInt();
                layer.OFMDim = (unsigned int) layers[i]["output"][1].GetInt();
                layer.IFMCh = (unsigned int) layers[i]["input"][0].GetInt();
                layer.IFMDim = (unsigned int) layers[i]["input"][1].GetInt();
                layer.padding = (double) layers[i]["padding"].GetDouble();
                layer.paddedDim = layer.IFMDim + (2 * layer.padding);
                //the next 2 are not in bytes, maybe rename this parameters:
                layer.convWMem = ((layer.kernelDim * layer.kernelDim * maxOFMCh * maxIFMCh) / (maxPEConv * maxSIMD));
                layer.convTMem = maxOFMCh / maxPEConv;
                layer.convMemBits = datawidth * maxPEConv * (layer.convWMem + (layer.convTMem * std::ceil(treshholdsBits / datawidth)));
                layer.convMem = (layer.convMemBits / memChannls / 8);
                layer.poolInDim = layer.OFMDim;
                layer.poolOutDim = layer.OFMDim;
                layer.poolStride = 0;
                layer.iterations = 1;
                if (layer.OFMCh > maxOFMCh) {
                    layer.iterations = (unsigned int) std::ceil((float) layer.OFMCh / (float) maxOFMCh);
                    layer.OFMCh = (unsigned int) std::ceil(layer.OFMCh/layer.iterations);
                }
                // This is not redundant, IFM and OFM are for the convolutional
                // layer, which can be combined with a maxpool in which the
                // output dimension can be changed!
                layer.inDim = layer.IFMDim;
                layer.inCh = layer.IFMCh;
                layer.outDim = layer.OFMDim;
                layer.outCh = layer.OFMCh;
                //Do not add the layer here, decide that later
            }

            if(layer.function == "split_layer") {
                layer.layer |= Layers::split;
                layer.split = (unsigned int) layers[i]["split"].GetInt();
                layer.inCh = (unsigned int) layers[i]["input_channels"].GetInt();
                layer.inDim = (unsigned int) layers[i]["input_dim"].GetInt();
                layer.outCh = layer.inCh / layer.split;
                layer.outDim = layer.inDim;
                splitMode = true;
                currentSplit = layer.split;
                add = true;
            }

            if(layer.function == "merge_layer") {
                layer.layer |= Layers::merge;
                layer.merge = (unsigned int) layers[i]["merge"].GetInt();
                layer.outCh = (unsigned int) layers[i]["output_channels"].GetInt();
                layer.outDim = (unsigned int) layers[i]["output_dim"].GetInt();
                layer.inCh = layer.outCh / layer.merge;
                layer.inDim = layer.outDim;
                weightIndex += (layer.merge - 1) * layerInSplit;
                layer.weightIndex = layerInSplit;
                splitMode = false;
                layerInSplit = 0;
                currentSplit = 0;
                add = true;
            }

            if (layer.function == "conv_layer") {
                layer.layer |= Layers::conv;
                layer.type = Layers::hw_conv;
                if (layers.Size() > (i + 1)) {
                    if (strcmp(layers[i+1]["func"].GetString(), "maxpool_layer") == 0) {
                        layer.layer |= Layers::max_pool;
                        layer.type = Layers::hw_convpool;
                        layer.poolInDim = (unsigned int) layers[i+1]["input"][1].GetInt();
                        layer.poolOutDim = (unsigned int) layers[i+1]["output"][1].GetInt();
                        layer.poolStride = (unsigned int) std::log2((float) layers[i+1]["kernel_stride"].GetInt());
                        layer.outCh = (unsigned int) layers[i+1]["output"][0].GetInt();
                        layer.outDim = (unsigned int) layers[i+1]["output"][1].GetInt();
                    }
                }
                layer.weightIndex = weightIndex;
                weightIndex += layer.iterations;
                if (splitMode) {
                    layer.split = currentSplit;
                    layerInSplit++;
                }
                add = true;
            }

            if (layer.function == "maxpool_layer") {
                layer.layer |= Layers::max_pool;
            }

            if (layer.function == "fc_layer") {
                layer.layer |= Layers::fc;
                layer.type = Layers::hw_fc;
                layer.input = (unsigned int) layers[i]["input"].GetInt();
                layer.output = (unsigned int) layers[i]["output"].GetInt();
            }

            // Calculate general layer values
            layer.inSize = ((activationBits * maxIFMCh * layer.inDim * layer.inDim) / 8);
            layer.outSize = ((activationBits * maxIFMCh * layer.outDim * layer.outDim) / 8);

            if (add) {
                this->_outDim = layer.outDim;
                this->_outCh = layer.outCh * layer.iterations;
                if (firstLayer) {
                    firstLayer = false;
                    this->_inDim = layer.inDim;
                    this->_inCh = layer.inCh;
                }
            }


            maxDim = (layer.inDim > maxDim) ? layer.inDim : maxDim;
            maxDim = (layer.outDim > maxDim) ? layer.outDim : maxDim;
            this->_maxIterations = (layer.iterations > this->_maxIterations) ? layer.iterations : this->_maxIterations;
            this->_maxSplit = (layer.split > this->_maxSplit) ? layer.split : this->_maxSplit;
            layer.inSplit = splitMode;

            // Filter all layers we don't need in the structure
            if (add) {
                this->_layers.push_back(layer);
            }

        }
    } catch(...) {
        throw;
    }
    this->_maxBufferSize =  GeneralUtils::padTo((activationBits * maxIFMCh * maxDim * maxDim) / 8, apintPadding);
    this->_outMem = GeneralUtils::padTo((activationBits * maxIFMCh * this->_outDim * this->_outDim) / 8, apintPadding);
    this->_outWords = this->_outMem / (datawidth / 8);
    this->_inMem = GeneralUtils::padTo((activationBits * maxIFMCh * this->_inDim * this->_inDim) / 8, apintPadding);
    this->_inWords = this->_inMem / (datawidth / 8);
}

unsigned int Layers::getMaxBufferSize() {
    return this->_maxBufferSize;
}

unsigned int Layers::getMaxIterations() {
    return this->_maxIterations;
}

unsigned int Layers::getMaxSplit() {
    return this->_maxSplit;
}

unsigned int Layers::getOutCh() {
    return this->_outCh;
};

unsigned int Layers::getOutDim() {
    return this->_outDim;
};

unsigned int Layers::getOutWords() {
    return this->_outWords;
};

unsigned int Layers::getOutMem() {
    return this->_outMem;
};

unsigned int Layers::getInCh() {
    return this->_inCh;
};

unsigned int Layers::getInDim() {
    return this->_inDim;
};

unsigned int Layers::getInWords() {
    return this->_inWords;
};

unsigned int Layers::getInMem() {
    return this->_inMem;
};

std::string Layers::getNetwork() {
    return std::string(this->_layerJson["network"].GetString());
};

bool Layers::useBinparams() {
    return this->_layerJson["use_binparams"].GetBool();
};

unsigned int Layers::getBinparamSkip() {
    return this->_layerJson["binparam_skip"].GetInt();
};

unsigned int Layers::getLayersSkip() {
    return this->_layerJson["layer_skip"].GetInt();

};

std::string Layers::getInputImagePath() {
    return GeneralUtils::abspathReference(this->_layerJson["input_image"].GetString(), this->_jsonFolder);
};

std::string Layers::getVerificationImagePath() {
    return GeneralUtils::abspathReference(this->_layerJson["verification_image"].GetString(), this->_jsonFolder);
};

std::string Layers::getBinparamPath() {
    return GeneralUtils::abspathReference(this->_layerJson["binparam"].GetString(), this->_jsonFolder) + "/";
};

struct Layers::Layer &Layers::getLayer(unsigned int layerIndex) {
    return _layers[layerIndex];
}

std::size_t Layers::size() {
    return this->_layers.size();
};

std::vector<Layers::Layer>::const_iterator Layers::begin() const {
    return this->_layers.begin();
}
std::vector<Layers::Layer>::const_iterator Layers::end() const {
    return this->_layers.end();
}

std::vector<Layers::Layer>::const_iterator Layers::getNext(std::vector<Layers::Layer>::const_iterator &from, unsigned int layerType) const {
    for (std::vector<Layers::Layer>::const_iterator iter = from; iter != this->end(); iter++) {
        if ((*iter).layer & layerType) {
            return iter;
        }
    }
    return this->end();
}

std::vector<Layers::Layer>::const_iterator Layers::getNext(unsigned int layerType) const {
    for (std::vector<Layers::Layer>::const_iterator iter = this->begin(); iter != this->end(); iter++) {
        if ((*iter).layer & layerType) {
            return iter;
        }
    }
    return this->end();
}
