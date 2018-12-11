/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010 Her Majesty the
   Queen in Right of Canada (Communications Research Center Canada)

   Copyright (C) 2018
   Evariste F5OEO, evaristec@gmail.com

    
DESCRIPTION:
   It is an output driver using the LimeSDR library.
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

#include "output/Lime.h"

#ifdef HAVE_LIMESDR

#include <chrono>
#include <limits>
#include <cstdio>
#include <iomanip>

#include "Log.h"
#include "Utils.h"

using namespace std;

namespace Output {

static constexpr size_t FRAMES_MAX_SIZE = 2;

Lime::Lime(SDRDeviceConfig& config) :
    SDRDevice(),
    m_conf(config)
{
    m_interpolate=m_conf.upsample;
    interpolatebuf=new complexf[200000*m_interpolate];  
    etiLog.level(info) <<
        "Lime:Creating the device with: " <<
        m_conf.device;
    int device_count = LMS_GetDeviceList(NULL);
	if (device_count < 0)
	{
		etiLog.level(error) << "Error making LimeSDR device: " << LMS_GetLastErrorMessage();
        throw std::runtime_error("Cannot find LimeSDR output device");
		
	}
    lms_info_str_t device_list[device_count];
	if (LMS_GetDeviceList(device_list) < 0)
	{
		etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw std::runtime_error("Cannot find LimeSDR output device");
		
	}
    unsigned int device_i=0; // If several cards, need to get device by configuration
    if (LMS_Open(&m_device, device_list[device_i], NULL) < 0)
	{
		etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw std::runtime_error("Cannot open LimeSDR output device");
	}
    if (LMS_Reset(m_device) < 0)
	{
		etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw std::runtime_error("Cannot reset LimeSDR output device");
	}

	if (LMS_Init(m_device) < 0)
	{
		etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw std::runtime_error("Cannot init LimeSDR output device");
	}
    
    if (LMS_EnableChannel(m_device, LMS_CH_TX, m_channel, true) < 0)
	{
		etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw std::runtime_error("Cannot channel LimeSDR output device");
	}

    if (LMS_SetSampleRate(m_device, m_conf.masterClockRate*m_interpolate, 0) < 0)
	{
		etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw std::runtime_error("Cannot channel LimeSDR output device");
	}
    float_type host_sample_rate=0.0;

    if (LMS_GetSampleRate(m_device, LMS_CH_TX, m_channel, &host_sample_rate, NULL) < 0)
	{
		etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw std::runtime_error("Cannot getsamplerate LimeSDR output device");
	}
    etiLog.level(info) << "LimeSDR master clock rate set to " <<
        std::fixed << std::setprecision(4) <<
        host_sample_rate/1000.0 << " kHz";

    if (LMS_SetLOFrequency(m_device, LMS_CH_TX,m_channel, m_conf.frequency) < 0) 
	{
		etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw std::runtime_error("Cannot Frequency LimeSDR output device");
	}

    float_type cur_frequency=0.0;

    if (LMS_GetLOFrequency(m_device, LMS_CH_TX,m_channel, &cur_frequency)<0)
    {
        etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw std::runtime_error("Cannot GetFrequency LimeSDR output device");   
    }
    etiLog.level(info) << "LimeSDR:Actual frequency: " <<
        std::fixed << std::setprecision(3) <<
        cur_frequency / 1000.0 << " kHz.";

	if (LMS_SetNormalizedGain(m_device, LMS_CH_TX, m_channel, m_conf.txgain/100.0) < 0) //value 0..100 -> Normalize
	{
			etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
            throw std::runtime_error("Cannot Gain LimeSDR output device");
	}
	
    
    if (LMS_SetAntenna(m_device, LMS_CH_TX, m_channel, LMS_PATH_TX2) < 0)
	{
	    etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw std::runtime_error("Cannot Antenna LimeSDR output device");
	}


    double  bandwidth_calibrating=2.5e6; // Minimal bandwidth
	if (LMS_Calibrate(m_device, LMS_CH_TX, m_channel, bandwidth_calibrating, 0) < 0)
	{
		etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
        throw std::runtime_error("Cannot Gain LimeSDR output device");
	}
    switch(m_interpolate)
    {
        case 1:
        {
            static double coeff[]= {-0.0014080960536375642, 0.0010270054917782545, 0.0002103941806126386, -0.0023147952742874622, 0.004256128799170256, -0.0038850826676934958, -0.0006057845894247293, 0.008352266624569893, -0.014639420434832573, 0.01275692880153656, 0.0012119393795728683, -0.02339744009077549, 0.04088031128048897, -0.03649924695491791, -0.001745241112075746, 0.07178881019353867, -0.15494878590106964, 0.22244733572006226, 0.7530255913734436, 0.22244733572006226, -0.15494878590106964, 0.07178881019353867, -0.001745241112075746, -0.03649924695491791, 0.04088031128048897, -0.02339744009077549, 0.0012119393795728683, 0.01275692880153656, -0.014639420434832573, 0.008352266624569893, -0.0006057845894247293, -0.0038850826676934958, 0.004256128799170256, -0.0023147952742874622, 0.0002103941806126386, 0.0010270054917782545, -0.0014080960536375642};
            LMS_SetGFIRCoeff(m_device, LMS_CH_TX, m_channel, LMS_GFIR3, coeff, 37) ;
            LMS_SetGFIR(m_device, LMS_CH_TX,m_channel, LMS_GFIR3, true);    
        }
        break;
        case 2:
        {
            static double coeff[]= {0.0007009872933849692, 0.0006160094635561109, -0.0003868100175168365, -0.0010892765130847692, -0.0003017585549969226, 0.0013388358056545258, 0.0014964848523959517, -0.000810395460575819, -0.0028437587898224592, -0.001026041223667562, 0.0033166243229061365, 0.004008698742836714, -0.0016114861937239766, -0.006794447544962168, -0.0029077117796987295, 0.0070640090852975845, 0.009203733876347542, -0.002605677582323551, -0.014204192906618118, -0.007088471669703722, 0.013578214682638645, 0.019509244710206985, -0.0035577849484980106, -0.028872046619653702, -0.016949573531746864, 0.02703845500946045, 0.045044951140880585, -0.00423968443647027, -0.07416801154613495, -0.05744718387722969, 0.09617383778095245, 0.30029231309890747, 0.39504382014274597, 0.30029231309890747, 0.09617383778095245, -0.05744718387722969, -0.07416801154613495, -0.00423968443647027, 0.045044951140880585, 0.02703845500946045, -0.016949573531746864, -0.028872046619653702, -0.0035577849484980106, 0.019509244710206985, 0.013578214682638645, -0.007088471669703722, -0.014204192906618118, -0.002605677582323551, 0.009203733876347542, 0.0070640090852975845, -0.0029077117796987295, -0.006794447544962168, -0.0016114861937239766, 0.004008698742836714, 0.0033166243229061365, -0.001026041223667562, -0.0028437587898224592, -0.000810395460575819, 0.0014964848523959517, 0.0013388358056545258, -0.0003017585549969226, -0.0010892765130847692, -0.0003868100175168365, 0.0006160094635561109, 0.0007009872933849692};
            LMS_SetGFIRCoeff(m_device, LMS_CH_TX, m_channel, LMS_GFIR3, coeff, 65) ;
            LMS_SetGFIR(m_device, LMS_CH_TX,m_channel, LMS_GFIR3, true);
        }
        break;
    }
}

Lime::~Lime()
{
    
    if (m_device != nullptr)
    {
     
        //if (m_tx_stream != nullptr)
        {
            LMS_StopStream(&m_tx_stream);
            LMS_DestroyStream(m_device, &m_tx_stream);
        }
        /*
        if (m_rx_stream != nullptr) {
            m_device->closeStream(m_rx_stream);
        }
        */
        LMS_EnableChannel(m_device, LMS_CH_TX, m_channel, false);
	    LMS_Close(m_device);
    }
    if(interpolatebuf!=nullptr)
        delete(interpolatebuf);
}

void Lime::tune(double lo_offset, double frequency)
{
   /* if (not m_device) throw runtime_error("Soapy device not set up");

    SoapySDR::Kwargs offset_arg;
    offset_arg["OFFSET"] = to_string(lo_offset);
    m_device->setFrequency(SOAPY_SDR_TX, 0, m_conf.frequency, offset_arg);
    */
}

double Lime::get_tx_freq(void) const
{
    if (not m_device) throw runtime_error("Lime device not set up");

    // TODO lo offset
    return 0;//m_device->getFrequency(SOAPY_SDR_TX, 0);
}

void Lime::set_txgain(double txgain)
{
    m_conf.txgain = txgain;
    if (not m_device) throw runtime_error("Lime device not set up");
    //m_device->setGain(SOAPY_SDR_TX, 0, m_conf.txgain);
}

double Lime::get_txgain(void) const
{
    if (not m_device) throw runtime_error("Lime device not set up");
    return 0;//m_device->getGain(SOAPY_SDR_TX, 0);
}

SDRDevice::RunStatistics Lime::get_run_statistics(void) const 
{
    RunStatistics rs;
    rs.num_underruns = underflows;
    rs.num_overruns = overflows;
    rs.num_late_packets = late_packets;
    rs.num_frames_modulated = num_frames_modulated;
    return rs;
}


double Lime::get_real_secs(void) const
{
    /*if (m_device) {
        long long time_ns = m_device->getHardwareTime();
        return time_ns / 1e9;
    }
    else {
        return 0.0;
    }*/
    return 0.0;
}

void Lime::set_rxgain(double rxgain)
{
    /*m_device->setGain(SOAPY_SDR_RX, 0, m_conf.rxgain);
    m_conf.rxgain = m_device->getGain(SOAPY_SDR_RX, 0);
    */
}

double Lime::get_rxgain(void) const
{
    return 0.0;//m_device->getGain(SOAPY_SDR_RX, 0);
}

size_t Lime::receive_frame(
        complexf *buf,
        size_t num_samples,
        struct frame_timestamp& ts,
        double timeout_secs)
{
    /*int flags = 0;
    long long timeNs = ts.get_ns();
    const size_t numElems = num_samples;

    void *buffs[1];
    buffs[0] = buf;

    int ret = m_device->activateStream(m_rx_stream, flags, timeNs, numElems);
    if (ret != 0) {
        throw std::runtime_error(string("Soapy activate RX stream failed: ") +
                SoapySDR::errToStr(ret));
    }
    m_rx_stream_active = true;

    int n_read = m_device->readStream(
            m_rx_stream, buffs, num_samples, flags, timeNs);

    ret = m_device->deactivateStream(m_rx_stream);
    if (ret != 0) {
        throw std::runtime_error(string("Soapy deactivate RX stream failed: ") +
                SoapySDR::errToStr(ret));
    }
    m_rx_stream_active = false;

    if (n_read < 0) {
        throw std::runtime_error(string("Soapy failed to read from RX stream : ") +
                SoapySDR::errToStr(ret));
    }

    ts.set_ns(timeNs);
    
    return n_read;
    */
   return 0;
}


bool Lime::is_clk_source_ok() const
{
    // TODO
    return true;
}

const char* Lime::device_name(void) const
{
    return "Lime";
}

double Lime::get_temperature(void) const
{
    // TODO Unimplemented
    // LimeSDR exports 'lms7_temp'
    return std::numeric_limits<double>::quiet_NaN();
}

void Lime::transmit_frame(const struct FrameData& frame)
{
    if (not m_device) throw runtime_error("Lime device not set up");

    if (not m_tx_stream_active)
    {
        unsigned int buffer_size = 200000*m_interpolate;

        m_tx_stream.channel = m_channel;
		m_tx_stream.fifoSize = buffer_size;
		m_tx_stream.throughputVsLatency = 0.5;
		m_tx_stream.isTx = LMS_CH_TX;
		m_tx_stream.dataFmt = lms_stream_t::LMS_FMT_F32;
	
        if ( LMS_SetupStream(m_device, &m_tx_stream) < 0 )
        {
            etiLog.level(error) << "Error making LimeSDR device: %s " << LMS_GetLastErrorMessage();
            throw std::runtime_error("Cannot Channel Activate LimeSDR output device");
	    }
        LMS_StartStream(&m_tx_stream);
        LMS_SetGFIR(m_device, LMS_CH_TX, m_channel, LMS_GFIR3, true);
        m_tx_stream_active = true;
    }

    // The frame buffer contains bytes representing FC32 samples
    const complexf *buf = reinterpret_cast<const complexf*>(frame.buf.data());
    const size_t numSamples = frame.buf.size() / sizeof(complexf);
    if ((frame.buf.size() % sizeof(complexf)) != 0)
    {
        throw std::runtime_error("Lime: invalid buffer size");
    }

    size_t num_sent=0;
    if(m_interpolate==1)
         num_sent= LMS_SendStream( &m_tx_stream, buf, numSamples, NULL, 1000 );

    if(m_interpolate>1) // We upsample (1 0 0 0), low pass filter is done by FIR
    {
        for(size_t i=0;i<numSamples;i++)
        {
            interpolatebuf[i*m_interpolate]=buf[i];
            for(size_t j=1;j<m_interpolate;j++)
                interpolatebuf[i*m_interpolate+j]=complexf(0,0);
        }
         num_sent= LMS_SendStream( &m_tx_stream, interpolatebuf, numSamples*m_interpolate, NULL, 1000 );     
    }

    if (num_sent <= 0)
    {
        etiLog.level(info) << num_sent;
        //throw std::runtime_error("Lime: Too Loonnnngg");
    }
    else
    {
        //etiLog.level(info) << "OK" << num_sent;
    }
    lms_stream_status_t LimeStatus;
    LMS_GetStreamStatus(&m_tx_stream,&LimeStatus);
    overflows=LimeStatus.overrun;
    underflows=LimeStatus.underrun;
    late_packets=LimeStatus.droppedPackets;
      
    num_frames_modulated++;
    
}

} // namespace Output

#endif // HAVE_LIMESDR


