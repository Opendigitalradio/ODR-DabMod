/*
   Copyright (C) 2005, 2206, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2024
   Matthias P. Braendli, matthias.braendli@mpb.li

    http://opendigitalradio.org
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

#include "GuardIntervalInserter.h"
#include "PcDebug.h"
#include <cstring>
#include <cassert>
#include <stdexcept>
#include <mutex>

GuardIntervalInserter::Params::Params(
        size_t nbSymbols,
        size_t spacing,
        size_t nullSize,
        size_t symSize,
        size_t& windowOverlap) :
    nbSymbols(nbSymbols),
    spacing(spacing),
    nullSize(nullSize),
    symSize(symSize),
    windowOverlap(windowOverlap) {}

GuardIntervalInserter::GuardIntervalInserter(
        size_t nbSymbols,
        size_t spacing,
        size_t nullSize,
        size_t symSize,
        size_t& windowOverlap,
        FFTEngine fftEngine) :
    ModCodec(),
    RemoteControllable("guardinterval"),
    m_fftEngine(fftEngine),
    m_params(nbSymbols, spacing, nullSize, symSize, windowOverlap)
{
    if (nullSize == 0) {
        throw std::logic_error("NULL symbol must be present");
    }


    RC_ADD_PARAMETER(windowlen, "Window length for OFDM windowng [0 to disable]");

    /* We use a raised-cosine window for the OFDM windowing.
     * Each symbol is extended on both sides by windowOverlap samples.
     *
     *
     * Sym n             |####################|
     * Sym n+1                                 |####################|
     *
     * We now extend the symbols by windowOverlap (one dash)
     *
     * Sym n extended   -|####################|-
     * Sym n+1 extended                       -|####################|-
     *
     * The windows are raised-cosine:
     *                    ____________________
     * Sym n window      /                    \
     *          ... ____/                      \___________ ...
     *
     * Sym n+1 window                           ____________________
     *                                         /                    \
     *                    ... ________________/                      \__ ...
     *
     * The window length is 2*windowOverlap.
     */

    update_window(windowOverlap);

    PDEBUG("GuardIntervalInserter::GuardIntervalInserter"
            "(%zu, %zu, %zu, %zu, %zu) @ %p\n",
            nbSymbols, spacing, nullSize, symSize, windowOverlap, this);
}

void GuardIntervalInserter::update_window(size_t new_window_overlap)
{
    std::lock_guard<std::mutex> lock(m_params.windowMutex);

    m_params.windowOverlap = new_window_overlap;

    // m_params.window only contains the rising window edge.
    m_params.windowFloat.resize(2*m_params.windowOverlap);
    m_params.windowFix.resize(2*m_params.windowOverlap);
    m_params.windowFixWide.resize(2*m_params.windowOverlap);
    for (size_t i = 0; i < 2*m_params.windowOverlap; i++) {
        const float value = (float)(0.5 * (1.0 - cos(M_PI * i / (2*m_params.windowOverlap - 1))));

        m_params.windowFloat[i] = value;
        m_params.windowFix[i] = complexfix::value_type((double)value);
        m_params.windowFixWide[i] = complexfix_wide::value_type((double)value);
    }
}

