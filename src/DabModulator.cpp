/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyright (C) 2019
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

#include "DabModulator.h"
#include "PcDebug.h"

#if !defined(BUILD_FOR_EASYDABV3)
# include "QpskSymbolMapper.h"
# include "FrequencyInterleaver.h"
# include "PhaseReference.h"
# include "DifferentialModulator.h"
# include "NullSymbol.h"
# include "CicEqualizer.h"
# include "OfdmGenerator.h"
# include "GainControl.h"
# include "GuardIntervalInserter.h"
# include "Resampler.h"
# include "FIRFilter.h"
# include "MemlessPoly.h"
# include "TII.h"
#endif

#include "FrameMultiplexer.h"
#include "PrbsGenerator.h"
#include "BlockPartitioner.h"
#include "SignalMultiplexer.h"
#include "ConvEncoder.h"
#include "PuncturingEncoder.h"
#include "TimeInterleaver.h"
#include "TimestampDecoder.h"
#include "RemoteControl.h"
#include "Log.h"

using namespace std;

DabModulator::DabModulator(EtiSource& etiSource,
                           mod_settings_t& settings) :
    ModInput(),
    RemoteControllable("modulator"),
    m_settings(settings),
    myEtiSource(etiSource),
    myFlowgraph()
{
    PDEBUG("DabModulator::DabModulator() @ %p\n", this);

    RC_ADD_PARAMETER(rate, "(Read-only) IQ output samplerate");

    if (m_settings.dabMode == 0) {
        setMode(2);
    }
    else {
        setMode(m_settings.dabMode);
    }
}


void DabModulator::setMode(unsigned mode)
{
    switch (mode) {
    case 1:
        myNbSymbols = 76;
        myNbCarriers = 1536;
        mySpacing = 2048;
        myNullSize = 2656;
        mySymSize = 2552;
        myFicSizeOut = 288;
        break;
    case 2:
        myNbSymbols = 76;
        myNbCarriers = 384;
        mySpacing = 512;
        myNullSize = 664;
        mySymSize = 638;
        myFicSizeOut = 288;
        break;
    case 3:
        myNbSymbols = 153;
        myNbCarriers = 192;
        mySpacing = 256;
        myNullSize = 345;
        mySymSize = 319;
        myFicSizeOut = 384;
        break;
    case 4:
        myNbSymbols = 76;
        myNbCarriers = 768;
        mySpacing = 1024;
        myNullSize = 1328;
        mySymSize = 1276;
        myFicSizeOut = 288;
        break;
    default:
        throw std::runtime_error("DabModulator::setMode invalid mode size");
    }
}


int DabModulator::process(Buffer* dataOut)
{
    using namespace std;

    PDEBUG("DabModulator::process(dataOut: %p)\n", dataOut);

    if (not myFlowgraph) {
        const unsigned mode = m_settings.dabMode;
        setMode(mode);

        myFlowgraph = make_shared<Flowgraph>(m_settings.showProcessTime);
        ////////////////////////////////////////////////////////////////
        // CIF data initialisation
        ////////////////////////////////////////////////////////////////
        auto cifPrbs = make_shared<PrbsGenerator>(864 * 8, 0x110);
        auto cifMux = make_shared<FrameMultiplexer>(myEtiSource);
        auto cifPart = make_shared<BlockPartitioner>(mode);

#if !defined(BUILD_FOR_EASYDABV3)
        auto cifMap = make_shared<QpskSymbolMapper>(myNbCarriers);
        auto cifRef = make_shared<PhaseReference>(mode);
        auto cifFreq = make_shared<FrequencyInterleaver>(mode);
        auto cifDiff = make_shared<DifferentialModulator>(myNbCarriers);

        auto cifNull = make_shared<NullSymbol>(myNbCarriers);
        auto cifSig = make_shared<SignalMultiplexer>(
                (1 + myNbSymbols) * myNbCarriers * sizeof(complexf));

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
                myNbCarriers,
                (float)mySpacing * (float)m_settings.outputRate / 2048000.0f,
                cic_ratio);
        }

        shared_ptr<TII> tii;
        shared_ptr<PhaseReference> tiiRef;
        try {
            tii = make_shared<TII>(
                    m_settings.dabMode,
                    m_settings.tiiConfig);
            rcs.enrol(tii.get());
            tiiRef = make_shared<PhaseReference>(mode);
        }
        catch (const TIIError& e) {
            etiLog.level(error) << "Could not initialise TII: " << e.what();
        }

        auto cifOfdm = make_shared<OfdmGenerator>(
                (1 + myNbSymbols),
                myNbCarriers,
                mySpacing,
                m_settings.enableCfr,
                m_settings.cfrClip,
                m_settings.cfrErrorClip);

        rcs.enrol(cifOfdm.get());

        auto cifGain = make_shared<GainControl>(
                mySpacing,
                m_settings.gainMode,
                m_settings.digitalgain,
                m_settings.normalise,
                m_settings.gainmodeVariance);

        rcs.enrol(cifGain.get());

        auto cifGuard = make_shared<GuardIntervalInserter>(
                myNbSymbols, mySpacing, myNullSize, mySymSize,
                m_settings.ofdmWindowOverlap);
        rcs.enrol(cifGuard.get());

        shared_ptr<FIRFilter> cifFilter;
        if (not m_settings.filterTapsFilename.empty()) {
            cifFilter = make_shared<FIRFilter>(m_settings.filterTapsFilename);
            rcs.enrol(cifFilter.get());
        }

        shared_ptr<MemlessPoly> cifPoly;
        if (not m_settings.polyCoefFilename.empty()) {
            cifPoly = make_shared<MemlessPoly>(m_settings.polyCoefFilename,
                                               m_settings.polyNumThreads);
            rcs.enrol(cifPoly.get());
        }

        shared_ptr<Resampler> cifRes;
        if (m_settings.outputRate != 2048000) {
            cifRes = make_shared<Resampler>(
                    2048000,
                    m_settings.outputRate,
                    mySpacing);
        }
