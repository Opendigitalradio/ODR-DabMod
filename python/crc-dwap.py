#!/usr/bin/env python

# Copyright (C) 2006, 2007, 2008, 2009,-2010 Her Majesty the Queen in
# Right of Canada (Communications Research Center Canada)

# This file is part of CRC-DADMOD.
#
# CRC-DADMOD is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# CRC-DADMOD is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with CRC-DADMOD.  If not, see <http://www.gnu.org/licenses/>.


from wxPython.wx import *
from optparse import OptionParser
from gnuradio import gr
from gnuradio import usrp
from gnuradio.wxgui import fftsink, scopesink
from gnuradio.eng_notation import num_to_str
from gnuradio.eng_option import *

ID_ABOUT = wxNewId()
ID_EXIT  = wxNewId()
ID_GAIN_SLIDER0 = wxNewId()
ID_FREQ_SLIDER0 = wxNewId()
ID_GAIN_SLIDER1 = wxNewId()
ID_FREQ_SLIDER1 = wxNewId()
ID_START = wxNewId()
ID_STOP = wxNewId()

def gcd(a, b) :
    if b == 0 :
        return a
    return gcd(b, a % b)


def appendFrequency(option, opt, value, parser):
    if parser.values.frequency is None :
        parser.values.frequency = [ value ]
    else :
        parser.values.frequency.append(value)

def listUsrp(option, opt, value, parser):
    id = 0
    while (true) :
        try:
            version = usrp._look_for_usrp(id)
            print "USRP #%i" % id
            print " Rev: %i" % version
            dst = usrp.sink_c(id)
            src = usrp.source_c(id)
            print " Tx"
            for db in dst.db:
                if (db[0].dbid() != -1):
                    print "  %s" % db[0].side_and_name()
                    (min, max, offset) = db[0].freq_range()
                    print "   Frequency"
                    print "    Min:    %sHz" % num_to_str(min)
                    print "    Max:    %sHz" % num_to_str(max)
                    print "    Offset: %sHz" % num_to_str(offset)
                    (min, max, offset) = db[0].gain_range()
                    print "   Gain"
                    print "    Min:    %sdB" % num_to_str(min)
                    print "    Max:    %sdB" % num_to_str(max)
                    print "    Offset: %sdB" % num_to_str(offset)
            print " Rx"
            for db in src.db:
                if (db[0].dbid() != -1):
                    print "  %s" % db[0].side_and_name()
                    (min, max, offset) = db[0].freq_range()
                    print "   Frequency"
                    print "    Min:    %sHz" % num_to_str(min)
                    print "    Max:    %sHz" % num_to_str(max)
                    print "    Offset: %sHz" % num_to_str(offset)
                    (min, max, offset) = db[0].gain_range()
                    print "   Gain"
                    print "    Min:    %sdB" % num_to_str(min)
                    print "    Max:    %sdB" % num_to_str(max)
                    print "    Offset: %sdB" % num_to_str(offset)
        except RuntimeError:
            break
        id += 1

    raise SystemExit

class MyFrame(wxFrame):
    def __init__(self, parent, ID, title):
        wxFrame.__init__(self, parent, ID, title,
                wxDefaultPosition)

        self.pga = 0
        self.pgaMin = -20
        self.pgaMax = 0
        self.pgaStep = 0.25

# Parsing options
        parser = OptionParser(option_class=eng_option,
                usage="usage: %prog [options] filename1" \
                " [-f frequency2 filename2 [...]]")
        parser.add_option("-a", "--agc", action="store_true",
                help="enable agc")
        parser.add_option("-c", "--clockrate", type="eng_float", default=128e6,
                help="set USRP clock rate (128e6)")
        parser.add_option("--copy", action="store_true",
                help="enable real to imag data copy when in real mode")
        parser.add_option("-e", "--encoding", type="choice", choices=["s", "f"],
                default="f", help="choose data encoding: [s]igned or [f]loat.")
        parser.add_option("-f", "--frequency", type="eng_float",
                action="callback", callback=appendFrequency,
                help="set output frequency (222.064e6)")
        parser.add_option("-g", "--gain", type="float",
                help="set output pga gain")
        parser.add_option("-l", "--list", action="callback", callback=listUsrp,
                help="list USRPs and daugtherboards")
        parser.add_option("-m", "--mode", type="eng_float", default=2,
                help="mode: 1: real, 2: complex (2)")
        parser.add_option("-o", "--osc", action="store_true",
                help="enable oscilloscope")
        parser.add_option("-r", "--samplingrate", type="eng_float",
                default=3.2e6,
                help="set input sampling rate (3200000)")
        parser.add_option("-s", "--spectrum", action="store_true",
                help="enable spectrum analyzer")
