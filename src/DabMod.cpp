/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyright (C) 2023
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

#include <fftw3.h>
#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <memory>
#include <complex>
#include <string>
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <stdexcept>
#include <cstdio>
#include <cstring>
#include <cstddef>
#include <sys/stat.h>
#include <signal.h>

#if HAVE_NETINET_IN_H
#   include <netinet/in.h>
#endif

#include "Events.h"
#include "Utils.h"
#include "Log.h"
#include "DabModulator.h"
#include "InputMemory.h"
#include "OutputFile.h"
#include "FormatConverter.h"
#include "FrameMultiplexer.h"
#include "output/SDR.h"
#include "output/UHD.h"
#include "output/Soapy.h"
#include "output/Dexter.h"
#include "output/Lime.h"
#include "output/BladeRF.h"
#include "OutputZeroMQ.h"
#include "InputReader.h"
#include "PcDebug.h"
#include "FIRFilter.h"
#include "RemoteControl.h"
#include "ConfigParser.h"

/* UHD requires the input I and Q samples to be in the interval
 * [-1.0,1.0], otherwise they get truncated, which creates very
 * wide-spectrum spikes. Depending on the Transmission Mode, the
 * Gain Mode and the sample rate (and maybe other parameters), the
 * samples can have peaks up to about 48000. The value of 50000
 * should guarantee that with a digital gain of 1.0, UHD never clips
 * our samples.
 */
static const float normalise_factor = 50000.0f;

//Empirical normalisation factors used to normalise the samples to amplitude 1.
static const float normalise_factor_file_fix = 81000.0f;
static const float normalise_factor_file_var = 46000.0f;
static const float normalise_factor_file_max = 46000.0f;

typedef std::complex<float> complexf;

using namespace std;

volatile sig_atomic_t running = 1;

void signalHandler(int signalNb)
{
    PDEBUG("signalHandler(%i)\n", signalNb);

    running = 0;
}

class ModulatorData : public RemoteControllable {
    public:
        // For ETI
        std::shared_ptr<InputReader> inputReader;
        std::shared_ptr<EtiReader> etiReader;

        // For EDI
        std::shared_ptr<EdiInput> ediInput;

        // Common to both EDI and EDI
        uint64_t framecount = 0;
        Flowgraph *flowgraph = nullptr;


        // RC-related
        ModulatorData() : RemoteControllable("mainloop") {
            RC_ADD_PARAMETER(num_modulator_restarts, "(Read-only) Number of mod restarts");
            RC_ADD_PARAMETER(most_recent_edi_decoded, "(Read-only) UNIX Timestamp of most recently decoded EDI frame");
            RC_ADD_PARAMETER(edi_source, "(Read-only) URL of the EDI/TCP source");
            RC_ADD_PARAMETER(running_since, "(Read-only) UNIX Timestamp of most recent modulator restart");
            RC_ADD_PARAMETER(ensemble_label, "(Read-only) Label of the ensemble");
            RC_ADD_PARAMETER(ensemble_eid, "(Read-only) Ensemble ID");
            RC_ADD_PARAMETER(ensemble_services, "(Read-only, only JSON) Ensemble service information");
            RC_ADD_PARAMETER(num_services, "(Read-only) Number of services in the ensemble");
        }

        virtual ~ModulatorData() {}

        virtual void set_parameter(const std::string& parameter, const std::string& value) {
            throw ParameterError("Parameter " + parameter + " is read-only");
        }

