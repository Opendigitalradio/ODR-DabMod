/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Written by
   2012, Matthias P. Braendli, matthias.braendli@mpb.li
 */
/*
   This file is part of ODR-DabMod.

   ODR-DabMod is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   ODR-DabMod is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with ODR-DabMod.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef FIRFILTER_H
#define FIRFILTER_H

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include <boost/thread.hpp>

#include "RemoteControl.h"
#include "ModCodec.h"
#include "PcDebug.h"
#include "ThreadsafeQueue.h"

#include <sys/types.h>
#include <complex>
#include <vector>
#include <time.h>
#include <cstdio>
#include <string>
#include <memory>

#define FIRFILTER_PIPELINE_DELAY 1

typedef std::complex<float> complexf;

struct FIRFilterWorkerData {
    /* Thread-safe queues to give data to and get data from
     * the worker
     */
    ThreadsafeQueue<std::shared_ptr<Buffer> > input_queue;
    ThreadsafeQueue<std::shared_ptr<Buffer> > output_queue;

    /* Remote-control can change the taps while the filter
     * runs. This lock makes sure nothing bad happens when
     * the taps are being modified
     */
    mutable boost::mutex taps_mutex;
    std::vector<float> taps;
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
        bool running;
        boost::thread fir_thread;
};


class FIRFilter : public ModCodec, public RemoteControllable
{
public:
    FIRFilter(std::string& taps_file);
    virtual ~FIRFilter();
    FIRFilter(const FIRFilter&);
    FIRFilter& operator=(const FIRFilter&);

    int process(Buffer* const dataIn, Buffer* dataOut);
    const char* name() { return "FIRFilter"; }

    /******* REMOTE CONTROL ********/
    virtual void set_parameter(const std::string& parameter,
            const std::string& value);

    virtual const std::string get_parameter(
            const std::string& parameter) const;


protected:
    void load_filter_taps(std::string tapsFile);

    std::string& myTapsFile;

    FIRFilterWorker worker;
    int number_of_runs;
    struct FIRFilterWorkerData firwd;
};

#endif //FIRFILTER_H

