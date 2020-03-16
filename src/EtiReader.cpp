/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

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

EtiReader::EtiReader(
        double& tist_offset_s) :
    myTimestampDecoder(tist_offset_s),
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

unsigned EtiReader::getFct()
{
    if (not eti_fc_valid) {
        throw std::runtime_error("Trying to access FCT before it is ready!");
    }
    return eti_fc.FCT;
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
            case EtiReaderState::NbFrame:
                if (input_size < 4) {
                    return dataIn.getLength() - input_size;
                }
                nb_frames = *(uint32_t*)in;
                input_size -= 4;
                in += 4;
                state = EtiReaderState::FrameSize;
                PDEBUG("Nb frames: %i\n", nb_frames);
                break;
            case EtiReaderState::FrameSize:
                if (input_size < 2) {
                    return dataIn.getLength() - input_size;
                }
                framesize = *(uint16_t*)in;
                input_size -= 2;
                in += 2;
                state = EtiReaderState::Sync;
                PDEBUG("Framesize: %i\n", framesize);
                break;
            case EtiReaderState::Sync:
                if (input_size < 4) {
                    return dataIn.getLength() - input_size;
                }
                framesize = 6144;
                memcpy(&eti_sync, in, 4);
                input_size -= 4;
                framesize -= 4;
                in += 4;
                state = EtiReaderState::Fc;
                PDEBUG("Sync.err: 0x%.2x\n", eti_sync.ERR);
                PDEBUG("Sync.fsync: 0x%.6x\n", eti_sync.FSYNC);
                break;
            case EtiReaderState::Fc:
                if (input_size < 4) {
                    return dataIn.getLength() - input_size;
                }
                memcpy(&eti_fc, in, 4);
                eti_fc_valid = true;
                input_size -= 4;
                framesize -= 4;
                in += 4;
                state = EtiReaderState::Nst;
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
            case EtiReaderState::Nst:
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
                state = EtiReaderState::Eoh;
                break;
            case EtiReaderState::Eoh:
                if (input_size < 4) {
                    return dataIn.getLength() - input_size;
                }
                memcpy(&eti_eoh, in, 4);
                input_size -= 4;
                framesize -= 4;
                in += 4;
                state = EtiReaderState::Fic;
                PDEBUG("Eoh.mnsc: 0x%.4x\n", eti_eoh.MNSC);
                PDEBUG("Eoh.crc: 0x%.4x\n", eti_eoh.CRC);
                break;
            case EtiReaderState::Fic:
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
                state = EtiReaderState::Subch;
                break;
            case EtiReaderState::Subch:
                for (size_t i = 0; i < eti_stc.size(); ++i) {
                    unsigned size = mySources[i]->framesize();
                    PDEBUG("Writting %i bytes of subchannel data\n", size);
                    Buffer subch(size, in);
                    mySources[i]->loadSubchannelData(move(subch));
                    input_size -= size;
                    framesize -= size;
                    in += size;
                }
                state = EtiReaderState::Eof;
                break;
            case EtiReaderState::Eof:
                if (input_size < 4) {
                    return dataIn.getLength() - input_size;
                }
                memcpy(&eti_eof, in, 4);
                input_size -= 4;
                framesize -= 4;
                in += 4;
                state = EtiReaderState::Tist;
                PDEBUG("Eof.crc: %#.4x\n", eti_eof.CRC);
                PDEBUG("Eof.rfu: %#.4x\n", eti_eof.RFU);
                break;
            case EtiReaderState::Tist:
                if (input_size < 4) {
                    return dataIn.getLength() - input_size;
                }
                memcpy(&eti_tist, in, 4);
                input_size -= 4;
                framesize -= 4;
                in += 4;
                state = EtiReaderState::Pad;
                PDEBUG("Tist: 0x%.6x\n", eti_tist.TIST);
                break;
            case EtiReaderState::Pad:
                if (framesize > 0) {
                    --input_size;
                    --framesize;
                    ++in;
                } else {
                    state = EtiReaderState::Sync;
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
        double& tist_offset_s) :
    m_timestamp_decoder(tist_offset_s)
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

unsigned EdiReader::getFct()
{
    if (not m_fc_valid) {
        throw std::runtime_error("Trying to access FCT before it is ready!");
    }
    return m_fc.fct();
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

void EdiReader::update_fic(std::vector<uint8_t>&& fic)
{
    if (not m_proto_valid) {
        throw std::logic_error("Cannot update FIC before protocol");
    }
    m_fic = move(fic);
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

void EdiReader::add_subchannel(EdiDecoder::eti_stc_data&& stc)
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
    source->loadSubchannelData(move(stc.mst));

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

EdiTransport::EdiTransport(EdiDecoder::ETIDecoder& decoder) :
    m_enabled(false),
    m_port(0),
    m_bindto("0.0.0.0"),
    m_mcastaddr("0.0.0.0"),
    m_decoder(decoder) { }


void EdiTransport::Open(const std::string& uri)
{
    etiLog.level(info) << "Opening EDI :" << uri;

    const string proto = uri.substr(0, 3);
    if (proto == "udp") {
        size_t found_port = uri.find_first_of(":", 6);
        if (found_port == string::npos) {
            throw std::invalid_argument("EDI UDP input port must be provided");
        }

        m_port = std::stoi(uri.substr(found_port+1));
        std::string host_full = uri.substr(6, found_port-6);// skip udp://
        size_t found_mcast = host_full.find_first_of("@"); //have multicast address:
        if (found_mcast != string::npos) {
            if (found_mcast > 0) {
                m_bindto = host_full.substr(0, found_mcast);
            }
            m_mcastaddr = host_full.substr(found_mcast+1);
        }
        else if (found_port != 6) {
            m_bindto=host_full;
        }

        etiLog.level(info) << "EDI UDP input: host:" << m_bindto <<
            ", source:" << m_mcastaddr << ", port:" << m_port;

        // The max_fragments_queued is only a protection against a runaway
        // memory usage.
        // Rough calculation:
        // 300 seconds, 24ms per frame, up to 20 fragments per frame
        const size_t max_fragments_queued = 20 * 300 * 1000 / 24;

        m_udp_rx.start(m_port, m_bindto, m_mcastaddr, max_fragments_queued);
        m_proto = Proto::UDP;
        m_enabled = true;
    }
    else if (proto == "tcp") {
        size_t found_port = uri.find_first_of(":", 6);
        if (found_port == string::npos) {
            throw std::invalid_argument("EDI TCP input port must be provided");
        }

        m_port = std::stoi(uri.substr(found_port+1));
        const std::string hostname = uri.substr(6, found_port-6);// skip tcp://

        etiLog.level(info) << "EDI TCP connect to " << hostname << ":" << m_port;

        m_tcpclient.connect(hostname, m_port);
        m_proto = Proto::TCP;
        m_enabled = true;
    }
    else {
        throw std::invalid_argument("ETI protocol '" + proto + "' unknown");
    }
}

bool EdiTransport::rxPacket()
{
    switch (m_proto) {
        case Proto::UDP:
            {
                auto udp_data = m_udp_rx.get_packet_buffer();

                if (udp_data.empty()) {
                    return false;
                }

                m_decoder.push_packet(udp_data);
                return true;
            }
        case Proto::TCP:
            {
                // The buffer size must be smaller than the size of two AF Packets, because otherwise
                // the EDI decoder decodes two in a row and discards the first. This leads to ETI FCT
                // discontinuity.
                m_tcpbuffer.resize(512);
                const int timeout_ms = 1000;
                try {
                    ssize_t ret = m_tcpclient.recv(m_tcpbuffer.data(), m_tcpbuffer.size(), 0, timeout_ms);
                    if (ret == 0 or ret == -1) {
                        return false;
                    }
                    else if (ret > (ssize_t)m_tcpbuffer.size()) {
                        throw logic_error("EDI TCP: invalid recv() return value");
                    }
                    else {
                        m_tcpbuffer.resize(ret);
                        m_decoder.push_bytes(m_tcpbuffer);
                        return true;
                    }
                }
                catch (const Socket::TCPSocket::Timeout&) {
                    return false;
                }
            }
    }
    throw logic_error("Incomplete rxPacket implementation!");
}

EdiInput::EdiInput(double& tist_offset_s, float edi_max_delay_ms) :
    ediReader(tist_offset_s),
    decoder(ediReader, false),
    ediTransport(decoder)
{
    if (edi_max_delay_ms > 0.0f) {
        // setMaxDelay wants number of AF packets, which correspond to 24ms ETI frames
        decoder.setMaxDelay(lroundf(edi_max_delay_ms / 24.0f));
    }
}