        virtual const std::string get_parameter(const std::string& parameter) const {
            stringstream ss;
            if (parameter == "num_modulator_restarts") {
                ss << num_modulator_restarts;
            }
            else if (parameter == "running_since") {
                ss << running_since;
            }
            else if (parameter == "most_recent_edi_decoded") {
                ss << most_recent_edi_decoded;
            }
            else if (parameter == "ensemble_label") {
                if (ediInput) {
                    const auto ens = ediInput->ediReader.getEnsembleInfo();
                    if (ens) {
                        ss << FICDecoder::ConvertLabelToUTF8(ens->label, nullptr);
                    }
                    else {
                        throw ParameterError("Not available yet");
                    }
                }
                else {
                    throw ParameterError("Not available yet");
                }
            }
            else if (parameter == "ensemble_eid") {
                if (ediInput) {
                    const auto ens = ediInput->ediReader.getEnsembleInfo();
                    if (ens) {
                        ss << ens->eid;
                    }
                    else {
                        throw ParameterError("Not available yet");
                    }
                }
                else {
                    throw ParameterError("Not available yet");
                }
            }
            else if (parameter == "edi_source") {
                if (ediInput) {
                    ss << ediInput->ediTransport.getTcpUri();
                }
                else {
                    throw ParameterError("Not available yet");
                }
            }
            else if (parameter == "num_services") {
                if (ediInput) {
                    ss << ediInput->ediReader.getSubchannels().size();
                }
                else {
                    throw ParameterError("Not available yet");
                }
            }
            else if (parameter == "ensemble_services") {
                throw ParameterError("ensemble_services is only available through 'showjson'");
            }
            else {
                ss << "Parameter '" << parameter <<
                    "' is not exported by controllable " << get_rc_name();
                throw ParameterError(ss.str());
            }
            return ss.str();
        }

        virtual const json::map_t get_all_values() const
        {
            json::map_t map;
            map["num_modulator_restarts"].v = num_modulator_restarts;
            map["running_since"].v = running_since;
            map["most_recent_edi_decoded"].v = most_recent_edi_decoded;

            if (ediInput) {
                map["edi_source"].v = ediInput->ediTransport.getTcpUri();
                map["num_services"].v = ediInput->ediReader.getSubchannels().size();

                const auto ens = ediInput->ediReader.getEnsembleInfo();
                if (ens) {
                    map["ensemble_label"].v = FICDecoder::ConvertLabelToUTF8(ens->label, nullptr);
                    map["ensemble_eid"].v = ens->eid;
                }
                else {
                    map["ensemble_label"].v = nullopt;
                    map["ensemble_eid"].v = nullopt;
                }

                std::vector<json::value_t> services;

                for (const auto& s : ediInput->ediReader.getServiceInfo()) {
                    auto service_map = make_shared<json::map_t>();
                    (*service_map)["sad"].v = s.second.subchannel.start;
                    (*service_map)["sid"].v = s.second.sid;
                    (*service_map)["label"].v = FICDecoder::ConvertLabelToUTF8(s.second.label, nullptr);
                    (*service_map)["bitrate"].v = s.second.subchannel.bitrate;
                    (*service_map)["protection_level"].v = s.second.subchannel.pl;
                    json::value_t v;
                    v.v = service_map;
                    services.push_back(v);
                }

                map["ensemble_services"].v = services;

            }
            return map;
        }

        size_t num_modulator_restarts = 0;
        time_t most_recent_edi_decoded = 0;
        time_t running_since = 0;
};

enum class run_modulator_state_t {
    failure,    // Corresponds to all failures
    normal_end, // Number of frames to modulate was reached
    again,      // Restart the modulator part
    reconfigure // Some sort of change of configuration we cannot handle happened
};

static run_modulator_state_t run_modulator(const mod_settings_t& mod_settings, ModulatorData& m);