#        parser.add_option("-t", "--tx", type="choice", choices=["A", "B"],
#                default="A", help="choose USRP tx A|B output (A)")
        parser.add_option("-u", "--usrp", action="store_true",
                help="enable USRP output")

        (options, args) = parser.parse_args()
        if len(args) == 0 :
            options.filename = [ "/dev/stdin" ]
        else :
            options.filename = args
# Setting default frequency
        if options.frequency is None :
            options.frequency = [ 222.064e6 ]
        if len(options.filename) != len(options.frequency) :
            parser.error("Nb input file != nb frequency!")

# Status bar
#        self.CreateStatusBar(3, 0)
#        msg = "PGA: %.2f dB" % (self.pga * self.pgaStep)
#        self.SetStatusText(msg, 1)
#        msg = "Freq: %.3f mHz" % (options.frequency[0] / 1000000.0)
#        self.SetStatusText(msg, 2)

# Menu bar
        menu = wxMenu()
        menu.Append(ID_ABOUT, "&About",
                "More information about this program")
        menu.AppendSeparator()
        menu.Append(ID_EXIT, "E&xit", "Terminate the program")
        menuBar = wxMenuBar()
        menuBar.Append(menu, "&File")
        self.SetMenuBar(menuBar)
        

# Main windows
        mainSizer = wxFlexGridSizer(0, 1)
        sliderSizer = wxFlexGridSizer(0, 2)
        buttonSizer = wxBoxSizer(wxHORIZONTAL)

        if options.usrp :
            # TX d'board 0
            gainLabel = wxStaticText(self, -1, "PGA 0")
            gainSlider = wxSlider(self, ID_GAIN_SLIDER0, self.pga,
                    self.pgaMin / self.pgaStep, self.pgaMax / self.pgaStep,
                    style = wxSL_HORIZONTAL | wxSL_AUTOTICKS)
            gainSlider.SetSize((400, -1))
            sliderSizer.Add(gainLabel, 0,
                    wxALIGN_CENTER_VERTICAL | wxFIXED_MINSIZE, 0)
            sliderSizer.Add(gainSlider, 0,
                    wxALIGN_CENTER_VERTICAL | wxFIXED_MINSIZE, 0)

            freqLabel = wxStaticText(self, -1, "Frequency 0")
            freqSlider = wxSlider(self, ID_FREQ_SLIDER0,
                    options.frequency[0] / 16000, 0, 20e3,
                    style = wxSL_HORIZONTAL | wxSL_AUTOTICKS)
            freqSlider.SetSize((400, -1))
            sliderSizer.Add(freqLabel, 0,
                    wxALIGN_CENTER_VERTICAL | wxFIXED_MINSIZE, 0)
            sliderSizer.Add(freqSlider, 0,
                    wxALIGN_CENTER_VERTICAL | wxFIXED_MINSIZE, 0)

            if len(options.frequency) > 1 :
                # TX d'board 1
                gainLabel = wxStaticText(self, -1, "PGA 1")
                gainSlider = wxSlider(self, ID_GAIN_SLIDER1, self.pga,
                        self.pgaMin / self.pgaStep, self.pgaMax / self.pgaStep,
                        style = wxSL_HORIZONTAL | wxSL_AUTOTICKS)
                gainSlider.SetSize((400, -1))
                sliderSizer.Add(gainLabel, 0,
                        wxALIGN_CENTER_VERTICAL | wxFIXED_MINSIZE, 0)
                sliderSizer.Add(gainSlider, 0,
                        wxALIGN_CENTER_VERTICAL | wxFIXED_MINSIZE, 0)

                freqLabel = wxStaticText(self, -1, "Frequency 1")
                freqSlider = wxSlider(self, ID_FREQ_SLIDER1,
                        options.frequency[1] / 16000, 0, 20e3,
                        style = wxSL_HORIZONTAL | wxSL_AUTOTICKS)
                freqSlider.SetSize((400, -1))
                sliderSizer.Add(freqLabel, 0,
                        wxALIGN_CENTER_VERTICAL | wxFIXED_MINSIZE, 0)
                sliderSizer.Add(freqSlider, 0,
                        wxALIGN_CENTER_VERTICAL | wxFIXED_MINSIZE, 0)

            mainSizer.Add(sliderSizer, 1, wxEXPAND, 0)

        start = wxButton(self, ID_START, "Start")
        stop = wxButton(self, ID_STOP, "Stop")
        buttonSizer.Add(start, 1, wxALIGN_CENTER, 0)
        buttonSizer.Add(stop, 1, wxALIGN_CENTER, 0)
        mainSizer.Add(buttonSizer, 1, wxEXPAND, 0)
    
