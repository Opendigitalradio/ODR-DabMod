Remote Control Interface
========================

The RC interface allows you to change settings at runtime and to access some
statistics. Two interfaces are available: Telnet and based on ZeroMQ.

The Telnet interface is designed for human interaction. Once you have enabled
the interface and set the port, use any telnet client to connect to the server
to get the RC command line interface. Since this is totally unsecure telnet,
the software will only listen on the local loopback interface. To get secure
remote access, use SSH port forwarding.

The ZeroMQ interface is designed for machine interaction, e.g. for usage in
scripts or from third party tools. The Munin monitoring is also using this
interface, please see `doc/stats_dabmod_munin.py`.
An example python script to connect to that
interface is available in `doc/zmq-ctrl/zmq_remote.py`,
and example C++ code is available in `doc/zmq-ctrl/cpp/`.

Both interfaces may be enabled simultaneously.

Statistics available
--------------------

The following statistics are presented through the RC:

 * Value of TIST in `tist timestamp`
 * UHD: number of underruns, overruns and frames transmitted
 * SoapySDR: number of underruns and overruns
 * OFDM Generator: CFR stats and MER after CFR (if CFR enabled) in `ofdm clip_stats`
 * OFDM Generator: PAPR before and after CFR in `ofdm papr`

More statistics are likely to be added in the future, and we are always open
for suggestions.


ZMQ RC Protocol
---------------

ODR-DabMod binds a zmq rep socket so clients must connect
using either req or dealer socket.
[] denotes message part as zmq multi-part message are used for delimitation.
All message parts are utf-8 encoded strings and match the Telnet command set.
Messages to be sent as literal strings are denoted with "" below.

The following commands are supported:

    REQ: ["ping"]
    REP: ["ok"]

    REQ: ["list"]
    REP: ["ok"][module name][module name]...

    REQ: ["show"][module name]
    REP: ["ok"][parameter: value][parameter: value]...

    REQ: ["get"][module name][parameter]
    REP: [value] _OR_ ["fail"][error description]

    REQ: ["set"][module name][parameter][value]
    REP: ["ok"] _OR_ ["fail"][error description]
