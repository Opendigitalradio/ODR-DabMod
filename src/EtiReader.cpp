/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2014, 2015
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

#include "EtiReader.h"
#include "Log.h"
#include "PcDebug.h"
#include "TimestampDecoder.h"

#include <stdexcept>
#include <memory>
#include <sys/types.h>
#include <string.h>
#include <arpa/inet.h>

using namespace std;

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


EtiReader::EtiReader(
        double& tist_offset_s,
        unsigned tist_delay_stages,
        RemoteControllers* rcs) :
    state(EtiReaderStateSync),
    myTimestampDecoder(tist_offset_s, tist_delay_stages)
{
    PDEBUG("EtiReader::EtiReader()\n");

    myTimestampDecoder.enrol_at(*rcs);

    myCurrentFrame = 0;
    eti_fc_valid = false;
}

std::shared_ptr<FicSource>& EtiReader::getFic()
{
    return myFicSource;
}


unsigned EtiReader::getMode()
{
    if (not eti_fc_valid) {
        throw std::runtime_error("Trying to access Mode before it is ready!");
    }
    return eti_fc.MID;
}


unsigned EtiReader::getFp()
{
    if (not eti_fc_valid) {
        throw std::runtime_error("Trying to access FP before it is ready!");
    }
    return eti_fc.FP;
}


const std::vector<std::shared_ptr<SubchannelSource> >& EtiReader::getSubchannels()
{
    return mySources;
}


int EtiReader::process(const Buffer* dataIn)
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
            eti_fc_valid = true;
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
            if (not myFicSource) {
                myFicSource = make_shared<FicSource>(eti_fc);
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
                memcpy(&eti_stc[0], in, 4 * eti_fc.NST);

                mySources.clear();
                for (unsigned i = 0; i < eti_fc.NST; ++i) {
                    mySources.push_back(make_shared<SubchannelSource>(eti_stc[i]));
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
            // throw std::runtime_error("Invalid state!");
            PDEBUG("Invalid state (%i)!", state);
            input_size = 0;
        }
    }

    // Update timestamps
    myTimestampDecoder.updateTimestampEti(eti_fc.FP & 0x3,
            eti_eoh.MNSC, getPPSOffset(), eti_fc.FCT);

    return dataIn->getLength() - input_size;
}

bool EtiReader::sourceContainsTimestamp()
{
    return (ntohl(eti_tist.TIST) & 0xFFFFFF) != 0xFFFFFF;
    /* See ETS 300 799, Annex C.2.2 */
}

uint32_t EtiReader::getPPSOffset()
{
    if (!sourceContainsTimestamp()) {
        //fprintf(stderr, "****** SOURCE NO TS\n");
        return 0.0;
    }

    uint32_t timestamp = ntohl(eti_tist.TIST) & 0xFFFFFF;
    //fprintf(stderr, "****** TIST 0x%x\n", timestamp);

    return timestamp;
}

