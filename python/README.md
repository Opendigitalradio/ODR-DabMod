GUI and DPDCE
=============

This folder contains a web-based GUI and a DPD computation engine.
The Digital Predistortion Computation Engine and the web GUI can
run independently, and communicate through UDP socket.

ODR-DabMod Web UI
=================

Goals
-----

Enable users to play with digital predistortion settings, through a
visualisation of the settings and the parameters.

Make it easier to discover the tuning possibilities of the modulator.

The Web GUI presents a control interface that connects to ODR-DabMod and the
DPD computation engine. It is the main frontend for the DPDCE.

Prerequisites: python 3 with CherryPy, Jinja2, `python-zeromq`, `python-yaml`


Digital Predistortion Computation Engine for ODR-DabMod
-------------------------------------------------------

This folder contains a digital predistortion prototype.
It was only tested in a laboratory system, and is not ready
for production usage.

Prerequisites: python 3 with SciPy, Matplotlib, `python-zeromq`, `python-yaml`

Concept
-------

ODR-DabMod makes outgoing TX samples and feedback RX samples available to an
external tool. This external tool can request a buffer of samples for analysis,
can calculate coefficients for the predistorter in ODR-DabMod and load the new
coefficients using the remote control.

The external tool is called the Digital Predistortion Computation Engine (DPDCE).
The DPDCE is written in python, and makes use of the numpy library for
efficient computation. Its sources reside in the *dpd* folder.

The predistorter in ODR-DabMod supports two modes: polynomial and lookup table.
In the DPDCE, only the polynomial model is implemented at the moment.

- Sample transfer and time alignment with subsample accuracy is done by *Measure.py*
- Estimating the effects of the PA using some model and calculation of the updated
  polynomial coefficients is done in *Model.py* and other specific *Model_XXX.py* files
- Finally, *Adapt.py* updates the ODR-DabMod predistortion setting and digital gain

The DPDCE can be controlled through a UDP interface from the web GUI.

The *old/main.py* script was the entry point for the *DPD Computation Engine*
stand-alone prototype, used to develop the DPDCE, and is not functional anymore.


Requirements
------------

- USRP B200.
- Power amplifier.
- A feedback connection from the power amplifier output, such that the average power level at
  the USRP RX port is at -45dBm or lower.
  Usually this is done with a directional coupler and additional attenuators.
- ODR-DabMod with enabled *dpd_port*, and with a samplerate of 8192000 samples per second.
- Synchronous=1 so that the USRP has the timestamping set properly, internal refclk and pps
  are sufficient (not GPSDO necessary).
- A live mux source with TIST enabled.

See `dpd.ini` for an example.

The DPD server port can be tested with the *show_spectrum.py* helper tool, which can also display
a constellation diagram.

Hardware Setup
--------------

![setup diagram](dpd/img/setup_diagram.svg)
![setup photo](dpd/img/setup_photo.svg)

Our setup is depicted in the Figure above. We used components with the following properties:
 1. USRP TX (max +20dBm)
 2. Band III Filter (Mini-Circuits RBP-220W+, 190-250MHz, -3.5dB)
 3. Power amplifier (Mini-Circuits, max +15dBm output, +10 dB gain at 200MHz)
 4. Directional coupler (approx. -25dB @ 223MHz)
 5. Attenuator (-20 dB)
 6. Attenuator (-30 dB)
 7. USRP RX (max -15dBm input allowed, max -45dBm desired)
 8. Spectrum analyzer (max +30dBm allowed)

It is important to make sure that the USRP RX port does not receive too much
power. Otherwise the USRP will break. Here is an example of how we calculated
the maximal USRP RX input power for our case. As this is only a rough
calculation to protect the port, the predistortion software will later
automatically apply a normalization for the RX input by adapting the USRP RX
gain.

    TX Power + PA Gain - Coupling Factor - Attenuation = 20dBm + 10dB -25dB -50dB = -45dBm

Thus we have a margin of about 30dB for the input power of the USRP RX port.
Keep in mind we need to calculate using peak power, not average power, and it is
essential that there is no nonlinearity in the RX path!

Software Setup
--------------

We assume that you already installed *ODR-DabMux* and *ODR-DabMod*.
You should install the required python dependencies for the DPDCE using
distribution packages. You will need at least scipy, matplotlib and
python-zeromq.

Use the predistortion
----------------------

Make sure you have a ODR-DabMux running with a TCP output on port 9200.

Then run the modulator, with the example dpd configuration file.

```
./odr-dabmod dpd.ini
```

This configuration file is different from usual defaults in several respects:

 * logging to /tmp/dabmod.log
 * 4x oversampling: 8192000 sample rate
 * a very small digital gain, which will be overridden by the DPDCE
 * predistorter enabled
 * enables zmq rc

The TX gain should be chosen so that you can drive your amplifier into
saturation with a digital gain of 0.1, so that there is margin for the DPD to
operate.

You should *not modify txgain, rxgain, digital gain or coefficient settings in the dpd.ini file!*
When the DPDCE is used, it controls these settings, and there are command line
options for you to define initial values.

When plotting is enabled, it generates all available
visualisation plots in the newly created logging directory
`/tmp/dpd_<time_stamp>`. As the predistortion should increase the peak to
shoulder ratio, you should select a *txgain* in the ODR-DabMod configuration
file such that the initial peak-to-soulder ratio visible on your spectrum
analyser. This way, you will be able to see a the
change.