# GnuRadio
        self.fg = gr.flow_graph()
        if options.mode == 1 :
            print "Source: real"
            if (options.encoding == "s") :
                print "Source encoding: short"
                src = gr.file_source(gr.sizeof_short, options.filename[0], 1)
                if (options.copy) :
                    print "Imag: copy"
                    imag = src
                else :
                    print "Imag: null"
                    imag = gr.null_source(gr.sizeof_short)
                interleaver = gr.interleave(gr.sizeof_short)
                self.fg.connect(src, (interleaver, 0))
                self.fg.connect(imag, (interleaver, 1))
                tail = interleaver
            elif (options.encoding == "f") :
                print "Source encoding: float"
                src = gr.file_source(gr.sizeof_gr_complex,
                    options.filename[0], 1)
                tail = src
        elif (options.mode == 2) :
            print "Source: complex"
            if len(options.frequency) == 1 :
                if (options.encoding == "s") :
                    print "Source encoding: short"
                    src = gr.file_source(gr.sizeof_short,
                            options.filename[0], 1)
                elif (options.encoding == "f") :
                    print "Source encoding: float"
                    src = gr.file_source(gr.sizeof_gr_complex,
                            options.filename[0], 1)
                else :
                    parser.error("Invalid encoding type for complex data!")
                tail = src
                    
            elif (len(options.frequency) == 2) :
                src0 = gr.file_source(gr.sizeof_gr_complex,
                        options.filename[0], 1)
                src1 = gr.file_source(gr.sizeof_gr_complex,
                        options.filename[1], 1)
                interleaver = gr.interleave(gr.sizeof_gr_complex)
                self.fg.connect(src0, (interleaver, 0))
                self.fg.connect(src1, (interleaver, 1))
                tail = interleaver
            else :
                parser.error(
                        "Invalid number of source (> 2) with complex input!")
        else :
            parser.error("Invalid mode!")

# Interpolation
        dac_freq = options.clockrate
        interp = int(dac_freq / options.samplingrate)
        if interp == 0 :
            parser.error("Invalid sampling rate!")
        if options.mode == 2 :
            print "Input sampling rate: %s complex samples/s" % \
                num_to_str(options.samplingrate)
        else :
            print "Input sampling rate: %s samples/s" % \
                num_to_str(options.samplingrate)
        print "Interpolation rate: int(%s / %s) = %sx" % \
            (num_to_str(dac_freq), num_to_str(options.samplingrate), interp)
        if interp > 512 :
            factor = gcd(dac_freq / 512, options.samplingrate)
            num = int((dac_freq / 512) / factor)
            den = int(options.samplingrate / factor)
            print "Resampling by %i / %i" % (num, den)
            resampler = blks.rational_resampler_ccc(self.fg, num, den)
            self.fg.connect(tail, resampler)
            tail = resampler
            interp = 512
            options.samplingrate = dac_freq / 512

# AGC
        if options.agc :
            agc = gr.agc_cc()
            self.fg.connect(tail, agc)
            tail = agc
            
