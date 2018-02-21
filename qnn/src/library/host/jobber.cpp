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

#include "jobber.h"

#include <chrono>

#define DEBUG 1
#include "debug.h"

Jobber::Jobber( unsigned int const workers) :
 _running(0), _stop(false) {
        for (unsigned int i = 0; i < workers; i++) {
            std::thread worker(&Jobber::_worker, this);
            this->_workers.push_back(std::move(worker));
        }
}

Jobber::~Jobber() {
    this->_stop = true;
    this->_condition.notify_all();
    for (auto &worker : this->_workers) {
        worker.join();
    }
}

bool Jobber::work() {
    if (this->_jobs.size() > 0) {
        std::unique_lock<std::mutex> locker(this->_lock);
        if (this->_jobs.size() == 0) {
            return false;
        }
        std::function<void()> callback(this->_jobs.front());
        this->_jobs.pop_front();
        locker.unlock();
        this->_running++;
        callback();
        this->_running--;
        this->_condition.notify_all();
        return true;
    }
    return false;
}

void Jobber::_worker() {
    while (true) {
        std::unique_lock<std::mutex> locker(this->_lock);
        this->_condition.wait(locker, [this] () {
                return (this->_jobs.size() > 0 || this->_stop);
            });
        if (this->_stop) {
            break;
        }
        std::function<void()> callback(this->_jobs.front());
        this->_jobs.pop_front();
        locker.unlock();
        this->_running++;
        callback();
        this->_running--;
        this->_condition.notify_all();
    }
}


void Jobber::add(std::function<void()> callback, bool threading) {
    if (threading) {
        std::unique_lock<std::mutex> locker(this->_lock);
        this->_jobs.emplace_back(callback);
        locker.unlock();
        this->_condition.notify_all();
    } else {
        callback();
    }
}

void Jobber::wait() {
    std::unique_lock<std::mutex> locker(this->_lock);
    this->_condition.wait(locker, [this] () {
            return ((this->_jobs.size() == 0) && (this->_running == 0));
        });
}

bool Jobber::running() {
    if (this->_jobs.size() == 0 && this->_running == 0) {
        return false;
    } else {
        return true;
    }
}
