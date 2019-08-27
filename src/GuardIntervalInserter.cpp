/*
   Copyright (C) 2005, 2206, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2017
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
#include <complex>
#include <mutex>

typedef std::complex<float> complexf;

GuardIntervalInserter::GuardIntervalInserter(
        size_t nbSymbols,
        size_t spacing,
        size_t nullSize,
        size_t symSize,
        size_t& windowOverlap) :
    ModCodec(),
    RemoteControllable("guardinterval"),
    d_nbSymbols(nbSymbols),
    d_spacing(spacing),
    d_nullSize(nullSize),
    d_symSize(symSize),
    d_windowOverlap(windowOverlap)
{
    if (d_nullSize == 0) {
        throw std::logic_error("NULL symbol must be present");
    }

    RC_ADD_PARAMETER(windowlen, "Window length for OFDM windowng [0 to disable]");

    /* We use a raised-cosine window for the OFDM windowing.
     * Each symbol is extended on both sides by d_windowOverlap samples.
     *
     *
     * Sym n             |####################|
     * Sym n+1                                 |####################|
     *
     * We now extend the symbols by d_windowOverlap (one dash)
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
     * The window length is 2*d_windowOverlap.
     */

    update_window(windowOverlap);

    PDEBUG("GuardIntervalInserter::GuardIntervalInserter"
            "(%zu, %zu, %zu, %zu, %zu) @ %p\n",
            nbSymbols, spacing, nullSize, symSize, windowOverlap, this);
}

void GuardIntervalInserter::update_window(size_t new_window_overlap)
{
    std::lock_guard<std::mutex> lock(d_windowMutex);

    d_windowOverlap = new_window_overlap;

    // d_window only contains the rising window edge.
    d_window.resize(2*d_windowOverlap);
    for (size_t i = 0; i < 2*d_windowOverlap; i++) {
        d_window[i] = (float)(0.5 * (1.0 - cos(M_PI * i / (2*d_windowOverlap - 1))));
    }
}

