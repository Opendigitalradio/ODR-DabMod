/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyright (C) 2015
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

#include "FrameMultiplexer.h"
#include "PrbsGenerator.h"
#include "BlockPartitioner.h"
#include "QpskSymbolMapper.h"
#include "FrequencyInterleaver.h"
#include "PhaseReference.h"
#include "DifferentialModulator.h"
#include "NullSymbol.h"
#include "SignalMultiplexer.h"
#include "CicEqualizer.h"
#include "OfdmGenerator.h"
#include "GainControl.h"
#include "GuardIntervalInserter.h"
#include "Resampler.h"
#include "ConvEncoder.h"
#include "FIRFilter.h"
#include "TII.h"
#include "PuncturingEncoder.h"
#include "TimeInterleaver.h"
#include "TimestampDecoder.h"
#include "RemoteControl.h"
#include "Log.h"

DabModulator::DabModulator(
        double& tist_offset_s, unsigned tist_delay_stages,
        RemoteControllers* rcs,
        tii_config_t& tiiConfig,
        unsigned outputRate, unsigned clockRate,
        unsigned dabMode, GainMode gainMode,
        float& digGain, float normalise,
        std::string& filterTapsFilename
        ) :
    ModCodec(ModFormat(1), ModFormat(0)),
    myOutputRate(outputRate),
    myClockRate(clockRate),
    myDabMode(dabMode),
    myGainMode(gainMode),
    myDigGain(digGain),
    myNormalise(normalise),
    myEtiReader(tist_offset_s, tist_delay_stages, rcs),
    myFlowgraph(NULL),
    myFilterTapsFilename(filterTapsFilename),
    myTiiConfig(tiiConfig),
    myRCs(rcs)
{
    PDEBUG("DabModulator::DabModulator(%u, %u, %u, %u) @ %p\n",
            outputRate, clockRate, dabMode, gainMode, this);

    if (myDabMode == 0) {
        setMode(2);
    } else {
        setMode(myDabMode);
    }
}


DabModulator::~DabModulator()
{
    PDEBUG("DabModulator::~DabModulator() @ %p\n", this);

    delete myFlowgraph;
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

    myOutputFormat.size((size_t)((myNullSize + (myNbSymbols * mySymSize))
                * sizeof(complexf) / 2048000.0 * myOutputRate));
}