# USRP
        if options.usrp :
            nchan = len(options.frequency)
            if len(options.frequency) == 1 :
                if options.mode == 1 :
                    mux = 0x00000098
                elif options.mode == 2 :
                    mux = 0x00000098
                else :
                    parser.error("Unsupported mode for USRP mux!")
            elif len(options.frequency) == 2 :
                if options.mode == 1 :
                    mux = 0x0000ba98
                elif options.mode == 2 :
                    mux = 0x0000ba98
                else :
                    parser.error("Unsupported mode for USRP mux!")
            else :
                parser.error("Invalid number of frequency [0..2]!")
#            if options.tx == "A" :
#                mux = 0x00000098
#            else :
#                mux = 0x00009800
            print "Nb channels: ", nchan
            print "Mux: 0x%x" % mux
            if options.encoding == 's' :
                dst = usrp.sink_s(0, interp, nchan, mux)
            elif options.encoding == 'f' :
                dst = usrp.sink_c(0, interp, nchan, mux)
            else :
                parser.error("Unsupported data encoding for USRP!")
            
            dst.set_verbose(1)

            for i in range(len(options.frequency)) :
                if options.gain is None :
                    print "Setting gain to %f" % dst.pga_max()
                    dst.set_pga(i << 1, dst.pga_max())
                else :
                    print "Setting gain to %f" % options.gain
                    dst.set_pga(i << 1, options.gain)

                tune = false
                for dboard in dst.db:
                    if (dboard[0].dbid() != -1):
                        device = dboard[0]
                        print "Tuning TX d'board %s to %sHz" % \
                                (device.side_and_name(),
                                num_to_str(options.frequency[i]))
                        device.lo_offset = 38e6
                        (min, max, offset) = device.freq_range()
                        print " Frequency"
                        print "  Min:    %sHz" % num_to_str(min)
                        print "  Max:    %sHz" % num_to_str(max)
                        print "  Offset: %sHz" % num_to_str(offset)
#device.set_gain(device.gain_range()[1])
                        device.set_enable(True)
                        tune = \
                            dst.tune(device._which, device,
                                    options.frequency[i] * 128e6 / dac_freq)
                        if tune:
                            print "  Baseband frequency: %sHz" % \
                                num_to_str(tune.baseband_freq)
                            print "  DXC frequency: %sHz" % \
                                num_to_str(tune.dxc_freq)
                            print "  Residual Freqency: %sHz" % \
                                num_to_str(tune.residual_freq)
                            print "  Inverted: ", \
                                tune.inverted
                            mux = usrp.determine_tx_mux_value(dst,
                                    (device._which, 0))
                            dst.set_mux(mux)
                            break
                        else:
                            print "  Failed!"
                if not tune:
                    print "  Failed!"
                    raise SystemExit

# int nunderruns ()

            print "USRP"
            print " Rx halfband: ", dst.has_rx_halfband()
            print " Tx halfband: ", dst.has_tx_halfband()
            print " Nb DDC: ", dst.nddc()
            print " Nb DUC: ", dst.nduc()
#dst._write_9862(0, 14, 224)
            
            print " DAC frequency: %s samples/s" % num_to_str(dst.dac_freq())
            print " Fpga decimation rate: %s -> %s samples/s" % \
                (num_to_str(dst.interp_rate()),
                 num_to_str(dac_freq / dst.interp_rate()))
            print " Nb channels:",
            if hasattr(dst, "nchannels()") :
                print dst.nchannels()
            else:
                print "N/A"
            print " Mux:",
            if hasattr(dst, "mux()") :
                print "0x%x" % dst.mux()
            else :
                print "N/A"
            print " FPGA master clock frequency:",
            if hasattr(dst, "fpga_master_clock_freq()") :
                print "%sHz" % num_to_str(dst.fpga_master_clock_freq())
            else :
                print "N/A"
            print " Converter rate:",
            if hasattr(dst, "converter_rate()") :
                print "%s" % num_to_str(dst.converter_rate())
            else :
                print "N/A"
            print " DAC rate:",
            if hasattr(dst, "dac_rate()") :
                print "%s sample/s" % num_to_str(dst.dac_rate())
            else :
                print "N/A"
            print " Interp rate: %sx" % num_to_str(dst.interp_rate())
            print " DUC frequency 0: %sHz" % num_to_str(dst.tx_freq(0))
            print " DUC frequency 1: %sHz" % num_to_str(dst.tx_freq(1))
            print " Programmable Gain Amplifier 0: %s dB" % \
                num_to_str(dst.pga(0))
            print " Programmable Gain Amplifier 1: %s dB" % \
                num_to_str(dst.pga(2))

        else :
            dst = gr.null_sink(gr.sizeof_gr_complex)
            
