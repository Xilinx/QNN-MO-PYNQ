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

#include <iostream>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <string>
#include <unistd.h>
#include <sstream>
#include <list>
#include <atomic>
#include <signal.h>
#include <iomanip>
#include <memory>
#include <vector>
#include <map>
#ifndef NOZIP
#include <zip.h>
#endif

#include "general-utils.h"
#include "offload-utils.h"
#include "offload-adapter.h"
#include "jobber.h"
#include "network.h"
#include "layers.h"
#include "platform.h"
#include "logger.h"

void dump_to_file(std::string filename, char * buffer, size_t const size) {
    std::ofstream ofile(filename, std::ios::out | std::ios::trunc);
    ofile.write(buffer, size);
    ofile.close();
}


extern "C" {
    void initParameters(unsigned int const batch, unsigned int const threads);
    void initAccelerator(char const *networkJson, char const *layerJson);
#ifndef NOZIP
    void initAcceleratorZip(char const *zipPath);
#endif
    void singleInference(char *in, size_t const inSize, char *out, size_t const outSize);
    void deinitAccelerator();
}

namespace {
    std::atomic<unsigned long long> duration(0);
    std::atomic<unsigned long long> splitTime(0);
    std::atomic<unsigned long long> splitBufferTime(0);
    std::atomic<unsigned long long> mergeTime(0);
    std::atomic<unsigned long long> weightsTime(0);
    std::atomic<unsigned long long> prepareTime(0);
    std::atomic<unsigned long long> offloadTime(0);
    std::atomic<unsigned long long> concatTime(0);
    std::atomic<unsigned long long> swpcpyTime(0);
    std::atomic<unsigned long long> swapTime(0);
    std::atomic<unsigned long long> resultTime(0);
    std::atomic<unsigned long long> inputTime(0);

    unsigned int batchSize = 1;
    unsigned int imageCount = 1;
    unsigned int threadCount = 0;
    bool verbose = false;
    bool threading = false;
    bool inputTiming = false;

    std::unique_ptr<Network>        network;
    std::unique_ptr<Layers>         layers;
    std::unique_ptr<OffloadAdapter> adapter;
    std::unique_ptr<Jobber>         jobber;

    std::vector<OffloadAdapter::ExtMemBuffer *> resultBuffers;
    std::vector<OffloadAdapter::ExtMemBuffer *> concatBuffers;
    std::vector<OffloadAdapter::ExtMemBuffer *> mergeBuffers;
    std::vector<OffloadAdapter::ExtMemBuffer *> testBuffers;
    std::vector<std::vector<OffloadAdapter::ExtMemBuffer *>> splitBuffers;

    Logger stdOut(std::cout, verbose);
    Logger stdErr(std::cerr, verbose);
    Logger::Verbosity verboseIgnore(true);
    Logger::Verbosity verboseNone(false);

    bool initialized = false;


}

void initParameters(unsigned int const batch, unsigned int const threads) {
    if (initialized)
        return;

    batchSize = (batch > 0) ? batch : 1;
    threadCount = threads;
}

