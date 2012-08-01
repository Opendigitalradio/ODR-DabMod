/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Written by
   2012, Matthias P. Braendli, matthias.braendli@mpb.li
 */
/*
   This file is part of CRC-DADMOD.

   CRC-DADMOD is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   CRC-DADMOD is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with CRC-DADMOD.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef FIRFILTER_H
#define FIRFILTER_H

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include <boost/thread.hpp>
#include <queue>

#include "ModCodec.h"
#include "PcDebug.h"

#include <sys/types.h>
#include <complex>

#include <time.h>
#include <cstdio>

#define FIRFILTER_PIPELINE_DELAY 1

typedef std::complex<float> complexf;

template<typename T>
class ThreadsafeQueue
{
private:
    std::queue<T> the_queue;
    mutable boost::mutex the_mutex;
    boost::condition_variable the_condition_variable;
public:
    void push(T const& val)
    {
        boost::mutex::scoped_lock lock(the_mutex);
        the_queue.push(val);
        lock.unlock();
        the_condition_variable.notify_one();
    }

    bool empty() const
    {
        boost::mutex::scoped_lock lock(the_mutex);
        return the_queue.empty();
    }

    bool try_pop(T& popped_value)
    {
        boost::mutex::scoped_lock lock(the_mutex);
        if(the_queue.empty())
        {
            return false;
        }

        popped_value = the_queue.front();
        the_queue.pop();
        return true;
    }

    void wait_and_pop(T& popped_value)
    {
        boost::mutex::scoped_lock lock(the_mutex);
        while(the_queue.empty())
        {
            the_condition_variable.wait(lock);
        }
        
        popped_value = the_queue.front();
        the_queue.pop();
    }
};

struct FIRFilterWorkerData {
    ThreadsafeQueue<Buffer*> input_queue;
    ThreadsafeQueue<Buffer*> output_queue;
    float* taps;
    int n_taps;
};

class FIRFilterWorker {
    public:
        FIRFilterWorker () {
            running = false;
            calculationTime = 0;
        }

        ~FIRFilterWorker() {
            PDEBUG("~FIRFilterWorker: Total elapsed thread time filtering: %zu\n", calculationTime);
        }

        void start(struct FIRFilterWorkerData *firworkerdata) {
            running = true;
            fir_thread = boost::thread(&FIRFilterWorker::process, this, firworkerdata);
        }

        void stop() {
            running = false;
            fir_thread.interrupt();
            fir_thread.join();
        }

        void process(struct FIRFilterWorkerData *fwd);


    private:
        time_t calculationTime; 
        struct FIRFilterWorkerData *workerdata;
        bool running;
        boost::thread fir_thread;
};


class FIRFilter : public ModCodec
{
public:
    FIRFilter(char* taps_file);
    virtual ~FIRFilter();
    FIRFilter(const FIRFilter&);
    FIRFilter& operator=(const FIRFilter&);

    int process(Buffer* const dataIn, Buffer* dataOut);
    const char* name() { return "FIRFilter"; }

protected:
    int my_Ntaps;
    float* myFilter;

    FIRFilterWorker worker;
    int number_of_runs;
    struct FIRFilterWorkerData firwd;
};


#endif //FIRFILTER_H
