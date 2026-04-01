/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2026
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

#include "DifferentialModulator.h"
#include "PcDebug.h"

#include <cstdio>
#include <stdexcept>
#include <cstring>

#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif

DifferentialModulator::DifferentialModulator(size_t carriers, bool fixedPoint, bool withNeon) :
    ModMux(),
    RemoteControllable("diffmod"),
    m_carriers(carriers),
    m_fixedPoint(fixedPoint),
    m_withNeon(withNeon)
{
    PDEBUG("DifferentialModulator::DifferentialModulator(%zu, %d)"
#if defined(__ARM_NEON)
            " With NEON"
#endif
            "\n", carriers, fixedPoint);

#if !defined(__ARM_NEON)
    if (withNeon) {
        throw std::runtime_error("Not compiled with NEON acceleration");
    }
    RC_ADD_PARAMETER(neon, "Enable NEON acceleration");
#endif
}


DifferentialModulator::~DifferentialModulator()
{
    PDEBUG("DifferentialModulator::~DifferentialModulator()\n");
}

#if defined(__ARM_NEON)
struct cpx_q14 {
    int16_t re, im;
};

static inline void cmul4_q14_neon(
    const cpx_q14* a,
    const cpx_q14* b,
    cpx_q14* dst)
{
    // Load 4 complex numbers from each input:
    // val[0] = re lanes, val[1] = im lanes
    int16x4x2_t va = vld2_s16(reinterpret_cast<const int16_t*>(a));
    int16x4x2_t vb = vld2_s16(reinterpret_cast<const int16_t*>(b));

    // Widen to int32
    int32x4_t ar = vmovl_s16(va.val[0]);
    int32x4_t ai = vmovl_s16(va.val[1]);
    int32x4_t br = vmovl_s16(vb.val[0]);
    int32x4_t bi = vmovl_s16(vb.val[1]);

    // Complex multiply
    int32x4_t ac = vmulq_s32(ar, br);
    int32x4_t bd = vmulq_s32(ai, bi);
    int32x4_t ad = vmulq_s32(ar, bi);
    int32x4_t bc = vmulq_s32(ai, br);

    int32x4_t real = vsubq_s32(ac, bd);
    int32x4_t imag = vaddq_s32(ad, bc);

    /*
    // Q14 rounding: add 1<<13 before shifting
    const int32x4_t round = vdupq_n_s32(1 << 13);
    real = vaddq_s32(real, round);
    imag = vaddq_s32(imag, round);
    */

    real = vshrq_n_s32(real, 14);
    imag = vshrq_n_s32(imag, 14);

    // Saturating narrow back to int16
    int16x4_t real16 = vqmovn_s32(real);
    int16x4_t imag16 = vqmovn_s32(imag);

    int16x4x2_t vc;
    vc.val[0] = real16;
    vc.val[1] = imag16;

    vst2_s16(reinterpret_cast<int16_t*>(dst), vc);
}

void do_process_complexfix_neon(size_t carriers, const std::vector<Buffer*>& dataIn, Buffer* dataOut)
{
    size_t phaseSize = dataIn[0]->getLength() / sizeof(cpx_q14);
    size_t dataSize = dataIn[1]->getLength() / sizeof(cpx_q14);
    dataOut->setLength((phaseSize + dataSize) * sizeof(cpx_q14));

    const cpx_q14* phase = reinterpret_cast<const cpx_q14*>(dataIn[0]->getData());
    const cpx_q14* in = reinterpret_cast<const cpx_q14*>(dataIn[1]->getData());
    cpx_q14* out = reinterpret_cast<cpx_q14*>(dataOut->getData());

    if (phaseSize != carriers) {
        throw std::runtime_error(
                "DifferentialModulator::process input phase size not valid!");
    }
    if (dataSize % carriers != 0) {
        throw std::runtime_error(
                "DifferentialModulator::process input data size not valid!");
    }

    memcpy(dataOut->getData(), phase, phaseSize * sizeof(cpx_q14));

    for (size_t i = 0; i < dataSize; i += carriers) {
        for (size_t j = 0; j + 4 <= carriers; j += 4) {
            cmul4_q14_neon(out + j, in + j, out + carriers + j);
        }
        in += carriers;
        out += carriers;
    }
}
#endif // defined(__ARM_NEON)

template<typename T>
void do_process(size_t carriers, const std::vector<Buffer*>& dataIn, Buffer* dataOut)
{
    size_t phaseSize = dataIn[0]->getLength() / sizeof(T);
    size_t dataSize = dataIn[1]->getLength() / sizeof(T);
    dataOut->setLength((phaseSize + dataSize) * sizeof(T));

    const T* phase = reinterpret_cast<const T*>(dataIn[0]->getData());
    const T* in = reinterpret_cast<const T*>(dataIn[1]->getData());
    T* out = reinterpret_cast<T*>(dataOut->getData());

    if (phaseSize != carriers) {
        throw std::runtime_error(
                "DifferentialModulator::process input phase size not valid!");
    }
    if (dataSize % carriers != 0) {
        throw std::runtime_error(
                "DifferentialModulator::process input data size not valid!");
    }

    memcpy(dataOut->getData(), phase, phaseSize * sizeof(T));
    for (size_t i = 0; i < dataSize; i += carriers) {
        for (size_t j = 0; j < carriers; j += 4) {
            out[carriers + j] = out[j] * in[j];
            out[carriers + j + 1] = out[j + 1] * in[j + 1];
            out[carriers + j + 2] = out[j + 2] * in[j + 2];
            out[carriers + j + 3] = out[j + 3] * in[j + 3];
        }
        in += carriers;
        out += carriers;
    }
}

// dataIn[0] -> phase reference
// dataIn[1] -> data symbols
int DifferentialModulator::process(std::vector<Buffer*> dataIn, Buffer* dataOut)
{
#ifdef TRACE
    fprintf(stderr, "DifferentialModulator::process (dataIn:");
    for (size_t i = 0; i < dataIn.size(); ++i) {
        fprintf(stderr, " %p", dataIn[i]);
    }
    fprintf(stderr, ", sizeIn: ");
    for (size_t i = 0; i < dataIn.size(); ++i) {
        fprintf(stderr, " %zu", dataIn[i]->getLength());
    }
    fprintf(stderr, ", dataOut: %p, sizeOut: %zu)\n", dataOut, dataOut->getLength());
#endif

    if (dataIn.size() != 2) {
        throw std::runtime_error(
                "DifferentialModulator::process nb of input streams not 2!");
    }

    if (m_fixedPoint) {
#if defined(__ARM_NEON)
        if (m_withNeon) {
            do_process_complexfix_neon(m_carriers, dataIn, dataOut);
        }
        else {
            do_process<complexfix>(m_carriers, dataIn, dataOut);
        }
#else
        do_process<complexfix>(m_carriers, dataIn, dataOut);
#endif
    }
    else {
        do_process<complexf>(m_carriers, dataIn, dataOut);
    }

    return dataOut->getLength();
}

void DifferentialModulator::set_parameter(const std::string& parameter, const std::string& value)
{
    std::stringstream ss(value);
    ss.exceptions(std::stringstream::failbit | std::stringstream::badbit);
    if (parameter == "neon") {
        ss >> m_withNeon;
    }
    else {
        std::stringstream ss;
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
}

const std::string DifferentialModulator::get_parameter(const std::string& parameter) const
{
    std::stringstream ss;
    if (parameter == "neon") {
        ss << m_withNeon;
    }
    else {
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();
}

const json::map_t DifferentialModulator::get_all_values() const
{
    json::map_t map;
    map["neon"] = m_withNeon;
    return map;
}
