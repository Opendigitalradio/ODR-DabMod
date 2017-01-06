/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011 Her Majesty
   the Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2016
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
#include "SubchannelSource.h"
#include "TimestampDecoder.h"
#include "lib/edi/ETIDecoder.hpp"
#include "lib/UdpSocket.h"

#include <vector>
#include <memory>
#include <stdint.h>
#include <sys/types.h>


class EtiSource
{
public:
    virtual unsigned getMode() = 0;
    virtual unsigned getFp() = 0;

    virtual bool sourceContainsTimestamp() = 0;
    virtual void calculateTimestamp(struct frame_timestamp& ts) = 0;

    virtual std::shared_ptr<FicSource>& getFic(void);
    virtual const std::vector<std::shared_ptr<SubchannelSource> > getSubchannels() const = 0;

protected:
    std::shared_ptr<FicSource> myFicSource;
};

class EtiReader : public EtiSource
{
public:
    EtiReader(
            double& tist_offset_s,
            unsigned tist_delay_stages);

    virtual unsigned getMode();
    virtual unsigned getFp();

    /* Read ETI data from dataIn. Returns the number of bytes
     * read from the buffer
     */
    int loadEtiData(const Buffer& dataIn);

    virtual void calculateTimestamp(struct frame_timestamp& ts)
    {
        myTimestampDecoder.calculateTimestamp(ts);
    }

    /* Returns true if we have valid time stamps in the ETI*/
    virtual bool sourceContainsTimestamp();

    virtual const std::vector<std::shared_ptr<SubchannelSource> > getSubchannels() const;

private:
    /* Transform the ETI TIST to a PPS offset in units of 1/16384000 s */
    uint32_t getPPSOffset();

    void sync();
    int state;
    uint32_t nb_frames;
    uint16_t framesize;
    eti_SYNC eti_sync;
    eti_FC eti_fc;
    std::vector<eti_STC> eti_stc;
    eti_EOH eti_eoh;
    eti_EOF eti_eof;
    eti_TIST eti_tist;
    TimestampDecoder myTimestampDecoder;

    size_t myCurrentFrame;
    bool eti_fc_valid;

    std::vector<std::shared_ptr<SubchannelSource> > mySources;
};

class EdiReader : public EtiSource, public EdiDecoder::DataCollector
{
public:
    virtual unsigned getMode();
    virtual unsigned getFp();
    virtual bool sourceContainsTimestamp() { return false; }
    virtual void calculateTimestamp(struct frame_timestamp& ts)
    { /* TODO */ }
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

    virtual void update_fic(const std::vector<uint8_t>& fic);

    virtual void update_err(uint8_t err);

    // In addition to TSTA in ETI, EDI also transports more time
    // stamp information.
    virtual void update_edi_time(
            uint32_t utco,
            uint32_t seconds);

    virtual void update_mnsc(uint16_t mnsc);

    virtual void update_rfu(uint16_t rfu);

    virtual void add_subchannel(const EdiDecoder::eti_stc_data& stc);

    // Tell the ETIWriter that the AFPacket is complete
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
};

class EdiUdpInput {
    public:
        EdiUdpInput(EdiDecoder::ETIDecoder& decoder);

        int Open(const std::string& uri);

        bool isEnabled(void) const { return m_enabled; }

        /* Receive a packet and give it to the decoder. Returns
         * true if a packet was received, false in case of socket
         * read was interrupted by a signal.
         */
        bool rxPacket(void);

    private:
        bool m_enabled;
        int m_port;

        UdpSocket m_sock;
        EdiDecoder::ETIDecoder& m_decoder;
};