void _init() {
    threading = (threadCount == 0) ? false : true;

    adapter.reset(new OffloadAdapter(layers->getNetwork(), network->getMemChannels(), layers->getMaxBufferSize()));
    jobber.reset(new Jobber(threadCount));

    if (layers->useBinparams()) {
        adapter->loadWeights(*network, *layers);
    };

    unsigned int const maxIterations = layers->getMaxIterations();
    unsigned int const maxSplits = layers->getMaxSplit();

    // batchSize of hardware buffers for the inputs, 2 for output buffers
    // even a single threaded call tries to do work parallel to hardware
    // that needs at least one more output buffer to not block the applciation
    unsigned int const minHardwareBuffers = batchSize + 2;
    unsigned int hardwareBufferCount = 0;
    stdOut << "Initializing a minimum of " << minHardwareBuffers << " hardware buffers of size " << adapter->getBufferSize() << " bytes..." << std::endl;
    // Threading is drastically improved through free hardware buffers
    // In case we are in hardware aquire as many buffers as possible
    // In case of software aquire the minimum amount, because sw is
    // already slow and not threaded at the moment
    hardwareBufferCount += adapter->reserveBuffers(minHardwareBuffers, EXTMEMBUFFER_HARDWARE);
    if (hardwareBufferCount < minHardwareBuffers) {
        throw std::runtime_error("Could not initizialize " + std::to_string(minHardwareBuffers) + " required hardware buffers!");
    }
    stdOut << "Initialized minimum of " << hardwareBufferCount << " hardware buffers!" << std::endl;


    unsigned int const minLocalBuffers = ((maxIterations > 1) ? batchSize : 0)
        + ((maxSplits > 1) ? batchSize + (batchSize * maxSplits) : 0)
        + batchSize;
    unsigned int localBufferCount = 0;
    stdOut << "Initializing a minimum of " << minLocalBuffers << " local buffers of size " << adapter->getBufferSize() << " bytes..." << std::endl;
    localBufferCount += adapter->reserveBuffers(minLocalBuffers, EXTMEMBUFFER_LOCAL);;
    if (localBufferCount < minLocalBuffers) {
        throw std::runtime_error("Could not initizialize " + std::to_string(minLocalBuffers) + " required local buffers!");
    }
    stdOut << "Initialized " << localBufferCount << " local buffers!" << std::endl;

    //Concat buffers are only used for multi iterations
    concatBuffers.resize((maxIterations > 1) ? batchSize : 0);
    for (auto &buf : concatBuffers) {
        buf = &adapter->getBuffer(EXTMEMBUFFER_LOCAL);
    }

    mergeBuffers.resize((maxSplits > 1) ? batchSize : 0);
    for (auto &buf : mergeBuffers) {
        buf = &adapter->getBuffer(EXTMEMBUFFER_LOCAL);
    }

    resultBuffers.resize(batchSize);
    for (auto &buf : resultBuffers) {
        buf = &adapter->getBuffer(EXTMEMBUFFER_LOCAL);
    }

    splitBuffers.resize(batchSize);
    for (auto &buffers : splitBuffers) {
        buffers.resize(maxSplits);
        for (auto &buf: buffers) {
            buf = &adapter->getBuffer(EXTMEMBUFFER_LOCAL);
        }
    }

    testBuffers.resize(batchSize);
    for (auto &buf : testBuffers) {
        buf = &adapter->getBuffer(EXTMEMBUFFER_HARDWARE);
    }

    if (threading) {
        stdOut << "Initializing " << batchSize << " hardware buffers to increase thread performance..." << std::endl;
        hardwareBufferCount += adapter->reserveBuffers(batchSize, EXTMEMBUFFER_HARDWARE);
        stdOut << "Initialized a total of " << hardwareBufferCount << " hardware buffers!" << std::endl;
    }

    initialized = true;
}

#ifndef NOZIP
void initAcceleratorZip(char const *zipPath) {
    if (initialized)
        return;

    static const std::pair<char const *, std::function<void(std::vector<char> &)>> extractFiles[] = {
        { "bitstream" , [](std::vector<char> &buffer){
            stdOut << "Loading Bitstream from zip..." << std::endl;
            GeneralUtils::configureFabric(buffer);
        }},
        { "network.json" , [](std::vector<char> &buffer){
            stdOut << "Loading network json from zip..." << std::endl;
            network.reset(new Network(buffer));
        }},
        { "layers.json" , [zipPath](std::vector<char> &buffer){
            stdOut << "Loading layers json from zip..." << std::endl;
            layers.reset(new Layers(*network, std::string(zipPath), buffer));
        }}
    };

    zip *zipFile = zip_open(zipPath, 0, NULL);
    if (zipFile == NULL) {
        throw std::runtime_error("Could not open zip file " + std::string(zipPath));
    }

    for (auto & extractFile : extractFiles) {
        std::vector<char> buffer;
        struct zip_stat stats;
        zip_stat_init(&stats);
        if (zip_stat(zipFile, extractFile.first, 0, &stats) < 0) {
            throw std::runtime_error("Could not find " + std::string(extractFile.first) + " in zip file!");
        }

        buffer.resize(stats.size);
        zip_file *f = zip_fopen(zipFile, extractFile.first, 0);
        zip_fread(f, buffer.data(), buffer.size());
        zip_fclose(f);

        extractFile.second(buffer);
    }
    zip_close(zipFile);

    _init();
}
#endif

