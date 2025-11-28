/*
   Copyright (C) 2005-2010 Her Majesty the Queen in Right of Canada
   Copyright (C) 2025 Evariste F5OEO
   

   http://opendigitalradio.org

DESCRIPTION:
  It is an output driver using the TezukaFirmware for PlutoSDR.
*/

#include "output/PlutoTezuka.h"

#ifdef HAVE_PLUTOTEZUKA

#include <chrono>
#include <limits>
#include <cstdio>
#include <iomanip>

#include "Log.h"
#include "Utils.h"

#ifdef __ARM_NEON__
#include <arm_neon.h>
#endif

using namespace std;

namespace Output
{

// --- Constant Definitions ---
static constexpr size_t FRAME_LENGTH = 196608; // at native sample rate!
static constexpr size_t IIO_ERROR_BUFFER_SIZE = 196608; // Buffer size for iio_strerror

// --- Conversion Function (from provided original snippet) ---

#ifdef __ARM_NEON__
void conv_s16_from_float(unsigned n, const float *a, short *b)
{
    unsigned i;
    const float32x4_t plusone4 = vdupq_n_f32(1.0f);
    const float32x4_t minusone4 = vdupq_n_f32(-1.0f);
    const float32x4_t half4 = vdupq_n_f32(0.5f);
    const float32x4_t scale4 = vdupq_n_f32(4096.0f);
    const uint32x4_t mask4 = vdupq_n_u32(0x80000000);

    for (i = 0; i < n / 4; i++)
    {
        float32x4_t v4 = ((float32x4_t *)a)[i];
        v4 = vmulq_f32(vmaxq_f32(vminq_f32(v4, plusone4), minusone4), scale4);

        const float32x4_t w4 = vreinterpretq_f32_u32(vorrq_u32(vandq_u32(
                                                                   vreinterpretq_u32_f32(v4), mask4),
                                                               vreinterpretq_u32_f32(half4)));

        ((int16x4_t *)b)[i] = vmovn_s32(vcvtq_s32_f32(vaddq_f32(v4, w4)));
    }
}
#else
void conv_s16_from_float(unsigned n, const float *a, short *b)
{
    unsigned i;
    for (i = 0; i < n; i++)
    {
        float val = a[i];
        if (val > 1.0f) val = 1.0f;
        if (val < -1.0f) val = -1.0f;
        b[i] = (short)(val * 4096.0f);
    }
}
#endif


// --- Class Implementation ---

PlutoTezuka::PlutoTezuka(SDRDeviceConfig &config) : SDRDevice(), m_conf(config)
{
    etiLog.level(info) << "PlutoTezuka: Creating the device with URI: " << m_conf.device;

    // 1. Create Context
    if (m_conf.device == "default" || m_conf.device.empty()) {
        m_ctx = iio_create_default_context();
    } else {
        m_ctx = iio_create_context_from_uri(m_conf.device.c_str());
    }

    if (!m_ctx) {
        throw runtime_error("PlutoTezuka: Unable to create IIO context");
    }

    // 2. Find the PHY device (ad9361-phy) for attributes (freq, gain, bw)
    m_phy_dev = iio_context_find_device(m_ctx, "ad9361-phy");
    if (!m_phy_dev) {
        iio_context_destroy(m_ctx);
        throw runtime_error("PlutoTezuka: AD9361 PHY device not found");
    }

    iio_channel_attr_write_bool(iio_device_find_channel(m_phy_dev, "altvoltage1", true), "powerdown", true);

    // 3. Find the TX DMA device (for buffer streaming)
    m_tx_dev = iio_context_find_device(m_ctx, "cf-ad9361-dds-core-lpc");
    if (!m_tx_dev) {
        iio_context_destroy(m_ctx);
        throw runtime_error("PlutoTezuka: TX DMA device (cf-ad9361-dds-core-lpc) not found");
    }

    // 4. Set Sample Rate 
    long long rate = (long long)m_conf.sampleRate;
        
    long long Dummyrate = 4000000LL ; // A Valid rate for ad9361 Rate > 2.083M 

    struct iio_channel *tx_rate_ch=iio_device_find_channel(m_phy_dev, "voltage0", true);
    if (!tx_rate_ch) 
    {
        etiLog.level(warn) << "PlutoTezuka: Failed to get TX chrate" ;    
    }
    
    if (  iio_channel_attr_write_longlong(tx_rate_ch, "sampling_frequency", rate*4) < 0) {
        etiLog.level(warn) << "PlutoTezuka: Failed to set TX sampling rate " << rate*4;
    }

    fmc_load_lpf_filter(4,1.0,true);


    if (iio_channel_attr_write_longlong(tx_rate_ch, "sampling_frequency", rate) < 0) {
        etiLog.level(warn) << "PlutoTezuka: Failed to set TX sampling rate " << rate;
    }
    else
        etiLog.level(warn) << "PlutoTezuka: Success to set TX sampling rate " << rate;
    

    tune(m_conf.lo_offset, m_conf.frequency);
    set_txgain(m_conf.txgain);
    iio_channel_attr_write_bool(iio_device_find_channel(m_phy_dev, "altvoltage1", true), "powerdown", false);


    // 5. Setup Channels for Buffer
    m_tx0_i = iio_device_find_channel(m_tx_dev, "voltage0", true);
    m_tx0_q = iio_device_find_channel(m_tx_dev, "voltage1", true);

    if (!m_tx0_i || !m_tx0_q) {
        iio_context_destroy(m_ctx);
        throw runtime_error("PlutoTezuka: TX Channels (voltage0/voltage1) not found");
    }

    iio_channel_enable(m_tx0_i);
    iio_channel_enable(m_tx0_q);

    // 6. Create Buffer
    m_tx_buf = iio_device_create_buffer(m_tx_dev, FRAME_LENGTH, false); // False for non-cyclic
    if (!m_tx_buf) {
        iio_context_destroy(m_ctx);
        throw runtime_error("PlutoTezuka: Could not create IIO buffer");
    }

    etiLog.level(info) << "PlutoTezuka: Device initialized successfully.";
}

PlutoTezuka::~PlutoTezuka()
{
    if (m_tx_buf) {
        iio_buffer_destroy(m_tx_buf);
    }
    if (m_ctx) {
        iio_context_destroy(m_ctx);
    }
}

void PlutoTezuka::tune(double lo_offset, double frequency)
{
    if (not m_phy_dev)
        throw runtime_error("PlutoTezuka device not set up");

    struct iio_channel *tx_lo = iio_device_find_channel(m_phy_dev, "altvoltage1", true);
    
    if (tx_lo) {
        long long freq_hz = (long long)(frequency + lo_offset);
        if (iio_channel_attr_write_longlong(tx_lo, "frequency", freq_hz) < 0) {
            etiLog.level(error) << "PlutoTezuka: Failed to tune to " << freq_hz;
        } else {
            etiLog.level(info) << "PlutoTezuka: Tuned to " << freq_hz;
        }
    } else {
        etiLog.level(error) << "PlutoTezuka: Could not find TX LO channel";
    }
}

double PlutoTezuka::get_tx_freq(void) const
{
    if (not m_phy_dev) return 0.0;

    struct iio_channel *tx_lo = iio_device_find_channel(m_phy_dev, "altvoltage1", true);
    long long val = 0;
    if (tx_lo) {
        iio_channel_attr_read_longlong(tx_lo, "frequency", &val);
    }
    return (double)val;
}

void PlutoTezuka::set_txgain(double txgain)
{
    m_conf.txgain = txgain;
    if (not m_phy_dev)
        throw runtime_error("PlutoTezuka device not set up");

    struct iio_channel *tx_ch = iio_device_find_channel(m_phy_dev, "voltage0", true);
    
    if (tx_ch) {
        // Set to manual gain control mode (required for setting hardwaregain)
        iio_channel_attr_write(tx_ch, "gain_control_mode", "manual");
        
        long long gain_val = (long long)txgain;
        if (iio_channel_attr_write_longlong(tx_ch, "hardwaregain", gain_val) < 0) {
             etiLog.level(warn) << "PlutoTezuka: Failed to set gain to " << gain_val << " dB.";
        }
    }
}

double PlutoTezuka::get_txgain(void) const
{
    if (not m_phy_dev) return 0.0;
    
    struct iio_channel *tx_ch = iio_device_find_channel(m_phy_dev, "voltage0", true);
    long long val = 0;
    if (tx_ch) iio_channel_attr_read_longlong(tx_ch, "hardwaregain", &val);
    return (double)val;
}

void PlutoTezuka::set_bandwidth(double bandwidth)
{
    if (!m_phy_dev) return;
    
    struct iio_channel *tx_ch = iio_device_find_channel(m_phy_dev, "voltage0", true);
    if (tx_ch) {
        // Set RF bandwidth in Hz
        iio_channel_attr_write_longlong(tx_ch, "rf_bandwidth", (long long)bandwidth);
    }
}

double PlutoTezuka::get_bandwidth(void) const
{
    if (!m_phy_dev) return 0.0;
    
    struct iio_channel *tx_ch = iio_device_find_channel(m_phy_dev, "voltage0", true);
    long long val = 0;
    if (tx_ch) iio_channel_attr_read_longlong(tx_ch, "rf_bandwidth", &val);
    return (double)val;
}

// --- Run Statistics and Helpers ---

SDRDevice::run_statistics_t PlutoTezuka::get_run_statistics(void) const
{
    run_statistics_t rs;
    rs["underruns"].v = underflows;
    rs["overruns"].v = overflows;
    rs["dropped_packets"].v = dropped_packets;
    rs["frames"].v = num_frames_modulated;
    rs["fifo_fill"].v = m_last_fifo_fill_percent * 100;
    return rs;
}

double PlutoTezuka::get_real_secs(void) const
{
    return 0.0;
}

void PlutoTezuka::set_rxgain(double rxgain)
{
    // RX functionality not implemented
}

double PlutoTezuka::get_rxgain(void) const
{
    return 0.0;
}

size_t PlutoTezuka::receive_frame(
    complexf *buf,
    size_t num_samples,
    frame_timestamp &ts,
    double timeout_secs)
{
    // RX functionality not implemented
    return 0;
}

bool PlutoTezuka::is_clk_source_ok(void)
{
    return true;
}

const char *PlutoTezuka::device_name(void) const
{
    return "PlutoTezuka";
}

std::optional<double> PlutoTezuka::get_temperature(void) const
{
    if (not m_ctx) return std::nullopt;

    // Read temperature from XADC device (FPGA temperature)
    struct iio_device *xadc = iio_context_find_device(m_ctx, "xadc");
    if (xadc) {
         struct iio_channel *temp_ch = iio_device_find_channel(xadc, "temp0", false);
         if (temp_ch) {
             long long raw = 0, offset = 0, scale = 1;
             iio_channel_attr_read_longlong(temp_ch, "raw", &raw);
             iio_channel_attr_read_longlong(temp_ch, "offset", &offset);
             iio_channel_attr_read_longlong(temp_ch, "scale", &scale);
             
             return (double)((raw + offset) * scale) / 1000.0;
         }
    }
    return std::nullopt;
}

// --- Transmit Frame ---

void PlutoTezuka::transmit_frame(struct FrameData&& frame)
{
    if (not m_tx_dev || not m_tx_buf)
        throw runtime_error("PlutoTezuka device not set up or buffer missing");

    const complexf *buf = reinterpret_cast<const complexf *>(frame.buf.data());
    const size_t numSamples = frame.buf.size() / sizeof(complexf);

    // 1. Convert Float samples to Signed Short (S16)
    m_i16samples.resize(numSamples * 2);
    short *buffi16 = &m_i16samples[0];
    
    conv_s16_from_float(numSamples * 2, (const float *)buf, buffi16);
    
    // 2. Copy data into the IIO mapped buffer
    void *start = iio_buffer_start(m_tx_buf);
    size_t input_size_bytes = numSamples * 2 * sizeof(short);
    
    //size_t buffer_size_bytes = iio_buffer_get_size(m_tx_buf);
    size_t buffer_size_bytes = FRAME_LENGTH* sizeof(short) * 2;

    
    if (input_size_bytes > buffer_size_bytes) {
        etiLog.level(error) << "PlutoTezuka: Input data exceeds IIO buffer size (" << buffer_size_bytes << " bytes). Truncating.";
        input_size_bytes = buffer_size_bytes;
    }
    
    std::memcpy(start, buffi16, input_size_bytes);

    // 3. Push the buffer to hardware
    ssize_t num_sent = iio_buffer_push(m_tx_buf);

    if (num_sent < 0)
    {
        char err_msg[IIO_ERROR_BUFFER_SIZE];
        iio_strerror((int)num_sent, err_msg, IIO_ERROR_BUFFER_SIZE);
        etiLog.level(error) << "Error sending PlutoTezuka stream: " << err_msg;
        underflows++;
    }

    num_frames_modulated++;
}

void PlutoTezuka::build_lpf_filter(double *filter, double bw, int ntaps ) {
	double a;
	double B = bw;// filter bandwidth
	double t = -( ntaps - 1) / 2;// First tap
	// Create the filter
	for (int i = 0; i < ntaps; i++) {
		if (t == 0)
			a = 2.0 * B;
		else
		    a = 2.0 * B * sin(M_PI * t * B)/ (M_PI * t * B);
		filter[i] = (double)a;
		t = t + 1.0;
	}
}


void PlutoTezuka::fmc_load_lpf_filter(int ratio, float digitalgain,bool db6boost)
{


	#include "doc/fir-filter/lpf.h"

    double firrx[128];
    double firtx[128];
	double max=1;
    fprintf(stderr,"Build filter with %d ratio (up/down)sample\n",ratio);

	int NbTaps= FIR_TAPS_COUNT;
		
        
            
            build_lpf_filter(firtx,(0.5)/(double)(ratio),NbTaps);
            build_lpf_filter(firrx,(0.5)/(double)(ratio),NbTaps);
			
			
			float gain=log2(2*ratio)*digitalgain;
			//max=set_filter_gain(firtx,1,NbTaps);
			//float gain=1/max;
			firrx[NbTaps]=0.0;
    		firtx[NbTaps]=0.0;
			fprintf(stderr, "Max FIR =%f\n", max);
			fmc_load_tx_filter(fir_taps,fir_taps, NbTaps+1, ratio, true,gain,db6boost);

}



void PlutoTezuka::fmc_load_tx_filter(const double *firrx,const double *firtx, int taps, int ratio, bool enable,float gain,bool db6boost)
{
	
	if (!enable)
		return; //No FIR, NEITHER upsample*/

	int buffsize = 8192;
	char *buf = (char *)malloc(buffsize);
	int clen = 0;
	clen += snprintf(buf + clen, buffsize - clen, "RX 3 GAIN 0 DEC %d\n", ratio); //The filter provides a fixed +6dB gain to maximize dynamic range, so the programmable gain is typically set to -6dB to produce a net gain of 0dB
	if(db6boost)
	{
		fprintf(stderr,"FIR boost\n");
		clen += snprintf(buf + clen, buffsize - clen, "TX 3 GAIN 0 INT %d\n", ratio); //-6db seems better
	}	
	else
	{
		fprintf(stderr,"FIR normal (no boost)\n");
		clen += snprintf(buf + clen, buffsize - clen, "TX 3 GAIN -6 INT %d\n", ratio); //-6db seems better
	}	
	short coefrx=0;
	for (int i = 0; i < taps; i++)
	{
		if(i==taps/2) coefrx=0x7FFF; else coefrx=0;
		
		clen += snprintf(buf + clen, buffsize - clen, "%d,%d\n", (short)(0x7FFF * firtx[i]*gain),coefrx); //Fixme ! 1FFF instead of 0x3FFF seems better but should have to be inspect
	}	
	clen += snprintf(buf + clen, buffsize - clen, "\n");

	
	
	iio_device_attr_write_raw(m_phy_dev, "filter_fir_config", buf, clen);
	struct iio_channel *tx_rate_ch=iio_device_find_channel(m_phy_dev, "voltage0", true);
	iio_channel_attr_write_bool(tx_rate_ch,"filter_fir_en", true);
    //BUG FROM IIO : We need also to set RX_FIR else sampling rate could not be applied
    tx_rate_ch=iio_device_find_channel(m_phy_dev, "voltage0", false);
	iio_channel_attr_write_bool(tx_rate_ch,"filter_fir_en", true);
	free(buf);
}

} // namespace Output

#endif // HAVE_PLUTOTEZUKA