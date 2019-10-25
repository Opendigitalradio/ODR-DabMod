OVERVIEW
========
ODR-DabMod is a *DAB (Digital Audio Broadcasting)* modulator compliant
to ETSI EN 300 401. It is the continuation of the work started by
the Communications Research Center Canada, and is now pursued in the
[Opendigitalradio project](http://opendigitalradio.org).


ODR-DabMod is part of the ODR-mmbTools tool-set. More information about the
ODR-mmbTools is available in the *guide*, available on the
[Opendigitalradio mmbTools page](http://www.opendigitalradio.org/mmbtools).

Features
--------

- Reads ETI and EDI, outputs compliant COFDM I/Q
- Supports native DAB sample rate and can also resample to other rates
- Supports all four DAB transmission modes
- Configuration file support, see `doc/example.ini`
- Integrated UHD output for [USRP devices](https://www.ettus.com/product)
  - Tested for B200, B100, USRP2, USRP1
  - With WBX daughterboard (where appropriate)
- [SoapySDR](https://github.com/pothosware/SoapySDR/wiki) output
  - Can be used to drive the [LimeSDR board](https://myriadrf.org/projects/limesdr/), the [HackRF](https://greatscottgadgets.com/hackrf/) and others.
- Timestamping support required for SFN
- GPSDO monitoring (both Ettus and [ODR LEA-M8F board](http://www.opendigitalradio.org/lea-m8f-gpsdo))
- Monitoring integration with munin
- A FIR filter for improved spectrum mask
- TII insertion
- Logging: log to file, to syslog
- ETI sources: ETI-over-TCP, file (Raw, Framed and Streamed) and ZeroMQ
- A Telnet and ZeroMQ remote-control that can be used to change
  some parameters during runtime and retrieve statistics.
  See `doc/README-RC.md` for more information
- ZeroMQ PUB and REP output.
- Ongoing work about digital predistortion for PA linearisation.
  See `python/dpd/README.md`
- A web GUI for control and supervision of modulator and predistortion engine. See `python/gui/README.md`
- A prototype algorithm for crest factor reduction.

The `src/` directory contains the source code of ODR-DabMod.

The `doc/` directory contains the ODR-DabMod documentation, an example
configuration file and a script for munin integration.

The `lib/` directory contains source code of libraries needed to build
ODR-DabMod.

The `python/` directory contains a web-based graphical control interface and
the digital predistortion project.

INSTALL
=======
See the `INSTALL.md` file for installation instructions.

LICENCE
=======
See the files `LICENCE` and `COPYING`

CONTACT
=======
Matthias P. Braendli *matthias [at] mpb [dot] li*

Pascal Charest *pascal [dot] charest [at] crc [dot] ca*

With thanks to other contributors listed in AUTHORS

http://opendigitalradio.org/