void initAccelerator(char const *networkJson, char const *layerJson) {
    if (initialized)
        return;

    std::string networkJsonPath(networkJson);
    std::string layersJsonPath(layerJson);

    network.reset(new Network(networkJsonPath));
    layers.reset(new Layers(*network, layersJsonPath));

    _init();
}

void deinitAccelerator() {
    if (!initialized)
        return;

    jobber.reset();
    adapter.reset();
    layers.reset();
    network.reset();
    splitBuffers.clear();
    testBuffers.clear();
    resultBuffers.clear();
    concatBuffers.clear();
    initialized = false;
}

void inference(unsigned int const batch = 1) {
    unsigned int layerIndex = 0;
    bool splitMode = false;
    unsigned int splitIndex = 0;
    unsigned int splitWeightOffset = 0;
    GeneralUtils::chrono_t timer = GeneralUtils::getTimer();
    std::vector<Layers::Layer>::const_iterator layerIter = layers->begin();
    std::vector<Layers::Layer>::const_iterator layerSplitIter = layers->end();
    while (layerIter != layers->end()) {
        Layers::Layer const &layer = (*layerIter);
        Layers::Layer const &nextLayer = ((layerIter + 1) == layers->end()) ? layers->getNoneLayer() : (*(layerIter + 1));
        if (layer.layer & Layers::split) {
            stdOut << "\t" << layer.function << "[" << layerIndex << "]"  << std::endl;
            for (unsigned int k = 0; k < batch; k++) {
                stdOut << "\t> Prepare new buffers for batch image " << k << " and split run " << splitIndex << "..." << std::endl;
                testBuffers[k]->waitPending();
                testBuffers[k]->setTarget(1);
                jobber->add([splitMode, k, splitIndex, &layer](){
                    if (!splitMode) {
                        GeneralUtils::chrono_t timer = GeneralUtils::getTimer();
                        for (unsigned int s = 0; s < layer.split; s++) {
                            OffloadUtils::split(*splitBuffers[k][s], *testBuffers[k], layer, s);
                        }
                        splitTime += GeneralUtils::getTime(timer);
                    }
                    GeneralUtils::chrono_t timer = GeneralUtils::getTimer();
                    OffloadUtils::memcpy(*testBuffers[k], *splitBuffers[k][splitIndex], layer.outSize);
                    OffloadUtils::down(*testBuffers[k]);
                    splitBufferTime += GeneralUtils::getTime(timer);
                }, threading);
            }
            if (!splitMode) {
                layerSplitIter = layerIter;
                splitMode = true;
                splitIndex = 0;
                splitWeightOffset = 0;
            }
        } else if (layer.layer & Layers::merge) {
            stdOut << "\t" << layer.function << "[" << layerIndex << "]"  << std::endl;
            if ( splitIndex < layer.merge - 1) {
                stdOut << "\t> Return to split layer..." << std::endl;
                splitIndex++;
                splitWeightOffset += layer.weightIndex;
                layerIter = layerSplitIter;
                continue;
            }
            splitMode = false;
            splitIndex = 0;
            splitWeightOffset = 0;
        } else if (layer.layer & Layers::conv) {
            stdOut << "\t" << layer.function << "[" << layerIndex << "]"  << std::endl;
            stdOut << "\t> Iterations: " << layer.iterations << std::endl;
                for (unsigned int j = 0; j < layer.iterations; j++) {
                    if (layers->useBinparams()) {
                        stdOut << "\t> [" << j << "] Loading weights: " << layer.weightIndex + j + splitWeightOffset << std::endl;
                        GeneralUtils::chrono_t weightsTimer = GeneralUtils::getTimer();
                        adapter->offloadWeights(layer, j + splitWeightOffset);
                        adapter->exec();
                        weightsTime += GeneralUtils::getTime(weightsTimer);
                    }
                    stdOut << "\t> [" << j << "] Offloading..." << std::endl;
                    for (unsigned int k = 0; k < batch; k++) {
                        GeneralUtils::chrono_t offloadTimer = GeneralUtils::getTimer();
                        OffloadAdapter::ExtMemBuffer &inputBuffer  = *(testBuffers[k]);
                        OffloadAdapter::ExtMemBuffer &outputBuffer = adapter->getBuffer(EXTMEMBUFFER_HARDWARE);

                        if (j == 0) {
                            inputBuffer.waitPending();
                            inputBuffer.setTarget(1);
                        }

                        prepareTime += GeneralUtils::getTime(offloadTimer);
                        stdOut << "\t> [" << j << "] Process image " << k << "... ";
                        offloadTimer = GeneralUtils::getTimer();

                        adapter->offload(inputBuffer, outputBuffer, layer);
                        adapter->execAsync();

                        do {
                            if (!jobber->work()) {
                                adapter->wait();
                            } else {
                            }
                        } while (adapter->running());
                        adapter->sync();


                        offloadTime += GeneralUtils::getTime(offloadTimer);
                        stdOut << " done" << std::endl;

                        if (layer.iterations > 1) {
                            OffloadAdapter::ExtMemBuffer &concatBuffer = *(concatBuffers[k]);
                            if (j == 0) {
                                concatBuffer.setTarget(layer.iterations);
                            }
                            jobber->add([&inputBuffer, &outputBuffer, &concatBuffer, &layer, j](){
                                    GeneralUtils::chrono_t timer = GeneralUtils::getTimer();
                                    OffloadUtils::concat(concatBuffer, outputBuffer, layer, j);
                                    outputBuffer.release();
                                    concatTime += GeneralUtils::getTime(timer);
                                    if(OffloadUtils::down(concatBuffer)) {
                                        timer = GeneralUtils::getTimer();
                                        OffloadUtils::swpcpy(inputBuffer, concatBuffer, layer.inSize);
                                        OffloadUtils::down(inputBuffer);
                                        swpcpyTime += GeneralUtils::getTime(timer);
                                    }
                                }, threading || (j+1 < layer.iterations));
                        } else {
                            if (nextLayer.layer & Layers::merge) {
                                OffloadAdapter::ExtMemBuffer &mergeBuffer = *(mergeBuffers[k]);
                                if (splitIndex == 0) {
                                    mergeBuffer.waitPending();
                                    mergeBuffer.setTarget(nextLayer.merge);
                                }

                                stdOut << "\t> Merging split " << splitIndex << " of batch image " << k << "..." << std::endl;
                                jobber->add([k, splitIndex, &mergeBuffer, &outputBuffer, &layer, &nextLayer](){
                                    GeneralUtils::chrono_t timer = GeneralUtils::getTimer();
                                    OffloadUtils::mergeBuffer(mergeBuffer.buffer, outputBuffer.buffer, nextLayer, splitIndex);
                                    outputBuffer.release();
                                    OffloadUtils::down(mergeBuffer);
                                    mergeTime += GeneralUtils::getTime(timer);
                                }, threading || splitIndex < (nextLayer.merge - 1));

                                if (splitIndex + 1 == nextLayer.merge) {
                                    stdOut << "\t> Copy merged layer back to batch image " << k << "..." << std::endl;
                                    jobber->add([k, &inputBuffer, &mergeBuffer, &nextLayer](){
                                        mergeBuffer.waitPending();
                                        GeneralUtils::chrono_t timer = GeneralUtils::getTimer();
                                        OffloadUtils::memcpy(inputBuffer, mergeBuffer, nextLayer.outSize);
                                        OffloadUtils::down(inputBuffer);
                                        mergeTime += GeneralUtils::getTime(timer);
                                    }, threading);
                                } else {
                                    // the input buffer can be reused by the split layer
                                    OffloadUtils::down(inputBuffer);
                                }

                            } else {
                                jobber->add([&inputBuffer, &outputBuffer](){
                                        GeneralUtils::chrono_t timer = GeneralUtils::getTimer();
                                        OffloadUtils::swap(inputBuffer, outputBuffer);
                                        OffloadUtils::down(inputBuffer);
                                        outputBuffer.release();
                                        swapTime += GeneralUtils::getTime(timer);
                                    }, threading);
                            }
                        }
                    }
                } // for layer.iterations
            layerIndex++;
            stdOut << std::endl;
        } //end if conv_layer
        std::advance(layerIter, 1);
    } // for layers
    duration += GeneralUtils::getTime(timer);
    stdOut << std::endl;
}

