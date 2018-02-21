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

#ifndef LOGGER_H_
#define LOGGER_H_

#include <iostream>

class Logger {

public:
    class Verbosity {
        public:
            Verbosity(bool verbose = true) : verbose(verbose) {};
            inline friend Logger &operator<<(Logger &l, const Verbosity &obj) {
                if (obj.verbose) {
                    l.on();
                } else {
                    l.off();
                }
                return l;
            }
            bool verbose;
    };

    Logger() : Logger(std::cout, true) {};
    Logger(bool on) : Logger(std::cout, on) {};
    Logger(std::ostream &out) : Logger(out, true) {};
    Logger(std::ostream &out, bool on) : _out(out), _on(on) {};

    void on() {
        this->_on = true;
    }

    void off() {
        this->_on = false;
    }

    bool active() {
        return this->_on;
    }

    template <typename T>
    Logger& operator<<( T const& t) {
        if (this->_on) {
            this->_out << t;
        }
        return *this;
    }

    Logger& operator<<(std::ostream& (*f)(std::ostream&)) {
        if (this->_on) {
            this->_out << f;
        }
        return *this;
    }

private:
    std::ostream &_out;
    bool _on;
};



#endif
