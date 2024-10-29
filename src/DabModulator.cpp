/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyright (C) 2023
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

#include <string>
#include <memory>
#include <vector>

#include "DabModulator.h"
#include "PcDebug.h"

#include "BlockPartitioner.h"
#include "CicEqualizer.h"
#include "ConvEncoder.h"
#include "DifferentialModulator.h"
#include "FIRFilter.h"
#include "FrameMultiplexer.h"
#include "FrequencyInterleaver.h"
#include "GainControl.h"
#include "GuardIntervalInserter.h"
#include "Log.h"
#include "MemlessPoly.h"
#include "NullSymbol.h"
#include "OfdmGenerator.h"
#include "PhaseReference.h"
#include "PrbsGenerator.h"
#include "PuncturingEncoder.h"
#include "QpskSymbolMapper.h"
#include "RemoteControl.h"
#include "Resampler.h"
#include "SignalMultiplexer.h"
#include "TII.h"
#include "TimeInterleaver.h"
#include "TimestampDecoder.h"

using namespace std;

DabModulator::DabModulator(EtiSource& etiSource,
                           mod_settings_t& settings,
                           const std::string& format) :
    ModInput(),
    RemoteControllable("modulator"),
    m_settings(settings),
    m_format(format),
    m_etiSource(etiSource),
    m_flowgraph()
{
    PDEBUG("DabModulator::DabModulator() @ %p\n", this);

    RC_ADD_PARAMETER(rate, "(Read-only) IQ output samplerate");
    RC_ADD_PARAMETER(num_clipped_samples, "(Read-only) Number of samples clipped in last frame during format conversion");

    if (m_settings.dabMode == 0) {
        setMode(1);
    }
    else {
        setMode(m_settings.dabMode);
    }
}


void DabModulator::setMode(unsigned mode)
{
    switch (mode) {
    case 1:
        m_nbSymbols = 76;
        m_nbCarriers = 1536;
        m_spacing = 2048;
        m_nullSize = 2656;
        m_symSize = 2552;
        m_ficSizeOut = 288;
        break;
    case 2:
        m_nbSymbols = 76;
        m_nbCarriers = 384;
        m_spacing = 512;
        m_nullSize = 664;
        m_symSize = 638;
        m_ficSizeOut = 288;
        break;
    case 3:
        m_nbSymbols = 153;
        m_nbCarriers = 192;
        m_spacing = 256;
        m_nullSize = 345;
        m_symSize = 319;
        m_ficSizeOut = 384;
        break;
    case 4:
        m_nbSymbols = 76;
        m_nbCarriers = 768;
        m_spacing = 1024;
        m_nullSize = 1328;
        m_symSize = 1276;
        m_ficSizeOut = 288;
        break;
    default:
        throw std::runtime_error("DabModulator::setMode invalid mode size");
    }
}


