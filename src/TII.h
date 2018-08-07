/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2018
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

#pragma once

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif

#include "ModPlugin.h"
#include "RemoteControl.h"

#include <cstddef>
#include <thread>
#include <complex>
#include <vector>
#include <string>

struct tii_config_t
{
    bool enable = false;
    int comb = 0;
    int pattern = 0;

    /* EN 300 401 clause 14.8 describes how to generate the TII signal, and
     * defines z_{m,0,k}:
     *
     * z_{m,0,k} = A_{c,p}(k) e^{j \psi_k} + A_{c,p}(k-1) e^{j \psi{k-1}}
     *
     * What was implemented in the old variant was
     *
     * z_{m,0,k} = A_{c,p}(k) e^{j \psi_k} + A_{c,p}(k-1) e^{j \psi{k}}
     *                                                              ^
     *                                                              |
     *                                                  Wrong phase on the second
     *                                                  carrier of the pair.
     *
     * Correctly implemented decoders ought to be able to decode such a TII,
     * but will not be able to correctly estimate the delay of different
     * transmitters.
     *
     * The option 'old_variant' allows the user to choose between this
     * old incorrect implementation and the new conforming one.
     */
    bool old_variant = false;
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

        int process(Buffer* dataIn, Buffer* dataOut);
        const char* name();

        /******* REMOTE CONTROL ********/
        virtual void set_parameter(const std::string& parameter,
                const std::string& value);

        virtual const std::string get_parameter(
                const std::string& parameter) const;

    protected:
        // Fill m_Acp with the correct carriers for the pattern/comb
        // combination
        void prepare_pattern(void);

        // prerequisites: calling thread must hold m_Acp mutex
        void enable_carrier(int k);

        // Configuration settings
        unsigned int m_dabmode;

        // Remote-controllable settings
        tii_config_t& m_conf;

        // Internal flag when to insert TII
        bool m_insert = true;

        size_t m_carriers = 0;

        std::string m_name;

        // m_Acp is read by modulator thread, and written
        // to by RC thread.
        mutable std::mutex m_enabled_carriers_mutex;

        // m_Acp corresponds to the A_{c,p}(k) function from the spec, except
        // that the leftmost carrier is at index 0, and not at -m_carriers/2 like
        // in the spec.
        std::vector<bool> m_Acp;
};

