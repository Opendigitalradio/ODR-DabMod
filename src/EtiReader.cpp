/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
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

#include "EtiReader.h"
#include "Log.h"
#include "PcDebug.h"
#include "TimestampDecoder.h"

#include <stdexcept>
#include <memory>
#include <sys/types.h>
#include <string.h>
#include <arpa/inet.h>
#include <regex>

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
        unsigned tist_delay_stages) :
    state(EtiReaderStateSync),
    myTimestampDecoder(tist_offset_s, tist_delay_stages),
    eti_fc_valid(false)
{
    rcs.enrol(&myTimestampDecoder);
}

std::shared_ptr<FicSource>& EtiSource::getFic()
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


const std::vector<std::shared_ptr<SubchannelSource> > EtiReader::getSubchannels() const
{
    return mySources;
}


int EtiReader::loadEtiData(const Buffer& dataIn)
{
    PDEBUG("EtiReader::loadEtiData(dataIn: %p)\n", &dataIn);
    PDEBUG(" state: %u\n", state);
    const unsigned char* in = reinterpret_cast<const unsigned char*>(dataIn.getData());
    size_t input_size = dataIn.getLength();

    while (input_size > 0) {
        switch (state) {
        case EtiReaderStateNbFrame:
            if (input_size < 4) {
                return dataIn.getLength() - input_size;
            }
            nb_frames = *(uint32_t*)in;
            input_size -= 4;
            in += 4;
            state = EtiReaderStateFrameSize;
            PDEBUG("Nb frames: %i\n", nb_frames);
            break;
        case EtiReaderStateFrameSize:
            if (input_size < 2) {
                return dataIn.getLength() - input_size;
            }
            framesize = *(uint16_t*)in;
            input_size -= 2;
            in += 2;
            state = EtiReaderStateSync;
            PDEBUG("Framesize: %i\n", framesize);
            break;
        case EtiReaderStateSync:
            if (input_size < 4) {
                return dataIn.getLength() - input_size;
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
                return dataIn.getLength() - input_size;
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
                unsigned ficf = eti_fc.FICF;
                unsigned mid = eti_fc.MID;
                myFicSource = make_shared<FicSource>(ficf, mid);
            }
            break;
        case EtiReaderStateNst:
            if (input_size < 4 * (size_t)eti_fc.NST) {
                return dataIn.getLength() - input_size;
            }
            if ((eti_stc.size() != eti_fc.NST) ||
                    (memcmp(&eti_stc[0], in, 4 * eti_fc.NST))) {
                PDEBUG("New stc!\n");
                eti_stc.resize(eti_fc.NST);
                memcpy(&eti_stc[0], in, 4 * eti_fc.NST);

                mySources.clear();
                for (unsigned i = 0; i < eti_fc.NST; ++i) {
                    const auto tpl = eti_stc[i].TPL;
                    mySources.push_back(
                            make_shared<SubchannelSource>(
                                eti_stc[i].getStartAddress(),
                                eti_stc[i].getSTL(),
                                tpl));
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
                return dataIn.getLength() - input_size;
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
                    return dataIn.getLength() - input_size;
                }
                PDEBUG("Writing 128 bytes of FIC channel data\n");
                Buffer fic(128, in);
                myFicSource->loadFicData(fic);
                input_size -= 128;
                framesize -= 128;
                in += 128;
            } else {
                if (input_size < 96) {
                    return dataIn.getLength() - input_size;
                }
                PDEBUG("Writing 96 bytes of FIC channel data\n");
                Buffer fic(96, in);
                myFicSource->loadFicData(fic);
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
                Buffer subch(size, in);
                mySources[i]->loadSubchannelData(subch);
                input_size -= size;
                framesize -= size;
                in += size;
            }
            state = EtiReaderStateEof;
            break;
        case EtiReaderStateEof:
            if (input_size < 4) {
                return dataIn.getLength() - input_size;
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
                return dataIn.getLength() - input_size;
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

    myFicSource->loadTimestamp(myTimestampDecoder.getTimestamp());

    return dataIn.getLength() - input_size;
}

bool EtiReader::sourceContainsTimestamp()
{
    return (ntohl(eti_tist.TIST) & 0xFFFFFF) != 0xFFFFFF;
    /* See ETS 300 799, Annex C.2.2 */
}

void EtiReader::calculateTimestamp(struct frame_timestamp& ts)
{
    myTimestampDecoder.calculateTimestamp(ts);
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

EdiReader::EdiReader(
        double& tist_offset_s,
        unsigned tist_delay_stages) :
    m_timestamp_decoder(tist_offset_s, tist_delay_stages)
{
    rcs.enrol(&m_timestamp_decoder);
}

unsigned EdiReader::getMode()
{
    if (not m_fc_valid) {
        throw std::runtime_error("Trying to access Mode before it is ready!");
    }
    return m_fc.mid;
}


unsigned EdiReader::getFp()
{
    if (not m_fc_valid) {
        throw std::runtime_error("Trying to access FP before it is ready!");
    }
    return m_fc.fp;
}

const std::vector<std::shared_ptr<SubchannelSource> > EdiReader::getSubchannels() const
{
    std::vector<std::shared_ptr<SubchannelSource> > sources;

    sources.resize(m_sources.size());
    for (const auto s : m_sources) {
        if (s.first < sources.size()) {
            sources.at(s.first) = s.second;
        }
        else {
            throw std::runtime_error("Missing subchannel data in EDI source");
        }
    }

    return sources;
}

bool EdiReader::sourceContainsTimestamp()
{
    if (not (m_frameReady and m_fc_valid)) {
        throw std::runtime_error("Trying to get timestamp before it is ready");
    }

    return m_fc.tsta != 0xFFFFFF;
}

void EdiReader::calculateTimestamp(struct frame_timestamp& ts)
{
    m_timestamp_decoder.calculateTimestamp(ts);
}

bool EdiReader::isFrameReady()
{
    return m_frameReady;
}

void EdiReader::clearFrame()
{
    m_frameReady = false;
    m_proto_valid = false;
    m_fc_valid = false;
    m_fic.clear();
}

void EdiReader::update_protocol(
        const std::string& proto,
        uint16_t major,
        uint16_t minor)
{
    m_proto_valid = (proto == "DETI" and major == 0 and minor == 0);

    if (not m_proto_valid) {
        throw std::invalid_argument("Wrong EDI protocol");
    }
}

void EdiReader::update_err(uint8_t err)
{
    if (not m_proto_valid) {
        throw std::logic_error("Cannot update ERR before protocol");
    }
    m_err = err;
}

void EdiReader::update_fc_data(const EdiDecoder::eti_fc_data& fc_data)
{
    if (not m_proto_valid) {
        throw std::logic_error("Cannot update FC before protocol");
    }

    m_fc_valid = false;
    m_fc = fc_data;

    if (not m_fc.ficf) {
        throw std::invalid_argument("FIC must be present");
    }

    if (m_fc.mid > 4) {
        throw std::invalid_argument("Invalid MID");
    }

    if (m_fc.fp > 7) {
        throw std::invalid_argument("Invalid FP");
    }

    m_fc_valid = true;
}

void EdiReader::update_fic(const std::vector<uint8_t>& fic)
{
    if (not m_proto_valid) {
        throw std::logic_error("Cannot update FIC before protocol");
    }
    m_fic = fic;
}

void EdiReader::update_edi_time(
        uint32_t utco,
        uint32_t seconds)
{
    if (not m_proto_valid) {
        throw std::logic_error("Cannot update time before protocol");
    }

    m_utco = utco;
    m_seconds = seconds;

    // TODO check validity
    m_time_valid = true;
}

void EdiReader::update_mnsc(uint16_t mnsc)
{
    if (not m_proto_valid) {
        throw std::logic_error("Cannot update MNSC before protocol");
    }

    m_mnsc = mnsc;
}

void EdiReader::update_rfu(uint16_t rfu)
{
    if (not m_proto_valid) {
        throw std::logic_error("Cannot update RFU before protocol");
    }

    m_rfu = rfu;
}

void EdiReader::add_subchannel(const EdiDecoder::eti_stc_data& stc)
{
    if (not m_proto_valid) {
        throw std::logic_error("Cannot add subchannel before protocol");
    }

    if (m_sources.count(stc.stream_index) == 0) {
        m_sources[stc.stream_index] = make_shared<SubchannelSource>(stc.sad, stc.stl(), stc.tpl);
    }

    auto& source = m_sources[stc.stream_index];

    if (source->framesize() != stc.mst.size()) {
        throw std::invalid_argument(
                "EDI: MST data length inconsistent with FIC");
    }
    source->loadSubchannelData(stc.mst);

    if (m_sources.size() > 64) {
        throw std::invalid_argument("Too many subchannels");
    }
}

void EdiReader::assemble()
{
    if (not m_proto_valid) {
        throw std::logic_error("Cannot assemble EDI data before protocol");
    }

    if (not m_fc_valid) {
        throw std::logic_error("Cannot assemble EDI data without FC");
    }

    if (m_fic.empty()) {
        throw std::logic_error("Cannot assemble EDI data without FIC");
    }

    // ETS 300 799 Clause 5.3.2, but we don't support not having
    // a FIC
    if (    (m_fc.mid == 3 and m_fic.size() != 32 * 4) or
            (m_fc.mid != 3 and m_fic.size() != 24 * 4) ) {
        stringstream ss;
        ss << "Invalid FIC length " << m_fic.size() <<
            " for MID " << m_fc.mid;
        throw std::invalid_argument(ss.str());
    }

    if (not myFicSource) {
        myFicSource = make_shared<FicSource>(m_fc.ficf, m_fc.mid);
    }

    myFicSource->loadFicData(m_fic);

    // Accept zero subchannels, because of an edge-case that can happen
    // during reconfiguration. See ETS 300 799 Clause 5.3.3

    if (m_utco == 0 and m_seconds == 0) {
        // We don't support relative-only timestamps
        m_fc.tsta = 0xFFFFFF; // disable TSTA
    }

    /* According to Annex F
     *  EDI = UTC + UTCO
     * We need UTC = EDI - UTCO
     *
     * The seconds value is given in number of seconds since
     * 1.1.2000
     */
    const std::time_t posix_timestamp_1_jan_2000 = 946684800;
    auto utc_ts = posix_timestamp_1_jan_2000 + m_seconds - m_utco;

    m_timestamp_decoder.updateTimestampEdi(utc_ts, m_fc.tsta, m_fc.fct(), m_fc.fp);

    myFicSource->loadTimestamp(m_timestamp_decoder.getTimestamp());

    m_frameReady = true;
}

EdiUdpInput::EdiUdpInput(EdiDecoder::ETIDecoder& decoder) :
    m_enabled(false),
    m_port(0),
    m_decoder(decoder) { }

void EdiUdpInput::Open(const std::string& uri)
{
    etiLog.level(info) << "Opening EDI :" << uri;

    const std::regex re_udp("udp://:([0-9]+)");
    std::smatch m;
    if (std::regex_match(uri, m, re_udp)) {
        m_port = std::stoi(m[1].str());

        etiLog.level(info) << "EDI port :" << m_port;

        // The max_fragments_queued is only a protection against a runaway
        // memory usage.
        // Rough calculation:
        // 300 seconds, 24ms per frame, up to 20 fragments per frame
        const size_t max_fragments_queued = 20 * 300 * 1000 / 24;

        m_udp_rx.start(m_port, max_fragments_queued);
        m_enabled = true;
    }
}

bool EdiUdpInput::rxPacket()
{
    try {
        auto udp_data = m_udp_rx.get_packet_buffer();
        m_decoder.push_packet(udp_data);
        return true;
    }
    catch (std::runtime_error& e) {
        etiLog.level(warn) << "EDI input: " << e.what();
        return false;
    }
}