static shared_ptr<ModOutput> prepare_output(mod_settings_t& s)
{
    shared_ptr<ModOutput> output;

    if (s.useFileOutput) {
        if (s.fileOutputFormat == "complexf") {
            output = make_shared<OutputFile>(s.outputName, s.fileOutputShowMetadata);
        }
        else if (s.fileOutputFormat == "complexf_normalised") {
            if (s.gainMode == GainMode::GAIN_FIX)
                s.normalise = 1.0f / normalise_factor_file_fix;
            else if (s.gainMode == GainMode::GAIN_MAX)
                s.normalise = 1.0f / normalise_factor_file_max;
            else if (s.gainMode == GainMode::GAIN_VAR)
                s.normalise = 1.0f / normalise_factor_file_var;
            output = make_shared<OutputFile>(s.outputName, s.fileOutputShowMetadata);
        }
        else if (s.fileOutputFormat == "s16") {
            // We must normalise the samples to the interval [-32767.0; 32767.0]
            s.normalise = 32767.0f / normalise_factor;

            output = make_shared<OutputFile>(s.outputName, s.fileOutputShowMetadata);
        }
        else if (s.fileOutputFormat == "s8" or
                s.fileOutputFormat == "u8") {
            // We must normalise the samples to the interval [-127.0; 127.0]
            // The formatconverter will add 127 for u8 so that it ends up in
            // [0; 255]
            s.normalise = 127.0f / normalise_factor;

            output = make_shared<OutputFile>(s.outputName, s.fileOutputShowMetadata);
        }
        else {
            throw runtime_error("File output format " + s.fileOutputFormat +
                    " not known");
        }
    }
#if defined(HAVE_OUTPUT_UHD)
    else if (s.useUHDOutput) {
        s.normalise = 1.0f / normalise_factor;
        s.sdr_device_config.sampleRate = s.outputRate;
        auto uhddevice = make_shared<Output::UHD>(s.sdr_device_config);
        output = make_shared<Output::SDR>(s.sdr_device_config, uhddevice);
        rcs.enrol((Output::SDR*)output.get());
    }
#endif
#if defined(HAVE_SOAPYSDR)
    else if (s.useSoapyOutput) {
        /* We normalise the same way as for the UHD output */
        s.normalise = 1.0f / normalise_factor;
        s.sdr_device_config.sampleRate = s.outputRate;
        auto soapydevice = make_shared<Output::Soapy>(s.sdr_device_config);
        output = make_shared<Output::SDR>(s.sdr_device_config, soapydevice);
        rcs.enrol((Output::SDR*)output.get());
    }
#endif
#if defined(HAVE_DEXTER)
    else if (s.useDexterOutput) {
        /* We normalise specifically range [-32768; 32767] */
        s.normalise = 32767.0f / normalise_factor;
        s.sdr_device_config.sampleRate = s.outputRate;
        auto dexterdevice = make_shared<Output::Dexter>(s.sdr_device_config);
        output = make_shared<Output::SDR>(s.sdr_device_config, dexterdevice);
        rcs.enrol((Output::SDR*)output.get());
    }
#endif
#if defined(HAVE_LIMESDR)
    else if (s.useLimeOutput) {
        /* We normalise the same way as for the UHD output */
        s.normalise = 1.0f / normalise_factor;
        s.sdr_device_config.sampleRate = s.outputRate;
        auto limedevice = make_shared<Output::Lime>(s.sdr_device_config);
        output = make_shared<Output::SDR>(s.sdr_device_config, limedevice);
        rcs.enrol((Output::SDR*)output.get());
    }
#endif
#if defined(HAVE_BLADERF)
    else if (s.useBladeRFOutput) {
        /* We normalise specifically for the BladeRF output : range [-2048; 2047] */
        s.normalise = 2047.0f / normalise_factor;
        s.sdr_device_config.sampleRate = s.outputRate;
        auto bladerfdevice = make_shared<Output::BladeRF>(s.sdr_device_config);
        output = make_shared<Output::SDR>(s.sdr_device_config, bladerfdevice);
        rcs.enrol((Output::SDR*)output.get());
    }
#endif
#if defined(HAVE_ZEROMQ)
    else if (s.useZeroMQOutput) {
        /* We normalise the same way as for the UHD output */
        s.normalise = 1.0f / normalise_factor;
        if (s.zmqOutputSocketType == "pub") {
            output = make_shared<OutputZeroMQ>(s.outputName, ZMQ_PUB);
        }
        else if (s.zmqOutputSocketType == "rep") {
            output = make_shared<OutputZeroMQ>(s.outputName, ZMQ_REP);
        }
        else {
            std::stringstream ss;
            ss << "ZeroMQ output socket type " << s.zmqOutputSocketType << " invalid";
            throw std::invalid_argument(ss.str());
        }
    }
#endif

    return output;
}

int launch_modulator(int argc, char* argv[])
{
    int ret = 0;

    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = &signalHandler;

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        const string errstr = strerror(errno);
        throw runtime_error("Could not set signal handler: " + errstr);
    }

    printStartupInfo();

    mod_settings_t mod_settings;
    parse_args(argc, argv, mod_settings);