int DabModulator::process(Buffer* const dataIn, Buffer* dataOut)
{
    using namespace std;

    PDEBUG("DabModulator::process(dataIn: %p, dataOut: %p)\n",
            dataIn, dataOut);

    myEtiReader.process(dataIn);
    if (myFlowgraph == NULL) {
        unsigned mode = myEtiReader.getMode();
        if (myDabMode != 0) {
            mode = myDabMode;
        } else if (mode == 0) {
            mode = 4;
        }
        setMode(mode);

        myFlowgraph = new Flowgraph();
        ////////////////////////////////////////////////////////////////
        // CIF data initialisation
        ////////////////////////////////////////////////////////////////
        auto cifPrbs = make_shared<PrbsGenerator>(864 * 8, 0x110);
        auto cifMux = make_shared<FrameMultiplexer>(
                myFicSizeOut + 864 * 8, &myEtiReader.getSubchannels());

        auto cifPart = make_shared<BlockPartitioner>(mode, myEtiReader.getFp());

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
        if (myClockRate) {
            cic_ratio = myClockRate / myOutputRate;
            cic_ratio /= 4; // FPGA DUC
            if (myClockRate == 400000000) { // USRP2
                if (cic_ratio & 1) { // odd
                    useCicEq = true;
                } // even, no filter
            }
            else {
                useCicEq = true;
            }
        }

        auto cifCicEq = make_shared<CicEqualizer>(
                myNbCarriers,
                (float)mySpacing * (float)myOutputRate / 2048000.0f, cic_ratio);

        shared_ptr<TII> tii;
        try {
            tii = make_shared<TII>(myDabMode, myTiiConfig);
            tii->enrol_at(*myRCs);
        }
        catch (TIIError& e) {
            etiLog.level(error) << "Could not initialise TII: " << e.what();
        }

        auto cifOfdm = make_shared<OfdmGenerator>(
                (1 + myNbSymbols), myNbCarriers, mySpacing);

        auto cifGain = make_shared<GainControl>(
                mySpacing, myGainMode, myDigGain, myNormalise);

        cifGain->enrol_at(*myRCs);

        auto cifGuard = make_shared<GuardIntervalInserter>(
                myNbSymbols, mySpacing, myNullSize, mySymSize);

        shared_ptr<FIRFilter> cifFilter;
        if (myFilterTapsFilename != "") {
            cifFilter = make_shared<FIRFilter>(myFilterTapsFilename);
            cifFilter->enrol_at(*myRCs);
        }
        auto myOutput = make_shared<OutputMemory>(dataOut);

        shared_ptr<Resampler> cifRes;
        if (myOutputRate != 2048000) {
            cifRes = make_shared<Resampler>(2048000, myOutputRate, mySpacing);
        } else {
            fprintf(stderr, "No resampler\n");
        }

        myFlowgraph->connect(cifPrbs, cifMux);

        ////////////////////////////////////////////////////////////////
        // Processing FIC
        ////////////////////////////////////////////////////////////////
        shared_ptr<FicSource> fic(myEtiReader.getFic());
        ////////////////////////////////////////////////////////////////
        // Data initialisation
        ////////////////////////////////////////////////////////////////
        myFicSizeIn = fic->getFramesize();

        ////////////////////////////////////////////////////////////////
        // Modules configuration
        ////////////////////////////////////////////////////////////////

        // Configuring FIC channel

        PDEBUG("FIC:\n");
        PDEBUG(" Framesize: %zu\n", fic->getFramesize());

        // Configuring prbs generator
        auto ficPrbs = make_shared<PrbsGenerator>(myFicSizeIn, 0x110);

        // Configuring convolutionnal encoder
        auto ficConv = make_shared<ConvEncoder>(myFicSizeIn);

        // Configuring puncturing encoder
        auto ficPunc = make_shared<PuncturingEncoder>();
        for (const auto *rule : fic->get_rules()) {
            PDEBUG(" Adding rule:\n");
            PDEBUG("  Length: %zu\n", rule->length());
            PDEBUG("  Pattern: 0x%x\n", rule->pattern());
            ficPunc->append_rule(*rule);
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
        std::vector<shared_ptr<SubchannelSource> > subchannels =
            myEtiReader.getSubchannels();
        for (const auto& subchannel : subchannels) {

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
            auto subchPunc = make_shared<PuncturingEncoder>();

            for (const auto& rule : subchannel->get_rules()) {
                PDEBUG(" Adding rule:\n");
                PDEBUG("  Length: %zu\n", rule->length());
                PDEBUG("  Pattern: 0x%x\n", rule->pattern());
                subchPunc->append_rule(*rule);
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
        myFlowgraph->connect(cifPart, cifMap);
        myFlowgraph->connect(cifMap, cifFreq);
        myFlowgraph->connect(cifRef, cifDiff);
        myFlowgraph->connect(cifFreq, cifDiff);
        myFlowgraph->connect(cifNull, cifSig);
        myFlowgraph->connect(cifDiff, cifSig);
        if (tii) {
            myFlowgraph->connect(tii, cifSig);
        }

        if (useCicEq) {
            myFlowgraph->connect(cifSig, cifCicEq);
            myFlowgraph->connect(cifCicEq, cifOfdm);
        } else {
            myFlowgraph->connect(cifSig, cifOfdm);
        }
        myFlowgraph->connect(cifOfdm, cifGain);
        myFlowgraph->connect(cifGain, cifGuard);

        if (cifFilter) {
            myFlowgraph->connect(cifGuard, cifFilter);
            if (cifRes) {
                myFlowgraph->connect(cifFilter, cifRes);
                myFlowgraph->connect(cifRes, myOutput);
            } else {
                myFlowgraph->connect(cifFilter, myOutput);
            }
        }
        else { //no filtering
            if (cifRes) {
                myFlowgraph->connect(cifGuard, cifRes);
                myFlowgraph->connect(cifRes, myOutput);
            } else {
                myFlowgraph->connect(cifGuard, myOutput);
            }

        }
    }

    ////////////////////////////////////////////////////////////////////
    // Proccessing data
    ////////////////////////////////////////////////////////////////////
    return myFlowgraph->run();
}

