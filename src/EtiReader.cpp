/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)
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

#include "EtiReader.h"
#include "PcDebug.h"

#include <stdexcept>
#include <sys/types.h>
#include <string.h>


enum ETI_READER_STATE {
    EtiReaderStateNbFrame,
    EtiReaderStateFrameSize,
    EtiReaderStateSync,
    EtiReaderStateFc,
    EtiReaderStateNst,
    EtiReaderStateEoh,
    EtiReaderStateFic,
    EtiReaderStateSubch,
    EtiReaderStateEof,
    EtiReaderStateTist,
    EtiReaderStatePad
};


EtiReader::EtiReader() : state(EtiReaderStateSync),
    myFicSource(NULL)
{
    PDEBUG("EtiReader::EtiReader()\n");

    myCurrentFrame = 0;
}


EtiReader::~EtiReader()
{
    PDEBUG("EtiReader::~EtiReader()\n");

//    if (myFicSource != NULL) {
//        delete myFicSource;
//    }
//    for (unsigned i = 0; i < mySources.size(); ++i) {
//        delete mySources[i];
//    }
}


FicSource* EtiReader::getFic()
{
    return myFicSource;
}


unsigned EtiReader::getMode()
{
    return eti_fc.MID;
}


unsigned EtiReader::getFct()
{
    return eti_fc.FCT;
}


const std::vector<SubchannelSource*>& EtiReader::getSubchannels()
{
    return mySources;
}


