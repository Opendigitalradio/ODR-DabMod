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

#pragma once

#ifdef HAVE_CONFIG_H
#   include <config.h>
#endif


#include "Eti.h"
#include "Log.h"
#include "FicSource.h"
#include "Socket.h"
#include "SubchannelSource.h"
#include "TimestampDecoder.h"
#include "lib/edi/ETIDecoder.hpp"

#include <vector>
#include <memory>
#include <stdint.h>
#include <sys/types.h>

/* The modulator uses this interface to get the necessary multiplex data,
 * either from an ETI or an EDI source.
 */
class EtiSource
{
public:
    /* Get the DAB Transmission Mode. Valid values: 1, 2, 3 or 4 */
    virtual unsigned getMode() = 0;

    /* Get the current Frame Phase */
    virtual unsigned getFp() = 0;

    /* Get the current Frame Count */
    virtual unsigned getFct() = 0;

    /* Returns true if we have valid time stamps in the ETI*/
    virtual bool sourceContainsTimestamp() = 0;

    /* Return the FIC source to be used for modulation */
    virtual std::shared_ptr<FicSource>& getFic(void);

    /* Return all subchannel sources containing MST data */
    virtual const std::vector<std::shared_ptr<SubchannelSource> > getSubchannels() const = 0;

protected:
    std::shared_ptr<FicSource> myFicSource;
};

enum class EtiReaderState {
    // For Framed input format
    NbFrame,

    // For Streamed input format
    FrameSize,

    // ETI Sync
    Sync,
    Fc,
    Nst,
    Eoh,
    Fic,
    Subch,
    Eof,
    Tist,
    Pad
};

/* The EtiReader extracts the necessary data for modulation from an ETI(NI) byte stream. */
class EtiReader : public EtiSource
{
public:
    EtiReader(double& tist_offset_s);

    virtual unsigned getMode();
    virtual unsigned getFp();
    virtual unsigned getFct();

    /* Read ETI data from dataIn. Returns the number of bytes
     * read from the buffer.
     */
    int loadEtiData(const Buffer& dataIn);

    virtual bool sourceContainsTimestamp();

    virtual const std::vector<std::shared_ptr<SubchannelSource> > getSubchannels() const;

private:
    /* Transform the ETI TIST to a PPS offset in units of 1/16384000 s */
    uint32_t getPPSOffset();

    EtiReaderState state = EtiReaderState::Sync;
    uint32_t nb_frames;
    uint16_t framesize;
    eti_SYNC eti_sync;
    eti_FC eti_fc;
    std::vector<eti_STC> eti_stc;
    eti_EOH eti_eoh;
    eti_EOF eti_eof;
    eti_TIST eti_tist;
    TimestampDecoder myTimestampDecoder;

    bool eti_fc_valid;

    std::vector<std::shared_ptr<SubchannelSource> > mySources;
};

/* The EdiReader extracts the necessary data using the EDI input library in
 * lib/edi
 */
class EdiReader : public EtiSource, public EdiDecoder::ETIDataCollector
{
public:
    EdiReader(double& tist_offset_s);

    virtual unsigned getMode();
    virtual unsigned getFp();
    virtual unsigned getFct();
    virtual bool sourceContainsTimestamp();
    virtual const std::vector<std::shared_ptr<SubchannelSource> > getSubchannels() const;

    virtual bool isFrameReady(void);
    virtual void clearFrame(void);

    // Tell the ETIWriter what EDI protocol we receive in *ptr.
    // This is not part of the ETI data, but is used as check
    virtual void update_protocol(
            const std::string& proto,
            uint16_t major,
            uint16_t minor);

    // Update the data for the frame characterisation
    virtual void update_fc_data(const EdiDecoder::eti_fc_data& fc_data);

    virtual void update_fic(std::vector<uint8_t>&& fic);

    virtual void update_err(uint8_t err);

    // In addition to TSTA in ETI, EDI also transports more time
    // stamp information.
    virtual void update_edi_time(
            uint32_t utco,
            uint32_t seconds);

    virtual void update_mnsc(uint16_t mnsc);

    virtual void update_rfu(uint16_t rfu);

    virtual void add_subchannel(EdiDecoder::eti_stc_data&& stc);

    // Gets called by the EDI library to tell us that all data for a frame was given to us
    virtual void assemble(void);
private:
    bool m_proto_valid = false;
    bool m_frameReady = false;

    uint8_t m_err;

    bool m_fc_valid = false;
    EdiDecoder::eti_fc_data m_fc;

    std::vector<uint8_t> m_fic;

    bool m_time_valid = false;
    uint32_t m_utco;
    uint32_t m_seconds;

    uint16_t m_mnsc = 0xffff;

    // 16 bits: RFU field in EOH
    uint16_t m_rfu = 0xffff;

    std::map<uint8_t, std::shared_ptr<SubchannelSource> > m_sources;

    TimestampDecoder m_timestamp_decoder;
};

/* The EDI input does not use the inputs defined in InputReader.h, as they were
 * designed for ETI. It uses the EdiTransport which in turn uses a threaded
 * receiver.
 */
class EdiTransport {
    public:
        EdiTransport(EdiDecoder::ETIDecoder& decoder);

        void Open(const std::string& uri);

        bool isEnabled(void) const { return m_enabled; }

        /* Receive a packet and give it to the decoder. Returns
         * true if a packet was received, false in case of socket
         * read was interrupted by a signal.
         */
        bool rxPacket(void);

    private:
        bool m_enabled;
        int m_port;
        std::string m_bindto;
        std::string m_mcastaddr;

        enum class Proto { UDP, TCP };
        Proto m_proto;
        Socket::UDPReceiver m_udp_rx;
        std::vector<uint8_t> m_tcpbuffer;
        Socket::TCPClient m_tcpclient;
        EdiDecoder::ETIDecoder& m_decoder;
};

// EdiInput wraps an EdiReader, an EdiDecoder::ETIDecoder and an EdiTransport
class EdiInput {
    public:
        EdiInput(double& tist_offset_s, float edi_max_delay_ms);
        EdiReader ediReader;
        EdiDecoder::ETIDecoder decoder;
        EdiTransport ediTransport;
};