The DPDCE now does 10 iterations, and tries to improve the predistortion effectiveness.
In each step the learning rate is decreased. The learning rate is the factor
with which new coefficients are weighted in a weighted mean with the old
coefficients. Moreover the nuber of measurements increases in each iteration.
You find more information about that in *Heuristic.py*.

Each plot is stored to the logging directory under a filename containing its
time stamp and its label. Following plots are generated chronologically:

 - ExtractStatistic: Extracted information from one or multiple measurements.
 - Model\_AM: Fitted function for the amplitudes of the power amplifier against the TX amplitude.
 - Model\_PM: Fitted function for the phase difference of the power amplifier against the TX amplitude.
 - adapt.pkl: Contains all settings for the predistortion.
   You can load them again without doing measurements with the `apply_adapt_dumps.py` script.
 - MER: Constellation diagram used to calculate the modulation error rate.

After the run you should be able to observe that the peak-to-shoulder
difference has increased on your spectrum analyzer, similar to the figure below.

Without digital predistortion:

![shoulder_measurement_before](dpd/img/shoulder_measurement_before.png)

With digital predistortion, computed by the DPDCE:

![shoulder_measurement_after](dpd/img/shoulder_measurement_after.png)

Now see what happens if you apply the predistortions for different TX gains.

File format for coefficients
----------------------------
The coef file contains the polynomial coefficients used in the predistorter.
The file format is very similar to the filtertaps file used in the FIR filter.
It is a text-based format that can easily be inspected and edited in a text
editor.

The first line contains an integer that defines the predistorter to be used:
1 for polynomial, 2 for lookup table.

For the polynomial, the subsequent line contains the number of coefficients
as an integer. The second and third lines contain the real, respectively the
imaginary parts of the first coefficient. Fourth and fifth lines give the
second coefficient, and so on. The file therefore contains 1 + 1 + 2xN lines if
it contains N coefficients.

For the lookup table, the subsequent line contains a float scalefactor that is
applied to the samples in order to bring them into the range of 32-bit unsigned
integer. Then, the next pair of lines contains real and imaginary part of the first
lookup-table entry, which is multiplied to samples in first range. Then it's
followed by 31 other pairs. The entries are complex values close to 1 + 0j.
The file therefore contains 1 + 1 + 2xN lines if it contains N coefficients.

TODO
----

 - Understand and fix occasional ODR-DabMod crashes when using DPDCE.
 - Make the predistortion more robust. At the moment the shoulders sometimes
   increase instead of decrease after applying newly calculated predistortion
   parameters. Can this behaviour be predicted from the measurement? This would
   make it possible to filter out bad predistortion settings.
 - Find a better measurement for the quality of the predistortion. The USRP
   might not be good enough to measure large peak-to-shoulder ratios, because
   the ADC has 12 bits and DAB signals have a large crest factor.
 - Implement a Volterra polynomial to model the PA. Compared to the current
   model this would also capture the time dependent behaviour of the PA (memory
   effects).
 - Continuously observe DAB signal in frequency domain and make sure the power
   stays the same. At the moment only the power in the time domain is kept the
   same.
 - At the moment we assume that the USRP RX gain has to be larger than 30dB and
   the received signal should have a median absolute value of 0.05 in order to
   have a high quality quantization. Do measurements to support or improve
   this heuristic.
 - Check if we need to measure MER differently (average over more symbols?)
 - Is -45dBm the best RX feedback power level?

REFERENCES
----------

Some papers:

The paper Raich, Qian, Zhou, "Orthogonal Polynomials for Power Amplifier
Modeling and Predistorter Design" proposes other base polynomials that have
less numerical instability.

Aladrén, Garcia, Carro, de Mingo, and Sanchez-Perez, "Digital Predistortion
Based on Zernike Polynomial Functions for RF Nonlinear Power Amplifiers".

Jiang and Wilford, "Digital predistortion for power amplifiers using separable functions"

Changsoo Eun and Edward J. Powers, "A New Volterra Predistorter Based on the Indirect Learning Architecture"

Raviv Raich, Hua Qian, and G. Tong Zhou, "Orthogonal Polynomials for Power Amplifier Modeling and Predistorter Design"


Models without memory:

Complex polynomial: y[i] = a1 x[i] + a2 x[i]^2 + a3 x[i]^3 + ...

The complex polynomial corresponds to the input/output relationship that
applies to the PA in passband (real-valued signal). According to several
sources, this gets transformed to another representation if we consider complex
baseband instead. In the following, all variables are complex.

Odd-order baseband: y[i] = (b1 + b2 abs(x[i])^2 + b3 abs(x[i])^4) + ...) x[i]

Complete baseband: y[i] = (b1 + b2 abs(x[i]) + b3 abs(x[i])^2) + ...) x[i]

with
    b_k = 2^{1-k} \binom{k}{(k-1)/2} a_k


Models with memory:

 - Hammerstein model: Nonlinearity followed by LTI filter
 - Wiener model: LTI filter followed by NL
 - Parallel Wiener: input goes to N delays, each delay goes to a NL, all NL outputs summed.

Taken from slide 36 of [ECE218C Lecture 15](http://www.ece.ucsb.edu/Faculty/rodwell/Classes/ece218c/notes/Lecture15_Digital%20Predistortion_and_Future%20Challenges.pdf)