void singleInference(char *in, size_t const inSize, char *out, size_t const outSize) {
    if (!initialized)
        return;

    stdOut << "Got input with " << inSize << " bytes..." << std::endl;
    if (inSize != layers->getInMem()) {
        stdOut << "Padding downto/to " <<  layers->getInMem() << " bytes..." << std::endl;
        OffloadUtils::padTo((char *) testBuffers[0]->buffer, layers->getInMem(), in, inSize, layers->getInDim() * layers->getInDim());
    } else {
        stdOut << "Memcpy to input buffer... " << std::endl;
        OffloadUtils::memcpy((char *) testBuffers[0]->buffer, in, inSize);
    }
    inference(1);
    testBuffers[0]->wait();
    testBuffers[0]->waitPending();
    stdOut << "Got output with " << outSize << " bytes..." << std::endl;
    if (outSize != layers->getOutMem()) {
        stdOut << "Padding downto/to " <<  layers->getOutMem() << " bytes..." << std::endl;
        OffloadUtils::padTo(out, outSize, (char *) testBuffers[0]->buffer, layers->getOutMem(), layers->getOutDim() * layers->getOutDim());
    } else {
        stdOut << "Memcpy to output buffer... " << std::endl;
        OffloadUtils::memcpy(out, (char *) testBuffers[0]->buffer, outSize);
    }
}