int DabModulator::process(Buffer* dataOut)
{
    using namespace std;

    PDEBUG("DabModulator::process(dataOut: %p)\n", dataOut);

    if (not m_flowgraph) {
        etiLog.level(debug) << "Setting up DabModulator...";
        const unsigned mode = m_settings.dabMode;
        setMode(mode);

        m_flowgraph = make_shared<Flowgraph>(m_settings.showProcessTime);
        ////////////////////////////////////////////////////////////////
        // CIF data initialisation
        ////////////////////////////////////////////////////////////////
        auto cifPrbs = make_shared<PrbsGenerator>(864 * 8, 0x110);
        auto cifMux = make_shared<FrameMultiplexer>(m_etiSource);
        auto cifPart = make_shared<BlockPartitioner>(mode);

        const bool fixedPoint = m_settings.fftEngine != FFTEngine::FFTW;
        auto cifMap = make_shared<QpskSymbolMapper>(m_nbCarriers, fixedPoint);
        auto cifRef = make_shared<PhaseReference>(mode, fixedPoint);
        auto cifFreq = make_shared<FrequencyInterleaver>(mode, fixedPoint);
        auto cifDiff = make_shared<DifferentialModulator>(m_nbCarriers, fixedPoint);

        auto cifNull = make_shared<NullSymbol>(m_nbCarriers,
                fixedPoint ? sizeof(complexfix) : sizeof(complexf));
        auto cifSig = make_shared<SignalMultiplexer>();

        // TODO this needs a review
        bool useCicEq = false;
        unsigned cic_ratio = 1;
        if (m_settings.clockRate) {
            cic_ratio = m_settings.clockRate / m_settings.outputRate;
            cic_ratio /= 4; // FPGA DUC
            if (m_settings.clockRate == 400000000) { // USRP2
                if (cic_ratio & 1) { // odd
                    useCicEq = true;
                } // even, no filter
            }
            else {
                useCicEq = true;
            }
        }

        shared_ptr<CicEqualizer> cifCicEq;
        if (useCicEq) {
            cifCicEq = make_shared<CicEqualizer>(
                m_nbCarriers,
                (float)m_spacing * (float)m_settings.outputRate / 2048000.0f,
                cic_ratio);
        }

        shared_ptr<TII> tii;
        shared_ptr<PhaseReference> tiiRef;
        try {
            if (fixedPoint) {
                etiLog.level(warn) << "TII does not yet support fixed point";
            }
            else {
                tii = make_shared<TII>(
                        m_settings.dabMode,
                        m_settings.tiiConfig);
                rcs.enrol(tii.get());
                tiiRef = make_shared<PhaseReference>(mode, fixedPoint);
            }
        }
        catch (const TIIError& e) {
            etiLog.level(error) << "Could not initialise TII: " << e.what();
        }

        shared_ptr<ModPlugin> cifOfdm;

        switch (m_settings.fftEngine) {
            case FFTEngine::FFTW:
                {
                    auto ofdm = make_shared<OfdmGeneratorCF32>(
                            (1 + m_nbSymbols),
                            m_nbCarriers,
                            m_spacing,
                            m_settings.enableCfr,
                            m_settings.cfrClip,
                            m_settings.cfrErrorClip);
                    rcs.enrol(ofdm.get());
                    cifOfdm = ofdm;
                }
                break;
            case FFTEngine::KISS:
                cifOfdm = make_shared<OfdmGeneratorFixed>(
                        (1 + m_nbSymbols),
                        m_nbCarriers,
                        m_spacing,
                        m_settings.enableCfr,
                        m_settings.cfrClip,
                        m_settings.cfrErrorClip);
                break;
            case FFTEngine::DEXTER:
                cifOfdm = make_shared<OfdmGeneratorDEXTER>(
                        (1 + m_nbSymbols),
                        m_nbCarriers,
                        m_spacing,
                        m_settings.enableCfr,
                        m_settings.cfrClip,
                        m_settings.cfrErrorClip);
                break;
        }

        shared_ptr<GainControl> cifGain;

        if (not fixedPoint) {
            cifGain = make_shared<GainControl>(
                    m_spacing,
                    m_settings.gainMode,
                    m_settings.digitalgain,
                    m_settings.normalise,
                    m_settings.gainmodeVariance);

            rcs.enrol(cifGain.get());
        }

        auto cifGuard = make_shared<GuardIntervalInserter>(
                m_nbSymbols, m_spacing, m_nullSize, m_symSize,
                m_settings.ofdmWindowOverlap, m_settings.fftEngine);
        rcs.enrol(cifGuard.get());

        shared_ptr<FIRFilter> cifFilter;
        if (not m_settings.filterTapsFilename.empty()) {
            if (fixedPoint) throw std::runtime_error("fixed point doesn't support fir filter");

            cifFilter = make_shared<FIRFilter>(m_settings.filterTapsFilename);
            rcs.enrol(cifFilter.get());
        }

        shared_ptr<MemlessPoly> cifPoly;
        if (not m_settings.polyCoefFilename.empty()) {
            if (fixedPoint) throw std::runtime_error("fixed point doesn't support predistortion");

            cifPoly = make_shared<MemlessPoly>(m_settings.polyCoefFilename,
                                               m_settings.polyNumThreads);
            rcs.enrol(cifPoly.get());
        }

        shared_ptr<Resampler> cifRes;
        if (m_settings.outputRate != 2048000) {
            if (fixedPoint) throw std::runtime_error("fixed point doesn't support resampler");

            cifRes = make_shared<Resampler>(
                    2048000,
                    m_settings.outputRate,
                    m_spacing);
        }

        if (m_settings.fftEngine == FFTEngine::FFTW and not m_format.empty()) {
            m_formatConverter = make_shared<FormatConverter>(false, m_format);
        }
        else if (m_settings.fftEngine == FFTEngine::DEXTER) {
            m_formatConverter = make_shared<FormatConverter>(true, m_format);
        }
        // KISS is already in s16

        m_output = make_shared<OutputMemory>(dataOut);

        m_flowgraph->connect(cifPrbs, cifMux);

        ////////////////////////////////////////////////////////////////
        // Processing FIC
        ////////////////////////////////////////////////////////////////
        shared_ptr<FicSource> fic(m_etiSource.getFic());
        ////////////////////////////////////////////////////////////////
        // Data initialisation
        ////////////////////////////////////////////////////////////////
        size_t ficSizeIn = fic->getFramesize();

        ////////////////////////////////////////////////////////////////
        // Modules configuration
        ////////////////////////////////////////////////////////////////

        // Configuring FIC channel

        PDEBUG("FIC:\n");
        PDEBUG(" Framesize: %zu\n", fic->getFramesize());

        // Configuring prbs generator
        auto ficPrbs = make_shared<PrbsGenerator>(ficSizeIn, 0x110);

        // Configuring convolutionnal encoder
        auto ficConv = make_shared<ConvEncoder>(ficSizeIn);

        // Configuring puncturing encoder
        auto ficPunc = make_shared<PuncturingEncoder>();
        for (const auto &rule : fic->get_rules()) {
            PDEBUG(" Adding rule:\n");
            PDEBUG("  Length: %zu\n", rule.length());
            PDEBUG("  Pattern: 0x%x\n", rule.pattern());
            ficPunc->append_rule(rule);
        }
        PDEBUG(" Adding tail\n");
        ficPunc->append_tail_rule(PuncturingRule(3, 0xcccccc));

        m_flowgraph->connect(fic, ficPrbs);
        m_flowgraph->connect(ficPrbs, ficConv);
        m_flowgraph->connect(ficConv, ficPunc);
        m_flowgraph->connect(ficPunc, cifPart);

        ////////////////////////////////////////////////////////////////
        // Configuring subchannels
        ////////////////////////////////////////////////////////////////
        for (const auto& subchannel : m_etiSource.getSubchannels()) {

            ////////////////////////////////////////////////////////////
            // Data initialisation
            ////////////////////////////////////////////////////////////
            size_t subchSizeIn = subchannel->framesize();
            size_t subchSizeOut = subchannel->framesizeCu() * 8;

            ////////////////////////////////////////////////////////////
            // Modules configuration
            ////////////////////////////////////////////////////////////

            // Configuring subchannel
            PDEBUG("Subchannel:\n");
            PDEBUG(" Start address: %zu\n",
                    subchannel->startAddress());
            PDEBUG(" Framesize: %zu\n",
                    subchannel->framesize());
            PDEBUG(" Bitrate: %zu\n", subchannel->bitrate());
            PDEBUG(" Framesize CU: %zu\n",
                    subchannel->framesizeCu());
            PDEBUG(" Protection: %zu\n",
                    subchannel->protection());
            PDEBUG("  Form: %zu\n",
                    subchannel->protectionForm());
            PDEBUG("  Level: %zu\n",
                    subchannel->protectionLevel());
            PDEBUG("  Option: %zu\n",
                    subchannel->protectionOption());

            // Configuring prbs genrerator
            auto subchPrbs = make_shared<PrbsGenerator>(subchSizeIn, 0x110);

            // Configuring convolutionnal encoder
            auto subchConv = make_shared<ConvEncoder>(subchSizeIn);

            // Configuring puncturing encoder
            auto subchPunc =
                make_shared<PuncturingEncoder>(subchannel->framesizeCu());

            for (const auto& rule : subchannel->get_rules()) {
                PDEBUG(" Adding rule:\n");
                PDEBUG("  Length: %zu\n", rule.length());
                PDEBUG("  Pattern: 0x%x\n", rule.pattern());
                subchPunc->append_rule(rule);
            }
            PDEBUG(" Adding tail\n");
            subchPunc->append_tail_rule(PuncturingRule(3, 0xcccccc));

            // Configuring time interleaver
            auto subchInterleaver = make_shared<TimeInterleaver>(subchSizeOut);

            m_flowgraph->connect(subchannel, subchPrbs);
            m_flowgraph->connect(subchPrbs, subchConv);
            m_flowgraph->connect(subchConv, subchPunc);
            m_flowgraph->connect(subchPunc, subchInterleaver);
            m_flowgraph->connect(subchInterleaver, cifMux);
        }

        m_flowgraph->connect(cifMux, cifPart);
        m_flowgraph->connect(cifPart, cifMap);
        m_flowgraph->connect(cifMap, cifFreq);
        m_flowgraph->connect(cifRef, cifDiff);
        m_flowgraph->connect(cifFreq, cifDiff);
        m_flowgraph->connect(cifNull, cifSig);
        m_flowgraph->connect(cifDiff, cifSig);
        if (tii) {
            m_flowgraph->connect(tiiRef, tii);
            m_flowgraph->connect(tii, cifSig);
        }

        shared_ptr<ModPlugin> prev_plugin = static_pointer_cast<ModPlugin>(cifSig);
        const std::vector<shared_ptr<ModPlugin> > plugins({
                static_pointer_cast<ModPlugin>(cifCicEq),
                static_pointer_cast<ModPlugin>(cifOfdm),
                static_pointer_cast<ModPlugin>(cifGain),
                static_pointer_cast<ModPlugin>(cifGuard),
                // optional blocks
                static_pointer_cast<ModPlugin>(cifFilter),
                static_pointer_cast<ModPlugin>(cifRes),
                static_pointer_cast<ModPlugin>(cifPoly),
                static_pointer_cast<ModPlugin>(m_formatConverter),
                // mandatory block
                static_pointer_cast<ModPlugin>(m_output),
                });

        for (auto& p : plugins) {
            if (p) {
                m_flowgraph->connect(prev_plugin, p);
                prev_plugin = p;
            }
        }
        etiLog.level(debug) << "DabModulator set up.";
    }

    ////////////////////////////////////////////////////////////////////
    // Processing data
    ////////////////////////////////////////////////////////////////////
    return m_flowgraph->run();
}

meta_vec_t DabModulator::process_metadata(const meta_vec_t& metadataIn)
{
    if (m_output) {
        return m_output->get_latest_metadata();
    }

    return {};
}


void DabModulator::set_parameter(const string& parameter, const string& value)
{
    if (parameter == "rate") {
        throw ParameterError("Parameter 'rate' is read-only");
    }
    else if (parameter == "num_clipped_samples") {
        throw ParameterError("Parameter 'num_clipped_samples' is read-only");
    }
    else {
        stringstream ss;
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
}

const string DabModulator::get_parameter(const string& parameter) const
{
    stringstream ss;
    if (parameter == "rate") {
        ss << m_settings.outputRate;
    }
    else if (parameter == "num_clipped_samples") {
        if (m_formatConverter) {
            ss << m_formatConverter->get_num_clipped_samples();
        }
        else {
            ss << "Parameter '" << parameter <<
                "' is not available when no format conversion is done.";
            throw ParameterError(ss.str());
        }
    }
    else {
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();
}

const json::map_t DabModulator::get_all_values() const
{
    json::map_t map;
    map["rate"].v = m_settings.outputRate;
    map["num_clipped_samples"].v = m_formatConverter ? m_formatConverter->get_num_clipped_samples() : 0;
    return map;
}
