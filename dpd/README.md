Digital Predistortion for ODR-DabMod
====================================

This folder contains work in progress for digital predistortion. It requires:

- USRP B200.
- Power amplifier.
- A feedback connection from the power amplifier output, at an appropriate power level for the B200.
  Usually this is done with a directional coupler.
- ODR-DabMod with enabled dpd_port, and with a samplerate of 8192000 samples per second.
- Synchronous=1 so that the USRP has the timestamping set properly, internal refclk and pps
  are sufficient for this example.
- A live mux source with TIST enabled.

See dpd/dpd.ini for an example.

TODO
----

Implement a PA model that updates the predistorter.