bool toUnsignedInt(char *from, unsigned int &to) {
    std::istringstream ss(from);
    unsigned int test;
    if (ss >> test) {
        to = test;
        return true;
    }
    return false;
}

void printHelp(char opt = 0, char *optarg = NULL) {
    if (opt != 0) {
        stdErr << "Invalid parameter -" << char(opt);
        if (optarg) {
            stdErr << " " << optarg;
        }
        stdErr << std::endl;
    }
    srand (time(NULL));
    stdErr << "QNN Testbench" << std::endl;
    stdErr << "\t -i <imageCount> Number of images to process" << std::endl;
    stdErr << "\t -b <batchSize>\t Batch size" << std::endl;
    stdErr << "\t -t <threads> \t Number of worker threads" << std::endl;
    stdErr << "\t -n <path> \t Network description json file" << std::endl;
    stdErr << "\t -l <path> \t Layers description json file" << std::endl;
    stdErr << "\t -z <path> \t Zip package (disables -l and -n)" << std::endl;
    stdErr << "\t -v \t\t increase verbosity" << std::endl;
    if (rand() % 100 < 20) {
        stdErr << "\t -a \t\t baaad timings" << std::endl;
    }
    stdErr << "\t -h \t\t show this help" << std::endl;
}





void sigHandler(int) {
    deinitAccelerator();
}

