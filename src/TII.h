/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2015
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org

   TII generation according to ETSI EN 300 401 Clause 14.8
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

#ifndef TII_H
#define TII_H

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include "ModCodec.h"
#include "RemoteControl.h"

#include <boost/thread.hpp>
#include <sys/types.h>
#include <complex>
#include <vector>
#include <string>

struct tii_config_t
{
    tii_config_t() : enable(false), comb(0), pattern(0) {}

    bool enable;
    int comb;
    int pattern;
};

class TIIError : public std::runtime_error {
    public:
        TIIError(const char* msg) :
            std::runtime_error(msg) {}
        TIIError(const std::string& msg) :
            std::runtime_error(msg) {}
};

class TII : public ModCodec, public RemoteControllable
{
    public:
        TII(unsigned int dabmode, tii_config_t& tii_config);
        virtual ~TII();

        int process(Buffer* const dataIn, Buffer* dataOut);
        const char* name();

        /******* REMOTE CONTROL ********/
        virtual void set_parameter(const std::string& parameter,
                const std::string& value);

        virtual const std::string get_parameter(
                const std::string& parameter) const;


    protected:
        // Fill m_dataIn with the correct carriers for the pattern/comb
        // combination
        void prepare_pattern(void);

        // prerequisites: calling thread must hold m_dataIn mutex
        void enable_carrier(int k);

        // Configuration settings
        unsigned int m_dabmode;

        // Remote-controllable settings
        tii_config_t& m_conf;

        // Internal flag when to insert TII
        bool m_insert;

        size_t m_carriers;

        std::string m_name;

        // m_dataIn is read by modulator thread, and written
        // to by RC thread.
        mutable boost::mutex m_dataIn_mutex;
        std::vector<std::complex<float> > m_dataIn;

    private:
        TII(const TII&);
        TII& operator=(const TII&);
};

#endif // TII_H