#endif

        myOutput = make_shared<OutputMemory>(dataOut);

        myFlowgraph->connect(cifPrbs, cifMux);

        ////////////////////////////////////////////////////////////////
        // Processing FIC
        ////////////////////////////////////////////////////////////////
        shared_ptr<FicSource> fic(myEtiSource.getFic());
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

        myFlowgraph->connect(fic, ficPrbs);
        myFlowgraph->connect(ficPrbs, ficConv);
        myFlowgraph->connect(ficConv, ficPunc);
        myFlowgraph->connect(ficPunc, cifPart);

        ////////////////////////////////////////////////////////////////
        // Configuring subchannels
        ////////////////////////////////////////////////////////////////
        for (const auto& subchannel : myEtiSource.getSubchannels()) {

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

            myFlowgraph->connect(subchannel, subchPrbs);
            myFlowgraph->connect(subchPrbs, subchConv);
            myFlowgraph->connect(subchConv, subchPunc);
            myFlowgraph->connect(subchPunc, subchInterleaver);
            myFlowgraph->connect(subchInterleaver, cifMux);
        }

        myFlowgraph->connect(cifMux, cifPart);
#if defined(BUILD_FOR_EASYDABV3)
        myFlowgraph->connect(cifPart, myOutput);
#else
        myFlowgraph->connect(cifPart, cifMap);
        myFlowgraph->connect(cifMap, cifFreq);
        myFlowgraph->connect(cifRef, cifDiff);
        myFlowgraph->connect(cifFreq, cifDiff);
        myFlowgraph->connect(cifNull, cifSig);
        myFlowgraph->connect(cifDiff, cifSig);
        if (tii) {
            myFlowgraph->connect(tiiRef, tii);
            myFlowgraph->connect(tii, cifSig);
        }

        shared_ptr<ModPlugin> prev_plugin = static_pointer_cast<ModPlugin>(cifSig);
        const std::list<shared_ptr<ModPlugin> > plugins({
                static_pointer_cast<ModPlugin>(cifCicEq),
                static_pointer_cast<ModPlugin>(cifOfdm),
                static_pointer_cast<ModPlugin>(cifGain),
                static_pointer_cast<ModPlugin>(cifGuard),
                static_pointer_cast<ModPlugin>(cifFilter), // optional block
                static_pointer_cast<ModPlugin>(cifRes),    // optional block
                static_pointer_cast<ModPlugin>(cifPoly),   // optional block
                static_pointer_cast<ModPlugin>(myOutput),
                });

        for (auto& p : plugins) {
            if (p) {
                myFlowgraph->connect(prev_plugin, p);
                prev_plugin = p;
            }
        }
#endif
    }

    ////////////////////////////////////////////////////////////////////
    // Processing data
    ////////////////////////////////////////////////////////////////////
    return myFlowgraph->run();
}

meta_vec_t DabModulator::process_metadata(const meta_vec_t& metadataIn)
{
    if (myOutput) {
        return myOutput->get_latest_metadata();
    }

    return {};
}


void DabModulator::set_parameter(const string& parameter, const string& value)
{
    if (parameter == "rate") {
        throw ParameterError("Parameter 'rate' is read-only");
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
    else {
        ss << "Parameter '" << parameter <<
            "' is not exported by controllable " << get_rc_name();
        throw ParameterError(ss.str());
    }
    return ss.str();
}