#if defined(HAVE_ZEROMQ)
    etiLog.register_backend(make_shared<LogToEventSender>());
#endif // defined(HAVE_ZEROMQ)

    etiLog.level(info) << "Configuration parsed. Starting up version " <<
#if defined(GITVERSION)
            GITVERSION;
#else
            VERSION;
#endif

    if (not (mod_settings.useFileOutput or
             mod_settings.useUHDOutput or
             mod_settings.useZeroMQOutput or
             mod_settings.useSoapyOutput or
             mod_settings.useDexterOutput or
             mod_settings.useLimeOutput or
             mod_settings.useBladeRFOutput)) {
        throw std::runtime_error("Configuration error: Output not specified");
    }

    if (not mod_settings.startupCheck.empty()) {
        etiLog.level(info) << "Running startup check '" << mod_settings.startupCheck << "'";
        int wstatus = system(mod_settings.startupCheck.c_str());

        if (WIFEXITED(wstatus)) {
            if (WEXITSTATUS(wstatus) == 0) {
                etiLog.level(info) << "Startup check ok";
            }
            else {
                etiLog.level(error) << "Startup check failed, returned " << WEXITSTATUS(wstatus);
                return 1;
            }
        }
        else {
            etiLog.level(error) << "Startup check failed, child didn't terminate normally";
            return 1;
        }
    }

    printModSettings(mod_settings);

    ModulatorData m;
    rcs.enrol(&m);

    {
        // This is mostly useful on ARM systems where FFTW planning takes some time. If we do it here
        // it will be done before the modulator starts up
        etiLog.level(debug) << "Running FFTW planning...";
        constexpr size_t fft_size = 2048; // Transmission Mode I. If different, it'll recalculate on OfdmGenerator
                                          // initialisation
        auto *fft_in = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * fft_size);
        auto *fft_out = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * fft_size);
        if (fft_in == nullptr or fft_out == nullptr) {
            throw std::runtime_error("FFTW malloc failed");
        }
        fftwf_set_timelimit(2);
        fftwf_plan plan = fftwf_plan_dft_1d(fft_size, fft_in, fft_out, FFTW_FORWARD, FFTW_MEASURE);
        fftwf_destroy_plan(plan);
        plan = fftwf_plan_dft_1d(fft_size, fft_in, fft_out, FFTW_BACKWARD, FFTW_MEASURE);
        fftwf_destroy_plan(plan);
        fftwf_free(fft_in);
        fftwf_free(fft_out);
        etiLog.level(debug) << "FFTW planning done.";
    }

    std::string output_format;
    if (mod_settings.useFileOutput and
            (mod_settings.fileOutputFormat == "s8" or
             mod_settings.fileOutputFormat == "u8" or
             mod_settings.fileOutputFormat == "s16")) {
        output_format = mod_settings.fileOutputFormat;
    }
    else if (mod_settings.useBladeRFOutput or mod_settings.useDexterOutput) {
        output_format = "s16";
    }

    auto output = prepare_output(mod_settings);

    if (not output_format.empty()) {
        if (auto o = dynamic_pointer_cast<Output::SDR>(output)) {
            o->set_sample_size(FormatConverter::get_format_size(output_format));
        }
    }

    // Set thread priority to realtime
    if (int r = set_realtime_prio(1)) {
        etiLog.level(error) << "Could not set priority for modulator:" << r;
    }

    shared_ptr<InputReader> inputReader;
    shared_ptr<EdiInput> ediInput;

    if (mod_settings.inputTransport == "edi") {
        ediInput = make_shared<EdiInput>(mod_settings.tist_offset_s, mod_settings.edi_max_delay_ms);

        ediInput->ediTransport.Open(mod_settings.inputName);
        if (not ediInput->ediTransport.isEnabled()) {
            throw runtime_error("inputTransport is edi, but ediTransport is not enabled");
        }
    }
    else if (mod_settings.inputTransport == "file") {
        auto inputFileReader = make_shared<InputFileReader>();

        // Opening ETI input file
        if (inputFileReader->Open(mod_settings.inputName, mod_settings.loop) == -1) {
            throw std::runtime_error("Unable to open input");
        }

        inputReader = inputFileReader;
    }
    else if (mod_settings.inputTransport == "tcp") {
        auto inputTcpReader = make_shared<InputTcpReader>();
        inputTcpReader->Open(mod_settings.inputName);
        inputReader = inputTcpReader;
    }
    else {
        throw std::runtime_error("Unable to open input: "
                "invalid input transport " + mod_settings.inputTransport + " selected!");
    }

    m.ediInput = ediInput;
    m.inputReader = inputReader;

    bool run_again = true;

    while (run_again) {
        m.running_since = get_clock_realtime_seconds();

        Flowgraph flowgraph(mod_settings.showProcessTime);

        m.framecount = 0;
        m.flowgraph = &flowgraph;

        shared_ptr<DabModulator> modulator;
        if (inputReader) {
            m.etiReader = make_shared<EtiReader>(mod_settings.tist_offset_s);
            modulator = make_shared<DabModulator>(*m.etiReader, mod_settings, output_format);
        }
        else if (ediInput) {
            modulator = make_shared<DabModulator>(ediInput->ediReader, mod_settings, output_format);
        }

        rcs.enrol(modulator.get());

        flowgraph.connect(modulator, output);

        if (inputReader) {
            etiLog.level(info) << inputReader->GetPrintableInfo();
        }

        run_modulator_state_t st = run_modulator(mod_settings, m);
        etiLog.log(trace, "DABMOD,run_modulator() = %d", st);

        switch (st) {
            case run_modulator_state_t::failure:
                etiLog.level(error) << "Modulator failure.";
                run_again = false;
                ret = 1;
                break;
            case run_modulator_state_t::again:
                etiLog.level(warn) << "Restart modulator.";
                run_again = false;
                if (auto in = dynamic_pointer_cast<InputFileReader>(inputReader)) {
                    if (in->Open(mod_settings.inputName, mod_settings.loop) == -1) {
                        etiLog.level(error) << "Unable to open input file!";
                        ret = 1;
                    }
                    else {
                        run_again = true;
                    }
                }
                else if (dynamic_pointer_cast<InputTcpReader>(inputReader)) {
                    // Keep the same inputReader, as there is no input buffer overflow
                    run_again = true;
                }
                else if (ediInput) {
                    // In EDI, keep the same input
                    run_again = true;
                }
                break;
            case run_modulator_state_t::reconfigure:
                etiLog.level(warn) << "Detected change in ensemble configuration.";
                /* We can keep the input in this case */
                run_again = true;
                break;
            case run_modulator_state_t::normal_end:
            default:
                etiLog.level(info) << "modulator stopped.";
                ret = 0;
                run_again = false;
                break;
        }

        etiLog.level(info) << m.framecount << " DAB frames, " << ((float)m.framecount * 0.024f) << " seconds encoded";
        m.num_modulator_restarts++;
    }

    etiLog.level(info) << "Terminating";
    return ret;
}