int EtiReader::process(Buffer* dataIn)
{
    PDEBUG("EtiReader::process(dataIn: %p)\n", dataIn);
    PDEBUG(" state: %u\n", state);
    const unsigned char* in = reinterpret_cast<const unsigned char*>(dataIn->getData());
    size_t input_size = dataIn->getLength();

    while (input_size > 0) {
        switch (state) {
        case EtiReaderStateNbFrame:
            if (input_size < 4) {
                return dataIn->getLength() - input_size;
            }
            nb_frames = *(uint32_t*)in;
            input_size -= 4;
            in += 4;
            state = EtiReaderStateFrameSize;
            PDEBUG("Nb frames: %i\n", nb_frames);
            break;
        case EtiReaderStateFrameSize:
            if (input_size < 2) {
                return dataIn->getLength() - input_size;
            }
            framesize = *(uint16_t*)in;
            input_size -= 2;
            in += 2;
            state = EtiReaderStateSync;
            PDEBUG("Framesize: %i\n", framesize);
            break;
        case EtiReaderStateSync:
            if (input_size < 4) {
                return dataIn->getLength() - input_size;
            }
            framesize = 6144;
            memcpy(&eti_sync, in, 4);
            input_size -= 4;
            framesize -= 4;
            in += 4;
            state = EtiReaderStateFc;
            PDEBUG("Sync.err: 0x%.2x\n", eti_sync.ERR);
            PDEBUG("Sync.fsync: 0x%.6x\n", eti_sync.FSYNC);
            break;
        case EtiReaderStateFc:
            if (input_size < 4) {
                return dataIn->getLength() - input_size;
            }
            memcpy(&eti_fc, in, 4);
            input_size -= 4;
            framesize -= 4;
            in += 4;
            state = EtiReaderStateNst;
            PDEBUG("Fc.fct: 0x%.2x\n", eti_fc.FCT);
            PDEBUG("Fc.ficf: %u\n", eti_fc.FICF);
            PDEBUG("Fc.nst: %u\n", eti_fc.NST);
            PDEBUG("Fc.fp: 0x%x\n", eti_fc.FP);
            PDEBUG("Fc.mid: %u\n", eti_fc.MID);
            PDEBUG("Fc.fl: %u\n", eti_fc.getFrameLength());
            if (!eti_fc.FICF) {
                throw std::runtime_error("FIC must be present to modulate!");
            }
            if (myFicSource == NULL) {
                myFicSource = new FicSource(eti_fc);
            }
            break;
        case EtiReaderStateNst:
            if (input_size < 4 * (size_t)eti_fc.NST) {
                return dataIn->getLength() - input_size;
            }
            if ((eti_stc.size() != eti_fc.NST) ||
                    (memcmp(&eti_stc[0], in, 4 * eti_fc.NST))) {
                PDEBUG("New stc!\n");
                eti_stc.resize(eti_fc.NST);
                for (unsigned i = 0; i < mySources.size(); ++i) {
                    delete mySources[i];
                }
                mySources.resize(eti_fc.NST);
                memcpy(&eti_stc[0], in, 4 * eti_fc.NST);
                for (unsigned i = 0; i < eti_fc.NST; ++i) {
                    mySources[i] = new SubchannelSource(eti_stc[i]);
                    PDEBUG("Sstc %u:\n", i);
                    PDEBUG(" Stc%i.scid: %i\n", i, eti_stc[i].SCID);
                    PDEBUG(" Stc%i.sad: %u\n", i, eti_stc[i].getStartAddress());
                    PDEBUG(" Stc%i.tpl: 0x%.2x\n", i, eti_stc[i].TPL);
                    PDEBUG(" Stc%i.stl: %u\n", i, eti_stc[i].getSTL());
                }
            }
            input_size -= 4 * eti_fc.NST;
            framesize -= 4 * eti_fc.NST;
            in += 4 * eti_fc.NST;
            state = EtiReaderStateEoh;
            break;
        case EtiReaderStateEoh:
            if (input_size < 4) {
                return dataIn->getLength() - input_size;
            }
            memcpy(&eti_eoh, in, 4);
            input_size -= 4;
            framesize -= 4;
            in += 4;
            state = EtiReaderStateFic;
            PDEBUG("Eoh.mnsc: 0x%.4x\n", eti_eoh.MNSC);
            PDEBUG("Eoh.crc: 0x%.4x\n", eti_eoh.CRC);
            break;
        case EtiReaderStateFic:
            if (eti_fc.MID == 3) {
                if (input_size < 128) {
                    return dataIn->getLength() - input_size;
                }
                PDEBUG("Writting 128 bytes of FIC channel data\n");
                Buffer fic = Buffer(128, in);
                myFicSource->process(&fic, NULL);
                input_size -= 128;
                framesize -= 128;
                in += 128;
            } else {
                if (input_size < 96) {
                    return dataIn->getLength() - input_size;
                }
                PDEBUG("Writting 96 bytes of FIC channel data\n");
                Buffer fic = Buffer(96, in);
                myFicSource->process(&fic, NULL);
                input_size -= 96;
                framesize -= 96;
                in += 96;
            }
            state = EtiReaderStateSubch;
            break;
        case EtiReaderStateSubch:
            for (size_t i = 0; i < eti_stc.size(); ++i) {
                unsigned size = mySources[i]->framesize();
                PDEBUG("Writting %i bytes of subchannel data\n", size);
                Buffer subch = Buffer(size, in);
                mySources[i]->process(&subch, NULL);
                input_size -= size;
                framesize -= size;
                in += size;
            }
            state = EtiReaderStateEof;
            break;
        case EtiReaderStateEof:
            if (input_size < 4) {
                return dataIn->getLength() - input_size;
            }
            memcpy(&eti_eof, in, 4);
            input_size -= 4;
            framesize -= 4;
            in += 4;
            state = EtiReaderStateTist;
            PDEBUG("Eof.crc: %#.4x\n", eti_eof.CRC);
            PDEBUG("Eof.rfu: %#.4x\n", eti_eof.RFU);
            break;
        case EtiReaderStateTist:
            if (input_size < 4) {
                return dataIn->getLength() - input_size;
            }
            memcpy(&eti_tist, in, 4);
            input_size -= 4;
            framesize -= 4;
            in += 4;
            state = EtiReaderStatePad;
            PDEBUG("Tist: 0x%.6x\n", eti_tist.TIST);
            break;
        case EtiReaderStatePad:
            if (framesize > 0) {
                --input_size;
                --framesize;
                ++in;
            } else {
                state = EtiReaderStateSync;
            }
            break;
        default:
    //        throw std::runtime_error("Invalid state!");
            PDEBUG("Invalid state (%i)!", state);
            input_size = 0;
        }
    }
    
    return dataIn->getLength() - input_size;
}
