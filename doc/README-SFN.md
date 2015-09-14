On the Usage of ODR-DabMod for Synchronous Transmissions
========================================================

Summary
-------
ODR-DabMux and ODR-DabMod offer support for timestamped transmission
when the UHD output is used. This README explains how this functionality
works, and how to set it up.

This feature is a prerequisite for the creation of a single-frequency
network.


Concept
-------
The goal of this functionality is to synchronise the transmission for
several transmitters. This has been tested with the USRP B100, B200 and the
USRP2, that both have the necessary REFCLK and 1PPS inputs. Both are
required to synchronise two USRPs:
- The REFCLK is used in the USRP for timekeeping. If we want two
  USRPs to stay synchronised, they both must have a precise 10MHz
  source at the REFCLK, otherwise their internal clocks will drift
  off.
- The 1PPS signal is used to set the time inside the USRPs. The rising
  edge of the 1PPS signal has happen synchronously for all transmitters.
  Usually, GPS is used to drive this 1PPS.

For such a system, there will be one multiplexer, which will send the ETI
stream to several modulators. The ETI stream, in this case, is transported
over the ZMQ interconnection.

Each modulator receives ETI frames that contain absolute timestamps, defining
the exact point in time when the frame has to be transmitted. These in-band
timestamps are composed of two parts:
- The TIST field as defined in the ETI standard, giving an offset after the
  pulse per second signal;
- A time information transmitted using the MNSC, representing the precise time
  when the frame must be transmitted, with one-second resolution.

When ODR-DabMux is configured accordingly, the TIST is defined in each frame.
The time is always encoded in the MNSC.

When the ETI stream is sent to several modulators using non-blocking I/O, it
is not possible to rely on a modulator to back-pressure the Ensemble multiplexer.
It is therefore necessary to throttle multiplexer output.

Each modulator then receives the ETI stream through a ZMQ connection. Each frame
contains the complete timestamp, to which an per-modulator offset is added.
The sum is then given to the USRP. The offset can be specified in the
configuration file of ODR-DabMod, and can be modified on the fly using
the remote control interface.

ODR-DabMod uses the UHD library to output modulated samples to the USRP device.
When started, it defines the USRP time using the local time and the PPS signal.
It is therefore important to synchronise computer time using NTP.

When a frame arrives with a timestamp that is in the past, the frame is dropped.
If the timestamp is too far in the future, the output module waits a short
delay.

Synchonisation can be verified by using an oscilloscope and a receiver. It is
very easy to see if the null symbols align. Then tune the receiver to the
ensemble, and alternatively lower the tx gain of each modulator to see if the
receiver is still able to receive the ensemble without hiccup.

Time and frequency references
-----------------------------
In addition to the 10MHz refclk and 1PPS inputs on the USRP, some USRPs also
support an integrated GPSDO. For the B200, there are two GPSDOs modules that
can be used: The Ettus GPSDO (Jackson Labs Firefly), and the u-blox LEA-M8F on
the [Opendigitalradio board](http://www.opendigitalradio.org/lea-m8f-gpsdo).

To use the LEA-M8F, some modifications in the UHD library are necessary, because
the module outputs a 30.72MHz refclk instead of a 10MHz. The changes are
available in the [ODR repository of UHD](https://github.com/Opendigitalradio/uhd),
with branch names called *lea-m8f-UHDVERSION*.

When using the integrated GPSDO, ODR-DabMod will also monitor if the GPS
reception is ok, and if the time reference is usable.

Hardware requirements
---------------------
The following hardware is required to build a SFN with the ODR-mmbTools:
- Two USRPs ;
- One or two computers with the mmbTools installed ;
- A network connection between the two computers ;
- A 10MHz refclk source ;
- A 1PPS source synchronised to the 10MHz ;
- An oscilloscope to check synchronisation.

It is possible to use signal generators as REFCLK source and 1PPS, if there is
no GPS-disciplined oscillator available. It is necessary to synchronise the
1PPS source to the 10MHz source.


###########
june 2012, initial version, Matthias P. Braendli
feb 2014, renamed crc-dabXYZ to odr-dabXYZ, mpb
sep 2015, overhaul, talk about GPSDOs, mpb