# AGC
        if options.agc :
            agc = gr.agc_cc()
            self.fg.connect(tail, agc)
            tail = agc
            
        self.fg.connect(tail, dst)

# oscilloscope
        if options.osc :
            oscPanel = wxPanel(self, -1)
            if (options.encoding == "s") :
                converter = gr.interleaved_short_to_complex()
                self.fg.connect(tail, converter)
                signal = converter
            elif (options.encoding == "f") :
                signal = tail
            else :
                parser.error("Unsupported data encoding for oscilloscope!")

#block = scope_sink_f(fg, parent, title=label, sample_rate=input_rate)
#return (block, block.win)

            oscWin = scopesink.scope_sink_c(self.fg, oscPanel, "Signal",
                    options.samplingrate)
            self.fg.connect(signal, oscWin)
            mainSizer.Add(oscPanel, 1, wxEXPAND)

# spectrometer
        if options.spectrum :
            ymin = 0
            ymax = 160
            fftPanel = wxPanel(self, -1)
            if (options.encoding == "s") :
                converter = gr.interleaved_short_to_complex()
                self.fg.connect(tail, converter)
                signal = converter
            elif (options.encoding == "f") :
                signal = tail
            else :
                parser.error("Unsupported data encoding for oscilloscope!")

            fftWin = fftsink.fft_sink_c(self.fg, fftPanel,
                    title="Spectrum",
                    fft_size=2048,
                    sample_rate=options.samplingrate,
                    y_per_div=(ymax - ymin) / 8,
                    ref_level=ymax,
                    fft_rate=50,
                    average=True
                    )
            self.fg.connect(signal, fftWin)
            mainSizer.Add(fftPanel, 1, wxEXPAND)

# Events
        EVT_MENU(self, ID_ABOUT, self.OnAbout)
        EVT_MENU(self, ID_EXIT,  self.TimeToQuit)
        EVT_SLIDER(self, ID_GAIN_SLIDER0, self.slideEvent)
        EVT_SLIDER(self, ID_FREQ_SLIDER0, self.slideEvent)
        EVT_SLIDER(self, ID_GAIN_SLIDER1, self.slideEvent)
        EVT_SLIDER(self, ID_FREQ_SLIDER1, self.slideEvent)
        EVT_BUTTON(self, ID_START, self.onClick)
        EVT_BUTTON(self, ID_STOP, self.onClick)

#Layout sizers
        self.SetSizer(mainSizer)
        self.SetAutoLayout(1)
        mainSizer.Fit(self)

        self.fg.start()

    def OnAbout(self, event):
        dlg = wxMessageDialog(self, "This sample program shows off\n"
                "frames, menus, statusbars, and this\n"
                "message dialog.",
                "About Me", wxOK | wxICON_INFORMATION)
        dlg.ShowModal()
        dlg.Destroy()


    def TimeToQuit(self, event):
        self.Close(true)
    
    def slideEvent(self, evt):
        value = evt.GetInt()
        id = evt.GetId()
        if id == ID_GAIN_SLIDER:
            msg = "PGA: %.2f dB" % (value * self.pgaStep)
            self.SetStatusText(msg, 1)
        elif id == ID_FREQ_SLIDER:
            msg = "Freq: %.3f mHz" % (value * 16.0 / 1000)
            self.SetStatusText(msg, 2)
        else:
            print "Slider event not yet coded!"
            self.Close(True)
        
    def onClick(self, event):
        id = event.GetId()
        if id == ID_START:
            self.fg.start()
        elif id == ID_STOP:
            self.fg.stop()
        else:
            print "Click event not yet coded!"
            self.Close(True)

class MyApp(wxApp):
    def OnInit(self):
        frame = MyFrame(NULL, -1, "Digital WAve Player")
        frame.Show(true)
        self.SetTopWindow(frame)
        return true

app = MyApp(0)
app.MainLoop()
