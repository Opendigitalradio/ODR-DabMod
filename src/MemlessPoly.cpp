/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2017
   Matthias P. Braendli, matthias.braendli@mpb.li
   Andreas Steger, andreas.steger@digris.ch

    http://opendigitalradio.org

   This block implements a memoryless polynom for digital predistortion.
   For better performance, multiplying is done in another thread, leading
   to a pipeline delay of two calls to MemlessPoly::process
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

#include "MemlessPoly.h"
#include "PcDebug.h"
#include "Utils.h"

#include <stdio.h>
#include <stdexcept>

#include <array>
#include <iostream>
#include <fstream>
#include <memory>

using namespace std;

#define NUM_COEFS 5

// By default the signal is unchanged
static const std::array<complexf, 8> default_coefficients({{
        1, 0.0, 0.0, 0.0,
        0.0, 0.0, 0.0, 0.0
        }});


MemlessPoly::MemlessPoly(const std::string& coefs_file) :
    PipelinedModCodec(),
    RemoteControllable("memlesspoly"),
    m_coefs_file(coefs_file)
{
    PDEBUG("MemlessPoly::MemlessPoly(%s) @ %p\n",
            coefs_file.c_str(), this);

    RC_ADD_PARAMETER(ncoefs, "(Read-only) number of coefficients.");
    RC_ADD_PARAMETER(coeffile, "Filename containing coefficients. When written to, the new file gets automatically loaded.");

    load_coefficients(m_coefs_file);

    start_pipeline_thread();
}

void MemlessPoly::load_coefficients(const std::string &coefFile)
{
    std::vector<complexf> coefs;
    if (coefFile == "default") {
        std::copy(default_coefficients.begin(), default_coefficients.end(),
                std::back_inserter(coefs));
    }
    else {
        std::ifstream coef_fstream(coefFile.c_str());
        if (!coef_fstream) {
            throw std::runtime_error("MemlessPoly: Could not open file with coefs!");
        }
        int n_coefs;
        coef_fstream >> n_coefs;

        if (n_coefs <= 0) {
            throw std::runtime_error("MemlessPoly: coefs file has invalid format.");
        }
        else if (n_coefs != NUM_COEFS) {
            throw std::runtime_error("MemlessPoly: invalid number of coefs: " +
                    std::to_string(coefs.size()));
        }

        etiLog.log(debug, "MemlessPoly: Reading %d coefs...", n_coefs);

        coefs.resize(n_coefs);

        for (int n = 0; n < n_coefs; n++) {
            float a, b;
            coef_fstream >> a;
            coef_fstream >> b;
            coefs[n] = complexf(a, b);

            if (coef_fstream.eof()) {
                etiLog.log(error, "MemlessPoly: file %s should contains %d coefs, "
                        "but EOF reached after %d coefs !",
                        coefFile.c_str(), n_coefs, n);
                throw std::runtime_error("MemlessPoly: coefs file invalid !");
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_coefs_mutex);

        m_coefs = coefs;
    }
}


int MemlessPoly::internal_process(Buffer* const dataIn, Buffer* dataOut)
{
    dataOut->setLength(dataIn->getLength());

    const complexf* in = reinterpret_cast<const complexf*>(dataIn->getData());
    complexf* out = reinterpret_cast<complexf*>(dataOut->getData());
    size_t sizeOut = dataOut->getLength() / sizeof(complexf);

    {
         std::lock_guard<std::mutex> lock(m_coefs_mutex);
         for (size_t i = 0; i < sizeOut; i += 1) {

             /* Implement
                a0 + a1*x + a2*x^2 + a3*x^3 + a4*x^4 + a5*x^5;
                with less multiplications:
                a0 + x*(a1 + x*(a2 + x*(a3 + x*(a3 + x*(a4 + a5*x)))));
              */

             /* Make sure to adapt NUM_COEFS when you change this */
             out[i] =
                 m_coefs[0] + in[i] *
                 ( m_coefs[1] + in[i] *
                   ( m_coefs[2] + in[i] *
                     ( m_coefs[3] + in[i] *
                       ( m_coefs[4] + in[i] *
                         ( m_coefs[5] + in[i] )))));
        }
    }

    return dataOut->getLength();
}

void MemlessPoly::set_parameter(const string& parameter, const string& value)
{
    stringstream ss(value);
    ss.exceptions ( stringstream::failbit | stringstream::badbit );

    if (parameter == "ncoefs") {
        throw ParameterError("Parameter 'ncoefs' is read-only");
    }
    else if (parameter == "coeffile") {
        try {
            load_coefficients(value);
            m_coefs_file = value;
        }
        catch (std::runtime_error &e) {
            throw ParameterError(e.what());
        }
    }
    else {
        stringstream ss;
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
}

const string MemlessPoly::get_parameter(const string& parameter) const
{
    stringstream ss;
    if (parameter == "ncoefs") {
        ss << m_coefs.size();
    }
    else if (parameter == "coefFile") {
        ss << m_coefs_file;
    }
    else {
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();
}

