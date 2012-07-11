/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)
 */
/*
   This file is part of CRC-DADMOD.

   CRC-DADMOD is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   CRC-DADMOD is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with CRC-DADMOD.  If not, see <http://www.gnu.org/licenses/>.
 */

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
#include "PuncturingEncoder.h"
#include "TimeInterleaver.h"


DabModulator::DabModulator(unsigned outputRate, unsigned clockRate,
        unsigned dabMode, GainMode gainMode, float factor) :
    ModCodec(ModFormat(1), ModFormat(0)),
    myOutputRate(outputRate),
    myClockRate(clockRate),
    myDabMode(dabMode),
    myGainMode(gainMode),
    myFactor(factor),
    myFlowgraph(NULL)
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
        FrameMultiplexer* cifMux = NULL;
        PrbsGenerator* cifPrbs = NULL;
        BlockPartitioner* cifPart = NULL;
        QpskSymbolMapper* cifMap = NULL;
        FrequencyInterleaver* cifFreq = NULL;
        PhaseReference* cifRef = NULL;
        DifferentialModulator* cifDiff = NULL;
        NullSymbol* cifNull = NULL;
        SignalMultiplexer* cifSig = NULL;
        CicEqualizer* cifCicEq = NULL;
        OfdmGenerator* cifOfdm = NULL;
        GainControl* cifGain = NULL;
        GuardIntervalInserter* cifGuard = NULL;
        Resampler* cifRes = NULL;

        cifPrbs = new PrbsGenerator(864 * 8, 0x110);
        cifMux = new FrameMultiplexer(myFicSizeOut + 864 * 8,
                &myEtiReader.getSubchannels());
        cifPart = new BlockPartitioner(mode, myEtiReader.getFct());
        cifMap = new QpskSymbolMapper(myNbCarriers);
        cifRef = new PhaseReference(mode);
        cifFreq = new FrequencyInterleaver(mode);
        cifDiff = new DifferentialModulator(myNbCarriers);
        cifNull = new NullSymbol(myNbCarriers);
        cifSig = new SignalMultiplexer(
                (1 + myNbSymbols) * myNbCarriers * sizeof(complexf));

        if (myClockRate) {
            cifCicEq = new CicEqualizer(myNbCarriers,
                    (float)mySpacing * (float)myOutputRate / 2048000.0f,
                    myClockRate / myOutputRate);
        }

        cifOfdm = new OfdmGenerator((1 + myNbSymbols), myNbCarriers, mySpacing);
        cifGain = new GainControl(mySpacing, myGainMode, myFactor);
        cifGuard = new GuardIntervalInserter(myNbSymbols, mySpacing,
                myNullSize, mySymSize);
        myOutput = new OutputMemory();

        if (myOutputRate != 2048000) {
            cifRes = new Resampler(2048000, myOutputRate, mySpacing);
        } else {
            fprintf(stderr, "No resampler\n");
        }

        myFlowgraph->connect(cifPrbs, cifMux);

        ////////////////////////////////////////////////////////////////
        // Processing FIC
        ////////////////////////////////////////////////////////////////
        FicSource* fic = myEtiReader.getFic();
        PrbsGenerator* ficPrbs = NULL;
        ConvEncoder* ficConv = NULL;
        PuncturingEncoder* ficPunc = NULL;
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
        ficPrbs = new PrbsGenerator(myFicSizeIn, 0x110);

        // Configuring convolutionnal encoder
        ficConv = new ConvEncoder(myFicSizeIn);

        // Configuring puncturing encoder
        ficPunc = new PuncturingEncoder();
        std::vector<PuncturingRule*> rules = fic->get_rules();
        std::vector<PuncturingRule*>::const_iterator rule;
        for (rule = rules.begin(); rule != rules.end(); ++rule) {
            PDEBUG(" Adding rule:\n");
            PDEBUG("  Length: %zu\n", (*rule)->length());
            PDEBUG("  Pattern: 0x%x\n", (*rule)->pattern());
            ficPunc->append_rule(*(*rule));
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
        std::vector<SubchannelSource*> subchannels =
            myEtiReader.getSubchannels();
        std::vector<SubchannelSource*>::const_iterator subchannel;
        for (subchannel = subchannels.begin();
                subchannel != subchannels.end();
                ++subchannel) {
            PrbsGenerator* subchPrbs = NULL;
            ConvEncoder* subchConv = NULL;
            PuncturingEncoder* subchPunc = NULL;
            TimeInterleaver* subchInterleaver = NULL;

            ////////////////////////////////////////////////////////////
            // Data initialisation
            ////////////////////////////////////////////////////////////
            size_t subchSizeIn = (*subchannel)->framesize();
            size_t subchSizeOut = (*subchannel)->framesizeCu() * 8;

            ////////////////////////////////////////////////////////////
            // Modules configuration
            ////////////////////////////////////////////////////////////

            // Configuring subchannel
            PDEBUG("Subchannel:\n");
            PDEBUG(" Start address: %zu\n",
                    (*subchannel)->startAddress());
            PDEBUG(" Framesize: %zu\n",
                    (*subchannel)->framesize());
            PDEBUG(" Bitrate: %zu\n", (*subchannel)->bitrate());
            PDEBUG(" Framesize CU: %zu\n",
                    (*subchannel)->framesizeCu());
            PDEBUG(" Protection: %zu\n",
                    (*subchannel)->protection());
            PDEBUG("  Form: %zu\n",
                    (*subchannel)->protectionForm());
            PDEBUG("  Level: %zu\n",
                    (*subchannel)->protectionLevel());
            PDEBUG("  Option: %zu\n",
                    (*subchannel)->protectionOption());

            // Configuring prbs genrerator
            subchPrbs = new PrbsGenerator(subchSizeIn, 0x110);

            // Configuring convolutionnal encoder
            subchConv = new ConvEncoder(subchSizeIn);

            // Configuring puncturing encoder
            subchPunc = new PuncturingEncoder();
            std::vector<PuncturingRule*> rules = (*subchannel)->get_rules();
            std::vector<PuncturingRule*>::const_iterator rule;
            for (rule = rules.begin(); rule != rules.end(); ++rule) {
                PDEBUG(" Adding rule:\n");
                PDEBUG("  Length: %zu\n", (*rule)->length());
                PDEBUG("  Pattern: 0x%x\n", (*rule)->pattern());
                subchPunc->append_rule(*(*rule));
            }
            PDEBUG(" Adding tail\n");
            subchPunc->append_tail_rule(PuncturingRule(3, 0xcccccc));

            // Configuring time interleaver
            subchInterleaver = new TimeInterleaver(subchSizeOut);

            myFlowgraph->connect(*subchannel, subchPrbs);
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
        if (myClockRate) {
            myFlowgraph->connect(cifSig, cifCicEq);
            myFlowgraph->connect(cifCicEq, cifOfdm);
        } else {
            myFlowgraph->connect(cifSig, cifOfdm);
        }
        myFlowgraph->connect(cifOfdm, cifGain);
        myFlowgraph->connect(cifGain, cifGuard);
        if (cifRes != NULL) {
            myFlowgraph->connect(cifGuard, cifRes);
            myFlowgraph->connect(cifRes, myOutput);
        } else {
            myFlowgraph->connect(cifGuard, myOutput);
        }
    }

    ////////////////////////////////////////////////////////////////////
    // Proccessing data
    ////////////////////////////////////////////////////////////////////
    myOutput->setOutput(dataOut);
    return myFlowgraph->run();
}
