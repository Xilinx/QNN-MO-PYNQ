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

#ifndef OFFLOAD_UTILS_H_
#define OFFLOAD_UTILS_H_

#include <bitset>
#include <sys/wait.h>
#include <sys/types.h>
#include <chrono>
#include <thread>
#include <iostream>
#include <iomanip>
#include <unistd.h>
#include "network.h"
#include "layers.h"
#include "general-utils.h"
#include "offload-adapter.h"
#include "platform.h"
#include "logger.h"
#include "jobber.h"


#define DEBUG 1
#include "debug.h"

class OffloadUtils {
    private:
        OffloadUtils() {};
        ~OffloadUtils() {};

        static unsigned long long _wrongPixels;
    public:

        static void padTo(char *, size_t const, char *, size_t const, unsigned int const);



        static bool down(OffloadAdapter::ExtMemBuffer &targetBuffer);

        static void memcpy(char *, char *, size_t);
        static void memset(char *, char, size_t);
        static void memset(ExtMemWord *to, char val, size_t const size);
        static void bitcpy(char *, size_t, char *, size_t, size_t);
        static void concatBuffer(ExtMemWord *, ExtMemWord *, Layers::Layer const &, unsigned int const);
        static void concat(OffloadAdapter::ExtMemBuffer &, OffloadAdapter::ExtMemBuffer &, Layers::Layer const &, unsigned int );
        static void splitBuffer(ExtMemWord *, ExtMemWord *, Layers::Layer const &, unsigned int);
        static void split(OffloadAdapter::ExtMemBuffer &, OffloadAdapter::ExtMemBuffer &, Layers::Layer const &, unsigned int);
        static void mergeBuffer(ExtMemWord *, ExtMemWord *, Layers::Layer const &, unsigned int );
        static void merge(OffloadAdapter::ExtMemBuffer &, OffloadAdapter::ExtMemBuffer &, Layers::Layer const &, unsigned int );
        static void memcpy(OffloadAdapter::ExtMemBuffer &, OffloadAdapter::ExtMemBuffer &, size_t);
        static void swap(OffloadAdapter::ExtMemBuffer &, OffloadAdapter::ExtMemBuffer &);
        static void swpcpy(OffloadAdapter::ExtMemBuffer &, OffloadAdapter::ExtMemBuffer &, size_t);
        static bool equal(char *, size_t const, char*, size_t const);
        static bool verifyBuffers(ExtMemWord *, ExtMemWord *, Network &, unsigned int const, unsigned int const, Logger &);
        static unsigned long long tellPixels();
        static void waitOrWork(Jobber &jobber, OffloadAdapter::ExtMemBuffer &buffer);
        /*
        Deprecated functions
         */
        static void naiveMemcpy(char *to, char *from, size_t size);
};

#endif