int main(int argc, char **argv) {
    // cleanup hardware on STRG+C
    signal(SIGINT, sigHandler);
    stdOut.on();
    stdErr.on();
    GeneralUtils::chrono_t timer;
    std::string networkJsonPath;
    std::string layersJsonPath;
    std::string zipPath;
    unsigned int result = 0;
    unsigned int correctImages  = 0;

    threadCount = std::thread::hardware_concurrency();
    threading = true;

    char *env = getenv("QNN_NETWORK_JSON");
    if (env) {
        networkJsonPath = env;
    }
    env = getenv("QNN_LAYERS_JSON");
    if (env) {
        layersJsonPath = env;
    }
    int opt;
    while ((opt = getopt(argc, argv, "ahvn:l:i:t:b:z:")) != -1) {
        switch (opt) {
            case 'n':
                networkJsonPath = optarg;
                break;
            case 'l':
                layersJsonPath = optarg;
                break;
            case 'z':
                zipPath = optarg;
                break;
            case 'v':
                verbose = true;
                break;
            case 'a':
                inputTiming = true;
                break;
            case 'i':
                if (!toUnsignedInt(optarg, imageCount)) {
                    printHelp(opt, optarg);
                    return 1;
                }
                break;
            case 't':
                if (!toUnsignedInt(optarg, threadCount)) {
                    printHelp(opt, optarg);
                    return 1;
                }
                if (threadCount == 0) {
                    threading = false;
                }
                break;
            case 'b':
                if (!toUnsignedInt(optarg, batchSize)) {
                    printHelp(opt, optarg);
                    return 1;
                }
                batchSize = (batchSize == 0) ? 1 : batchSize;
                break;
            case '?':
            case 'h':
                printHelp();
                return 0;
            default:
                printHelp(opt);
                return 1;
                break;
        }
    }

    if (zipPath.size() == 0) {
        if (networkJsonPath.size() == 0) {
            stdErr << "Please specifiy network json file through n parameter or QNN_NETWORK_JSON enviroment variable!" << std::endl;
            printHelp();
            return 1;
        }

        if (layersJsonPath.size() == 0) {
            stdErr << "Please specifiy layers json file through l parameter or QNN_LAYERS_JSON enviroment variable!" << std::endl;
            printHelp();
            return 1;
        }
    }

    unsigned const int batchIterations = std::ceil((float) imageCount/ (float) batchSize);

    try {
        Logger::Verbosity verboseLevel(verbose);

        stdOut <<verboseIgnore;
        stdOut << "QNN Testbench" << std::endl;
        stdOut << "Images to test:   " << imageCount << std::endl;
        stdOut << "Batch size:       " << batchSize << std::endl;
        stdOut << "Batch iterations: " << batchIterations << std::endl;
        stdOut << "Threading:        " << threading << std::endl;
        if (threading) {
            stdOut << "Worker threads:   " << threadCount << std::endl;
        } else {
            threadCount = 0;
        }
        stdOut << std::endl;
        stdOut << verboseLevel;
        stdErr << verboseLevel;

        if (zipPath.size() == 0) {
            initAccelerator(networkJsonPath.c_str(), layersJsonPath.c_str());
        } else {
#ifndef NOZIP
            initAcceleratorZip(zipPath.c_str());
#else
            throw std::runtime_error("zip capability was not compiled in");
#endif
        }

        stdOut << "Network is " << layers->getNetwork() << std::endl;

        if (layers->size() == 0) {
            throw std::runtime_error("There are no layers specified, maybe layer_skip is to high!");
        }

        // Load the verification images
        std::vector<char> resultImage;
        std::string resultFilename = layers->getVerificationImagePath();
        stdOut << "Loading result images from " << resultFilename << "..." << std::endl;
        GeneralUtils::readBinaryFile(resultImage, resultFilename);
        stdOut << "read " << resultImage.size() << " bytes!" << std::endl;

        // load test images from binary files
        std::vector<char> inputImage;
        std::string imageFilename = layers->getInputImagePath();
        stdOut << "Loading test images from " << imageFilename << "..." << std::endl;
        GeneralUtils::readBinaryFile(inputImage, imageFilename);
        stdOut << "read " << inputImage.size() << " bytes!" << std::endl;

        OffloadAdapter::ExtMemBuffer &inputImagePadded = adapter->getBuffer(EXTMEMBUFFER_LOCAL);
        if (inputImage.size() < layers->getInMem()) {
            stdOut << "Input image only contains " << inputImage.size() << " bytes, need a padding to " << layers->getInMem() << " bytes..." << std::endl;
            OffloadUtils::padTo((char *) inputImagePadded.buffer, layers->getInMem(), (char *) inputImage.data(), inputImage.size() , layers->getInDim() * layers->getInDim());
        } else {
            std::memcpy(inputImagePadded.buffer, inputImage.data(),  layers->getInMem());
            stdOut << "Copied " << layers->getInMem() << " bytes from input image" << std::endl;
        }

        stdOut << std::endl << std::endl;
        for (unsigned int i = 0; i < batchIterations; i++) {
            unsigned int const currentBatchSize = ((i * batchSize) + batchSize > imageCount) ? imageCount - (i * batchSize)  : batchSize;
            stdOut << verboseIgnore << "Batch run " << i << " processing " << currentBatchSize << " images" << std::endl << verboseLevel;

            for (unsigned int k = 0; k < currentBatchSize; k++) {
                testBuffers[k]->setTarget(1);
                jobber->add([k, &inputImagePadded](){
                    GeneralUtils::chrono_t timer = GeneralUtils::getTimer();
                    OffloadUtils::memcpy(*testBuffers[k], inputImagePadded, inputImagePadded.size());
                    OffloadUtils::down(*testBuffers[k]);
                    inputTime += GeneralUtils::getTime(timer);
                }, threading && inputTiming);
            }

            inference(currentBatchSize);

            for (unsigned int k = 0; k < currentBatchSize; k++) {
                stdOut << "\t> Get result of image " << k << std::endl;
                GeneralUtils::chrono_t timer = GeneralUtils::getTimer();
                testBuffers[k]->waitPending();
                OffloadUtils::memcpy(*resultBuffers[k], *testBuffers[k], layers->getOutMem());
                resultTime += GeneralUtils::getTime(timer);
                stdOut << "\t> Copied " << layers->getOutMem() << " bytes from the result" << std::endl;
            }

            for (int k = 0; k < currentBatchSize; k++) {
                // dump_to_file("/tmp/accel_out_" + std::to_string(k) + ".bin",(char *) resultBuffers[k]->buffer, layers->getOutMem());
                unsigned int const currentImage = (i * batchSize) + k;
                if (OffloadUtils::verifyBuffers((ExtMemWord *) resultImage.data(), resultBuffers[k]->buffer, *network, layers->getOutCh(), layers->getOutDim(), stdErr)) {
                    stdOut << "Verification of image " << currentImage << " succeeded!" << std::endl;
                    correctImages++;
                } else {
                    const unsigned long long outPixels = layers->getOutDim() * layers->getOutDim();
                    unsigned long long const correctPixels = outPixels - OffloadUtils::tellPixels();
                    stdErr << verboseIgnore << "Verification of image " << currentImage << " failed!" << std::endl;
                    stdErr << correctPixels << "/" << outPixels << " pixels are correct, accuracy " << std::fixed << std::setprecision(2) << (float) 100 * ((float) correctPixels / (float) outPixels) << "%" << std::endl;
                    stdErr << verboseLevel;
                    result = 1;
                }
            }
            stdOut << std::endl;
        } // for batchIterations

        duration += resultTime;
        if (inputTiming) {
            duration += inputTime;
        }

        size_t maxLen = std::to_string(duration).length();
        stdOut << verboseIgnore << std::endl;
        stdOut << correctImages << "/" << imageCount << " images correct, accuracy " << (100.0 * (float) correctImages / imageCount) << "%" << std::endl;
        stdOut << "━┳━Overall    " << std::setw(maxLen) << duration    << " us, " << std::fixed << std::setprecision(2) << std::setw(maxLen) << ((float) duration    / 1000) << " ms, " << std::setw(maxLen - 3) << ((float) duration    / 1000000) << "s, 100.00%" << std::endl;
        if (inputTiming) {
            stdOut << " ┣━Inputs     " << std::setw(maxLen) << inputTime   << " us, " << std::fixed << std::setprecision(2) << std::setw(maxLen) << ((float) inputTime   / 1000) << " ms, " << std::setw(maxLen - 3) << ((float) inputTime   / 1000000) << "s, " << std::setw(6) << ((float) inputTime   * 100 / duration) << "%" << std::endl;
        }
        stdOut << " ┣┳━Splitting " << std::setw(maxLen) << splitTime       << " us, " << std::fixed << std::setprecision(2) << std::setw(maxLen) << ((float) splitTime       / 1000) << " ms, " << std::setw(maxLen - 3) << ((float) splitTime       / 1000000) << "s, " << std::setw(6) << ((float) splitTime       * 100 / duration) << "%" << std::endl;
        stdOut << " ┃┣━Prepare   " << std::setw(maxLen) << splitBufferTime << " us, " << std::fixed << std::setprecision(2) << std::setw(maxLen) << ((float) splitBufferTime / 1000) << " ms, " << std::setw(maxLen - 3) << ((float) splitBufferTime / 1000000) << "s, " << std::setw(6) << ((float) splitBufferTime * 100 / duration) << "%" << std::endl;
        stdOut << " ┣┻━Merging   " << std::setw(maxLen) << mergeTime       << " us, " << std::fixed << std::setprecision(2) << std::setw(maxLen) << ((float) mergeTime       / 1000) << " ms, " << std::setw(maxLen - 3) << ((float) mergeTime       / 1000000) << "s, " << std::setw(6) << ((float) mergeTime       * 100 / duration) << "%" << std::endl;
        stdOut << " ┣━Weights    " << std::setw(maxLen) << weightsTime     << " us, " << std::fixed << std::setprecision(2) << std::setw(maxLen) << ((float) weightsTime     / 1000) << " ms, " << std::setw(maxLen - 3) << ((float) weightsTime     / 1000000) << "s, " << std::setw(6) << ((float) weightsTime     * 100 / duration) << "%" << std::endl;
        stdOut << " ┣┳━Prepare   " << std::setw(maxLen) << prepareTime     << " us, " << std::fixed << std::setprecision(2) << std::setw(maxLen) << ((float) prepareTime     / 1000) << " ms, " << std::setw(maxLen - 3) << ((float) prepareTime     / 1000000) << "s, " << std::setw(6) << ((float) prepareTime     * 100 / duration) << "%" << std::endl;
        stdOut << " ┃┣━Offload   " << std::setw(maxLen) << offloadTime     << " us, " << std::fixed << std::setprecision(2) << std::setw(maxLen) << ((float) offloadTime     / 1000) << " ms, " << std::setw(maxLen - 3) << ((float) offloadTime     / 1000000) << "s, " << std::setw(6) << ((float) offloadTime     * 100 / duration) << "%" << std::endl;
        stdOut << " ┃┣┳━Concat   " << std::setw(maxLen) << concatTime      << " us, " << std::fixed << std::setprecision(2) << std::setw(maxLen) << ((float) concatTime      / 1000) << " ms, " << std::setw(maxLen - 3) << ((float) concatTime      / 1000000) << "s, " << std::setw(6) << ((float) concatTime      * 100 / duration) << "%" << std::endl;
        stdOut << " ┃┃┗━SwapCpy  " << std::setw(maxLen) << swpcpyTime     << " us, " << std::fixed << std::setprecision(2) << std::setw(maxLen) << ((float) swpcpyTime     / 1000) << " ms, " << std::setw(maxLen - 3) << ((float) swpcpyTime     / 1000000) << "s, " << std::setw(6) << ((float) swpcpyTime     * 100 / duration) << "%" << std::endl;
        stdOut << " ┃┗━Swap      " << std::setw(maxLen) << swapTime        << " us, " << std::fixed << std::setprecision(2) << std::setw(maxLen) << ((float) swapTime        / 1000) << " ms, " << std::setw(maxLen - 3) << ((float) swapTime        / 1000000) << "s, " << std::setw(6) << ((float) swapTime        * 100 / duration) << "%" << std::endl;
        stdOut << " ┗━Results    " << std::setw(maxLen) << resultTime      << " us, " << std::fixed << std::setprecision(2) << std::setw(maxLen) << ((float) resultTime      / 1000) << " ms, " << std::setw(maxLen - 3) << ((float) resultTime      / 1000000) << "s, " << std::setw(6) << ((float) resultTime      * 100 / duration) << "%" << std::endl;
        stdOut << "> " << std::fixed << std::setprecision(4) << (float) (1000000 * imageCount) / duration << " fps" << std::endl;

        deinitAccelerator();
    } catch(...) {
        deinitAccelerator();
        throw;
    }
    return result;
}