template<typename T>
int do_process(const GuardIntervalInserter::Params& p, Buffer* const dataIn, Buffer* dataOut)
{
    PDEBUG("GuardIntervalInserter do_process(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);

    // Every symbol overlaps over a length of windowOverlap with
    // the previous symbol, and with the next symbol. First symbol
    // receives no prefix window, because we don't remember the
    // last symbol from the previous TF (yet). Last symbol also
    // receives no suffix window, for the same reason.
    // Overall output buffer length must stay independent of the windowing.
    dataOut->setLength((p.nullSize + (p.nbSymbols * p.symSize)) * sizeof(T));

    const T* in = reinterpret_cast<const T*>(dataIn->getData());
    T* out = reinterpret_cast<T*>(dataOut->getData());
    size_t sizeIn = dataIn->getLength() / sizeof(T);

    const size_t num_symbols = p.nbSymbols + 1;
    if (sizeIn != num_symbols * p.spacing)
    {
        PDEBUG("Nb symbols: %zu\n", p.nbSymbols);
        PDEBUG("Spacing: %zu\n", p.spacing);
        PDEBUG("Null size: %zu\n", p.nullSize);
        PDEBUG("Sym size: %zu\n", p.symSize);
        PDEBUG("\n%zu != %zu\n", sizeIn, (p.nbSymbols + 1) * p.spacing);
        throw std::runtime_error(
                "GuardIntervalInserter::process input size not valid!");
    }

    // TODO remember the end of the last TF so that we can do some
    //      windowing too.

    std::lock_guard<std::mutex> lock(p.windowMutex);
    if (p.windowOverlap) {
        {
            // Handle Null symbol separately because it is longer
            const size_t prefixlength = p.nullSize - p.spacing;

            // end = spacing
            memcpy(out, &in[p.spacing - prefixlength],
                    prefixlength * sizeof(T));

            memcpy(&out[prefixlength], in, (p.spacing - p.windowOverlap) * sizeof(T));

            // The remaining part of the symbol must have half of the window applied,
            // sloping down from 1 to 0.5
            for (size_t i = 0; i < p.windowOverlap; i++) {
                const size_t out_ix = prefixlength + p.spacing - p.windowOverlap + i;
                const size_t in_ix = p.spacing - p.windowOverlap + i;
                if constexpr (std::is_same_v<complexf, T>) {
                    out[out_ix] = in[in_ix] * p.windowFloat[2*p.windowOverlap - (i+1)];
                }
                if constexpr (std::is_same_v<complexfix, T>) {
                    out[out_ix] = in[in_ix] * p.windowFix[2*p.windowOverlap - (i+1)];
                }
                if constexpr (std::is_same_v<complexfix_wide, T>) {
                    out[out_ix] = in[in_ix] * p.windowFixWide[2*p.windowOverlap - (i+1)];
                }
            }

            // Suffix is taken from the beginning of the symbol, and sees the other
            // half of the window applied.
            for (size_t i = 0; i < p.windowOverlap; i++) {
                const size_t out_ix = prefixlength + p.spacing + i;
                if constexpr (std::is_same_v<complexf, T>) {
                    out[out_ix] = in[i] * p.windowFloat[p.windowOverlap - (i+1)];
                }
                if constexpr (std::is_same_v<complexfix, T>) {
                    out[out_ix] = in[i] * p.windowFix[p.windowOverlap - (i+1)];
                }
                if constexpr (std::is_same_v<complexfix_wide, T>) {
                    out[out_ix] = in[i] * p.windowFixWide[p.windowOverlap - (i+1)];
                }
            }

            in += p.spacing;
            out += p.nullSize;
            // out is now pointing to the proper end of symbol. There are
            // windowOverlap samples ahead that were already written.
        }

        // Data symbols
        for (size_t sym_ix = 0; sym_ix < p.nbSymbols; sym_ix++) {
            /* _ix variables are indices into in[], _ox variables are
             * indices for out[] */
            const ssize_t start_rise_ox = -p.windowOverlap;
            const size_t start_rise_ix = 2 * p.spacing - p.symSize - p.windowOverlap;
            /*
               const size_t start_real_symbol_ox = 0;
               const size_t start_real_symbol_ix = 2 * p.spacing - p.symSize;
               */
            const ssize_t end_rise_ox = p.windowOverlap;
            const size_t end_rise_ix = 2 * p.spacing - p.symSize + p.windowOverlap;
            const ssize_t end_cyclic_prefix_ox = p.symSize - p.spacing;
            /* end_cyclic_prefix_ix = end of symbol
               const size_t begin_fall_ox = p.symSize - p.windowOverlap;
               const size_t begin_fall_ix = p.spacing - p.windowOverlap;
               const size_t end_real_symbol_ox = p.symSize;
               end_real_symbol_ix = end of symbol
               const size_t end_fall_ox = p.symSize + p.windowOverlap;
               const size_t end_fall_ix = p.spacing + p.windowOverlap;
               */

            ssize_t ox = start_rise_ox;
            size_t ix = start_rise_ix;

            for (size_t i = 0; ix < end_rise_ix; i++) {
                if constexpr (std::is_same_v<complexf, T>) {
                    out[ox] += in[ix] * p.windowFloat.at(i);
                }
                if constexpr (std::is_same_v<complexfix, T>) {
                    out[ox] += in[ix] * p.windowFix.at(i);
                }
                if constexpr (std::is_same_v<complexfix_wide, T>) {
                    out[ox] += in[ix] * p.windowFixWide.at(i);
                }
                ix++;
                ox++;
            }
            assert(ox == end_rise_ox);

            const size_t remaining_prefix_length = end_cyclic_prefix_ox - end_rise_ox;
            memcpy( &out[ox], &in[ix],
                    remaining_prefix_length * sizeof(T));
            ox += remaining_prefix_length;
            assert(ox == end_cyclic_prefix_ox);
            ix = 0;

            const bool last_symbol = (sym_ix + 1 >= p.nbSymbols);
            if (last_symbol) {
                // No windowing at all at end
                memcpy(&out[ox], &in[ix], p.spacing * sizeof(T));
                ox += p.spacing;
            }
            else {
                // Copy the middle part of the symbol, p.windowOverlap samples
                // short of the end.
                memcpy( &out[ox],
                        &in[ix],
                        (p.spacing - p.windowOverlap) * sizeof(T));
                ox += p.spacing - p.windowOverlap;
                ix += p.spacing - p.windowOverlap;
                assert(ox == (ssize_t)(p.symSize - p.windowOverlap));

                // Apply window from 1 to 0.5 for the end of the symbol
                for (size_t i = 0; ox < (ssize_t)p.symSize; i++) {
                    if constexpr (std::is_same_v<complexf, T>) {
                        out[ox] = in[ix] * p.windowFloat[2*p.windowOverlap - (i+1)];
                    }
                    if constexpr (std::is_same_v<complexfix, T>) {
                        out[ox] = in[ix] * p.windowFix[2*p.windowOverlap - (i+1)];
                    }
                    if constexpr (std::is_same_v<complexfix_wide, T>) {
                        out[ox] = in[ix] * p.windowFixWide[2*p.windowOverlap - (i+1)];
                    }
                    ox++;
                    ix++;
                }
                assert(ix == p.spacing);

                ix = 0;
                // Cyclic suffix, with window from 0.5 to 0
                for (size_t i = 0; ox < (ssize_t)(p.symSize + p.windowOverlap); i++) {
                    if constexpr (std::is_same_v<complexf, T>) {
                        out[ox] = in[ix] * p.windowFloat[p.windowOverlap - (i+1)];
                    }
                    if constexpr (std::is_same_v<complexfix, T>) {
                        out[ox] = in[ix] * p.windowFix[p.windowOverlap - (i+1)];
                    }
                    if constexpr (std::is_same_v<complexfix_wide, T>) {
                        out[ox] = in[ix] * p.windowFixWide[p.windowOverlap - (i+1)];
                    }
                    ox++;
                    ix++;
                }

                assert(ix == p.windowOverlap);
            }

            out += p.symSize;
            in += p.spacing;
            // out is now pointing to the proper end of symbol. There are
            // windowOverlap samples ahead that were already written.
        }
    }
    else {
        // Handle Null symbol separately because it is longer
        // end - (nullSize - spacing) = 2 * spacing - nullSize
        memcpy(out, &in[2 * p.spacing - p.nullSize],
                (p.nullSize - p.spacing) * sizeof(T));
        memcpy(&out[p.nullSize - p.spacing], in, p.spacing * sizeof(T));
        in += p.spacing;
        out += p.nullSize;

        // Data symbols
        for (size_t i = 0; i < p.nbSymbols; ++i) {
            // end - (symSize - spacing) = 2 * spacing - symSize
            memcpy(out, &in[2 * p.spacing - p.symSize],
                    (p.symSize - p.spacing) * sizeof(T));
            memcpy(&out[p.symSize - p.spacing], in, p.spacing * sizeof(T));
            in += p.spacing;
            out += p.symSize;
        }
    }

    const auto sizeOut = dataOut->getLength();
    return sizeOut;
}

int GuardIntervalInserter::process(Buffer* const dataIn, Buffer* dataOut)
{
    switch (m_fftEngine) {
        case FFTEngine::FFTW:
            return do_process<complexf>(m_params, dataIn, dataOut);
        case FFTEngine::KISS:
            return do_process<complexfix>(m_params, dataIn, dataOut);
        case FFTEngine::DEXTER:
            return do_process<complexfix_wide>(m_params, dataIn, dataOut);
    }
    throw std::logic_error("Unhandled fftEngine variant");
}

void GuardIntervalInserter::set_parameter(
        const std::string& parameter,
        const std::string& value)
{
    using namespace std;
    stringstream ss(value);
    ss.exceptions ( stringstream::failbit | stringstream::badbit );

    if (parameter == "windowlen") {
        size_t new_window_overlap = 0;
        ss >> new_window_overlap;
        update_window(new_window_overlap);
    }
    else {
        stringstream ss_err;
        ss_err << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss_err.str());
    }
}

const std::string GuardIntervalInserter::get_parameter(const std::string& parameter) const
{
    using namespace std;
    stringstream ss;
    if (parameter == "windowlen") {
        ss << m_params.windowOverlap;
    }
    else {
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();
}

const json::map_t GuardIntervalInserter::get_all_values() const
{
    json::map_t map;
    map["windowlen"].v = m_params.windowOverlap;
    return map;
}
