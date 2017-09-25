# DAB Frame constants
# Sources:
#   - etsi_EN_300_401_v010401p p145
#   - Measured with USRP B200

import numpy as np

class const:
    def __init__(self, sample_rate, target_median):
        self.sample_rate = sample_rate

        # Time domain
        self.T_F = sample_rate / 2048000 * 196608  # Transmission frame duration
        self.T_NULL = sample_rate / 2048000 * 2656  # Null symbol duration
        self.T_S = sample_rate / 2048000 * 2552  # Duration of OFDM symbols of indices l = 1, 2, 3,... L;
        self.T_U = sample_rate / 2048000 * 2048  # Inverse of carrier spacing
        self.T_C = sample_rate / 2048000 * 504  # Duration of cyclic prefix

        # Frequency Domain
        # example: np.delete(fft[3328:4865], 768)
        self.FFT_delete = 768
        self.FFT_delta = 1536  # Number of carrier frequencies
        if sample_rate == 2048000:
            self.FFT_start = 256
            self.FFT_end = 1793
        elif sample_rate == 8192000:
            self.FFT_start = 3328
            self.FFT_end = 4865
        else:
            raise RuntimeError("Sample Rate '{}' not supported".format(
                sample_rate
            ))

        # Calculate sample offset from phase rotation
        # time per sample = 1 / sample_rate
        # frequency per bin = 1kHz
        # phase difference per sample offset = delta_t * 2 * pi * delta_freq
        self.phase_offset_per_sample = 1. / sample_rate * 2 * np.pi * 1000

        # Constants for ExtractStatistic
        self.ES_start = 0.0
        self.ES_end = 1.0
        self.ES_n_bins = 64
        self.ES_n_per_bin = 128

        # Constants for TX_Agc
        self.TAGC_max_txgain = 89
        self.TAGC_tx_median_target = target_median
        self.TAGC_tx_median_max = self.TAGC_tx_median_target*1.4
        self.TAGC_tx_median_min = self.TAGC_tx_median_target/1.4

        # Constants for Agc
        self.RAGC_min_rxgain = 25
        self.RAGC_rx_median_target = self.TAGC_tx_median_target

        # Constants for Model_PM
        self.MPM_tx_min = 0.1

        # Constants for Measure_Shoulder
        assert sample_rate==8192000
        meas_offset = 976 # Offset from center frequency to measure shoulder [kHz]
        meas_width = 100 # Size of frequency delta to measure shoulder [kHz]
        shoulder_offset_edge = np.abs(meas_offset - self.FFT_delta)
        self.MS_shoulder_left_start = self.FFT_start - shoulder_offset_edge - meas_width / 2
        self.MS_shoulder_left_end = self.FFT_start - shoulder_offset_edge + meas_width / 2
        self.MS_shoulder_right_start = self.FFT_end + shoulder_offset_edge - meas_width / 2
        self.MS_shoulder_right_end = self.FFT_end + shoulder_offset_edge + meas_width / 2
        self.MS_peak_start = self.FFT_start + 100 # Ignore region near edges
        self.MS_peak_end = self.FFT_end - 100

        self.MS_FFT_size = 8192
        self.MS_averaging_size = 4 * self.MS_FFT_size
        self.MS_n_averaging = 40
        self.MS_n_proc = 4