int GuardIntervalInserter::process(Buffer* const dataIn, Buffer* dataOut)
{
    PDEBUG("GuardIntervalInserter::process(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);

    std::lock_guard<std::mutex> lock(d_windowMutex);

    // Every symbol overlaps over a length of d_windowOverlap with
    // the previous symbol, and with the next symbol. First symbol
    // receives no prefix window, because we don't remember the
    // last symbol from the previous TF (yet). Last symbol also
    // receives no suffix window, for the same reason.
    // Overall output buffer length must stay independent of the windowing.
    dataOut->setLength((d_nullSize + (d_nbSymbols * d_symSize)) * sizeof(complexf));

    const complexf* in = reinterpret_cast<const complexf*>(dataIn->getData());
    complexf* out = reinterpret_cast<complexf*>(dataOut->getData());
    size_t sizeIn = dataIn->getLength() / sizeof(complexf);

    const size_t num_symbols = d_nbSymbols + 1;
    if (sizeIn != num_symbols * d_spacing)
    {
        PDEBUG("Nb symbols: %zu\n", d_nbSymbols);
        PDEBUG("Spacing: %zu\n", d_spacing);
        PDEBUG("Null size: %zu\n", d_nullSize);
        PDEBUG("Sym size: %zu\n", d_symSize);
        PDEBUG("\n%zu != %zu\n", sizeIn, (d_nbSymbols + 1) * d_spacing);
        throw std::runtime_error(
                "GuardIntervalInserter::process input size not valid!");
    }

    // TODO remember the end of the last TF so that we can do some
    //      windowing too.

    if (d_windowOverlap) {
        {
            // Handle Null symbol separately because it is longer
            const size_t prefixlength = d_nullSize - d_spacing;

            // end = spacing
            memcpy(out, &in[d_spacing - prefixlength],
                    prefixlength * sizeof(complexf));

            memcpy(&out[prefixlength], in, (d_spacing - d_windowOverlap) * sizeof(complexf));

            // The remaining part of the symbol must have half of the window applied,
            // sloping down from 1 to 0.5
            for (size_t i = 0; i < d_windowOverlap; i++) {
                const size_t out_ix = prefixlength + d_spacing - d_windowOverlap + i;
                const size_t in_ix = d_spacing - d_windowOverlap + i;
                out[out_ix] = in[in_ix] * d_window[2*d_windowOverlap - (i+1)];
            }

            // Suffix is taken from the beginning of the symbol, and sees the other
            // half of the window applied.
            for (size_t i = 0; i < d_windowOverlap; i++) {
                const size_t out_ix = prefixlength + d_spacing + i;
                out[out_ix] = in[i] * d_window[d_windowOverlap - (i+1)];
            }

            in += d_spacing;
            out += d_nullSize;
            // out is now pointing to the proper end of symbol. There are
            // d_windowOverlap samples ahead that were already written.
        }

        // Data symbols
        for (size_t sym_ix = 0; sym_ix < d_nbSymbols; sym_ix++) {
            /* _ix variables are indices into in[], _ox variables are
             * indices for out[] */
            const ssize_t start_rise_ox = -d_windowOverlap;
            const size_t start_rise_ix = 2 * d_spacing - d_symSize - d_windowOverlap;
            /*
            const size_t start_real_symbol_ox = 0;
            const size_t start_real_symbol_ix = 2 * d_spacing - d_symSize;
            */
            const ssize_t end_rise_ox = d_windowOverlap;
            const size_t end_rise_ix = 2 * d_spacing - d_symSize + d_windowOverlap;
            const ssize_t end_cyclic_prefix_ox = d_symSize - d_spacing;
            /* end_cyclic_prefix_ix = end of symbol
            const size_t begin_fall_ox = d_symSize - d_windowOverlap;
            const size_t begin_fall_ix = d_spacing - d_windowOverlap;
            const size_t end_real_symbol_ox = d_symSize;
             end_real_symbol_ix = end of symbol
            const size_t end_fall_ox = d_symSize + d_windowOverlap;
            const size_t end_fall_ix = d_spacing + d_windowOverlap;
            */

            ssize_t ox = start_rise_ox;
            size_t ix = start_rise_ix;

            for (size_t i = 0; ix < end_rise_ix; i++) {
                out[ox] += in[ix] * d_window.at(i);
                ix++;
                ox++;
            }
            assert(ox == end_rise_ox);

            const size_t remaining_prefix_length = end_cyclic_prefix_ox - end_rise_ox;
            memcpy( &out[ox], &in[ix],
                    remaining_prefix_length * sizeof(complexf));
            ox += remaining_prefix_length;
            assert(ox == end_cyclic_prefix_ox);
            ix = 0;

            const bool last_symbol = (sym_ix + 1 >= d_nbSymbols);
            if (last_symbol) {
                // No windowing at all at end
                memcpy(&out[ox], &in[ix], d_spacing * sizeof(complexf));
                ox += d_spacing;
            }
            else {
                // Copy the middle part of the symbol, d_windowOverlap samples
                // short of the end.
                memcpy( &out[ox],
                        &in[ix],
                        (d_spacing - d_windowOverlap) * sizeof(complexf));
                ox += d_spacing - d_windowOverlap;
                ix += d_spacing - d_windowOverlap;
                assert(ox == (ssize_t)(d_symSize - d_windowOverlap));

                // Apply window from 1 to 0.5 for the end of the symbol
                for (size_t i = 0; ox < (ssize_t)d_symSize; i++) {
                    out[ox] = in[ix] * d_window[2*d_windowOverlap - (i+1)];
                    ox++;
                    ix++;
                }
                assert(ix == d_spacing);

                ix = 0;
                // Cyclic suffix, with window from 0.5 to 0
                for (size_t i = 0; ox < (ssize_t)(d_symSize + d_windowOverlap); i++) {
                    out[ox] = in[ix] * d_window[d_windowOverlap - (i+1)];
                    ox++;
                    ix++;
                }

                assert(ix == d_windowOverlap);
            }

            out += d_symSize;
            in += d_spacing;
            // out is now pointing to the proper end of symbol. There are
            // d_windowOverlap samples ahead that were already written.
        }
    }
    else {
        // Handle Null symbol separately because it is longer
        // end - (nullSize - spacing) = 2 * spacing - nullSize
        memcpy(out, &in[2 * d_spacing - d_nullSize],
                (d_nullSize - d_spacing) * sizeof(complexf));
        memcpy(&out[d_nullSize - d_spacing], in, d_spacing * sizeof(complexf));
        in += d_spacing;
        out += d_nullSize;

        // Data symbols
        for (size_t i = 0; i < d_nbSymbols; ++i) {
            // end - (symSize - spacing) = 2 * spacing - symSize
            memcpy(out, &in[2 * d_spacing - d_symSize],
                    (d_symSize - d_spacing) * sizeof(complexf));
            memcpy(&out[d_symSize - d_spacing], in, d_spacing * sizeof(complexf));
            in += d_spacing;
            out += d_symSize;
        }
    }

    return sizeIn;
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
        ss << d_windowOverlap;
    }
    else {
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();
}
