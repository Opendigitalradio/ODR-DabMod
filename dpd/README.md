Digital Predistortion Calculation Engine for ODR-DabMod
=======================================================

This folder contains work in progress for digital predistortion.

Concept
-------

ODR-DabMod makes outgoing TX samples and feedback RX samples available for an external tool. This
external tool can request a buffer of samples for analysis, can calculate coefficients for the
polynomial predistorter in ODR-DabMod and load the new coefficients using the remote control.

The *dpd/main.py* script is the entry point for the *DPD Calculation Engine* into which these
features will be implemented. The tool uses modules from the *dpd/src/* folder:

- Sample transfer and time alignment with subsample accuracy is done by *Measure.py*
- Estimating the effects of the PA using some model and calculation of the updated
  polynomial coefficients is done in *Model.py*
- Finally, *Adapt.py* loads them into ODR-DabMod.

These modules themselves use additional helper scripts in the *dpd/src/* folder.

Requirements
------------

- USRP B200.
- Power amplifier.
- A feedback connection from the power amplifier output, at an appropriate power level for the B200.
  Usually this is done with a directional coupler and additional attenuators.
- ODR-DabMod with enabled *dpd_port*, and with a samplerate of 8192000 samples per second.
- Synchronous=1 so that the USRP has the timestamping set properly, internal refclk and pps
  are sufficient for this example.
- A live mux source with TIST enabled.

See dpd/dpd.ini for an example.

The DPD server port can be tested with the *dpd/show_spectrum.py* helper tool, which can also display
a constellation diagram.

File format for coefficients
----------------------------
The coef file contains the polynomial coefficients used in the predistorter. The file format is
very similar to the filtertaps file used in the FIR filter. It is a text-based format that can
easily be inspected and edited in a text editor.

The first line contains the number of coefficients as an integer. The second and third lines contain
the real, respectively the imaginary parts of the first coefficient. Fourth and fifth lines give the
second coefficient, and so on. The file therefore contains 2xN + 1 lines if it contains N
coefficients.

TODO
----

Implement a PA model.
Implement cases for different oversampling for FFT bin choice.
Fix loads of missing and buggy aspects of the implementation.