static run_modulator_state_t run_modulator(const mod_settings_t& mod_settings, ModulatorData& m)
{
    auto ret = run_modulator_state_t::failure;
    try {
        int last_eti_fct = -1;
        auto last_frame_received = chrono::steady_clock::now();
        frame_timestamp ts;
        Buffer data;
        if (m.inputReader) {
            data.setLength(6144);
        }

        while (running) {
            unsigned fct = 0;
            unsigned fp = 0;

            /* Load ETI data from the source */
            if (m.inputReader) {
                int framesize = m.inputReader->GetNextFrame(data.getData());

                if (framesize == 0) {
                    if (dynamic_pointer_cast<InputFileReader>(m.inputReader)) {
                        etiLog.level(info) << "End of file reached.";
                        running = 0;
                        ret = run_modulator_state_t::normal_end;
                        break;
                    }
                    else if (dynamic_pointer_cast<InputTcpReader>(m.inputReader)) {
                        /* An empty frame marks a timeout. We ignore it, but we are
                         * now able to handle SIGINT properly. */
                    }
                    else {
                        throw logic_error("Unhandled framesize==0!");
                    }
                    continue;
                }
                else if (framesize < 0) {
                    etiLog.level(error) << "Input read error.";
                    running = 0;
                    ret = run_modulator_state_t::normal_end;
                    break;
                }

                const int eti_bytes_read = m.etiReader->loadEtiData(data);
                if ((size_t)eti_bytes_read != data.getLength()) {
                    etiLog.level(error) << "ETI frame incompletely read";
                    throw std::runtime_error("ETI read error");
                }

                last_frame_received = chrono::steady_clock::now();

                fct = m.etiReader->getFct();
                fp = m.etiReader->getFp();
                ts = m.etiReader->getTimestamp();
            }
            else if (m.ediInput) {
                while (running and not m.ediInput->ediReader.isFrameReady()) {
                    try {
                        bool packet_received = m.ediInput->ediTransport.rxPacket();
                        if (packet_received) {
                            last_frame_received = chrono::steady_clock::now();
                        }
                    }
                    catch (const std::runtime_error& e) {
                        etiLog.level(warn) << "EDI input: " << e.what();
                        running = 0;
                        break;
                    }
                }

                if (!running) {
                    break;
                }

                m.most_recent_edi_decoded = get_clock_realtime_seconds();
                fct = m.ediInput->ediReader.getFct();
                fp = m.ediInput->ediReader.getFp();
                ts = m.ediInput->ediReader.getTimestamp();
            }

            // timestamp is good if we run unsynchronised, or if margin is sufficient
            bool ts_good = not mod_settings.sdr_device_config.enableSync or
                (ts.timestamp_valid and ts.offset_to_system_time() > 0.2);

            if (!ts_good) {
                etiLog.level(warn) << "Modulator skipping frame " << fct <<
                    " TS " << (ts.timestamp_valid ? "valid" : "invalid") <<
                    " offset " << (ts.timestamp_valid ? ts.offset_to_system_time() : 0);
            }
            else {
                bool modulate = true;
                if (last_eti_fct == -1) {
                    if (fp != 0) {
                        // Do not start the flowgraph before we get to FP 0
                        // to ensure all blocks are properly aligned.
                        modulate = false;
                    }
                    else {
                        last_eti_fct = fct;
                    }
                }
                else {
                    const unsigned expected_fct = (last_eti_fct + 1) % 250;
                    if (fct == expected_fct) {
                        last_eti_fct = fct;
                    }
                    else {
                        etiLog.level(warn) << "ETI FCT discontinuity, expected " <<
                            expected_fct << " received " << fct;
                        if (m.ediInput) {
                            m.ediInput->ediReader.clearFrame();
                        }
                        return run_modulator_state_t::again;
                    }
                }

                if (modulate) {
                    m.framecount++;
                    m.flowgraph->run();
                }
            }

            if (m.ediInput) {
                m.ediInput->ediReader.clearFrame();
            }

            /* Check every once in a while if the remote control
             * is still working */
            if ((m.framecount % 250) == 0) {
                rcs.check_faults();
            }
        }
    }
    catch (const FrameMultiplexerError& e) {
        // The FrameMultiplexer saw an error or a change in the size of a
        // subchannel. This can be due to a multiplex reconfiguration.
        etiLog.level(warn) << e.what();
        ret = run_modulator_state_t::reconfigure;
    }
    catch (const std::exception& e) {
        etiLog.level(error) << "Exception caught: " << e.what();
        ret = run_modulator_state_t::failure;
    }

    return ret;
}

int main(int argc, char* argv[])
{
    // Set timezone to UTC
    setenv("TZ", "", 1);
    tzset();

    // Version handling is done very early to ensure nothing else but the version gets printed out
    if (argc == 2 and strcmp(argv[1], "--version") == 0) {
        fprintf(stdout, "%s\n",
#if defined(GITVERSION)
                GITVERSION
#else
                PACKAGE_VERSION
#endif
               );
        return 0;
    }

    try {
        return launch_modulator(argc, argv);
    }
    catch (const std::invalid_argument& e) {
        std::string what(e.what());
        if (not what.empty()) {
            std::cerr << "Modulator error: " << what << std::endl;
        }
    }
    catch (const std::runtime_error& e) {
        std::cerr << "Modulator runtime error: " << e.what() << std::endl;
    }

    return 1;
}

