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

#ifndef GENERAL_UTILS_H_
#define GENERAL_UTILS_H_

#include <string>
#include <limits.h>
#include <stdlib.h>
#include <stdexcept>
#include <chrono>
#include <vector>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>
#include <thread>


class GeneralUtils {
    public:
        typedef std::chrono::high_resolution_clock::time_point chrono_t;
        static bool dirExists(std::string const &path);
        static bool fileExists(std::string const &path);
        static std::string dirname(std::string const &);
        static std::string filename(std::string const &);
        static std::string abspath(std::string const &);
        static std::string abspathReference(std::string const &, std::string const &);
        static std::string readStringFile(std::string const &);
        static void readStringFile(std::string &, std::string const &);
        static std::vector<char> readBinaryFile(std::string const &);
        static void readBinaryFile(std::vector<char> &, std::string const &);
        static void configureFabric(std::vector<char> const &buffer);
        static void loadBitstreamFile(std::string const &);
        static unsigned int padTo(unsigned int , unsigned int );
        static signed long long getTime(chrono_t &);
        static chrono_t getTimer();
    private:
        GeneralUtils() {};
        ~GeneralUtils() {};
};

#endif
