#!/usr/bin/env python2
#
# present statistics from ODR-DabMod's
# RC interface to munin. Expects ZeroMQ on port
# 9400.
#
# Copy this file to /etc/munin/plugins/dabmod
# to use it, and make sure it's executable (chmod +x)

import sys
import json
import zmq
import os
import re

# Values monitored:

config_all = ""

#default data type is GAUGE

# One GAUGE multigraph from 0% to 100% with
#   ofdm clip_stats clip_ratio
#   ofdm clip_stats errorclip_ratio
config_all += """
multigraph ofdm_clip_stats
graph_title OFDM CFR clip stats
graph_order clip_ratio errorclip_ratio
graph_vlabel number of samples/errors clipped during last ${{graph_period}}
graph_category dabmod
graph_info This graph shows CFR clipping statistics

clip_ratio.info Number of samples clipped
clip_ratio.label Number of samples clipped
clip_ratio.min 0
clip_ratio.max 100
errorclip_ratio.info Number of errors clipped
errorclip_ratio.label Number of errors clipped
errorclip_ratio.min 0
errorclip_ratio.max 100"""

# One GAUGE multigraph
#   ofdm clip_stats mer
config_all += """
multigraph ofdm_clip_stats_mer
graph_title OFDM MER after CFR
graph_order mer
graph_vlabel MER in dB after CFR
graph_category dabmod
graph_info This graph shows MER after CFR

mer.info MER dB
mer.label MER dB
mer.min 0
mer.max 100"""

# One GAUGE multigraph in dB for
#   ofdm papr before-cfr
#   ofdm papr after-cfr
config_all += """
multigraph ofdm_papr
graph_title OFDM PAPR stats
graph_order before_cfr after_cfr
graph_args --base 1000
graph_vlabel Averate PAPR before/after CFR during last ${{graph_period}}
graph_category dabmod
graph_info This graph shows the Peak-to-Average Power Ratio before and after CFR

before_cfr.info PAPR before CFR
before_cfr.label PAPR before CFR
before_cfr.min 0
after_cfr.info PAPR after CFR
after_cfr.label PAPR after CFR
after_cfr.min 0"""

# One GAUGE graph for
#   tist offset
config_all += """
multigraph tist_offset
graph_title TIST configured offset
graph_order offset
graph_args --base 1000
graph_vlabel Configured offset
graph_category dabmod
graph_info This graph shows the configured TIST offset

offset.info Configured offset
offset.label Configured offset
offset.min 0
offset.max 300"""

# One DDERIVE graph for
#   tist timestamp timestamps
config_all += """
multigraph tist_timestamp
graph_title TIST timestamp
graph_order timestamp
graph_args --base 1000
graph_vlabel timestamp value
graph_category dabmod
graph_info This graph shows the timestamp value in seconds

timestamp.info timestamp
timestamp.label timestamp
timestamp.type DDERIVE
timestamp.min 0"""

# One DERIVE (min 0) multigraph for
#   sdr underruns
#   sdr latepackets
config_all += """
multigraph sdr_stats
graph_title SDR device statistics
graph_order underruns latepackets
graph_args --base 1000
graph_vlabel Number of underruns and late packets
graph_category dabmod
graph_info This graph shows the number of underruns and late packets

underruns.info Number of SoapySDR/UHD underruns
underruns.label Number of SoapySDR/UHD underruns
underruns.type DERIVE
underruns.min 0
latepackets.info Number of SoapySDR/UHD late packets
latepackets.label Number of SoapySDR/UHD late packets
latepackets.type DERIVE
latepackets.min 0"""

# One DERIVE (min 0) graph for
#   sdr frames
config_all += """
multigraph sdr_frames
graph_title SDR number of frames transmitted
graph_order frames
graph_args --base 1000
graph_vlabel Number of frames transmitted
graph_category dabmod
graph_info This graph shows the number of frames transmitted

frames.info Number of SoapySDR/UHD frames
frames.label Number of SoapySDR/UHD frames
frames.type DERIVE
frames.min 0"""

# One GAUGE multigraph
#   sdr gpsdo_num_sv
# and one for device sensors
#   sdr temp
config_all += """
multigraph sdr_gpsdo_sv_holdover
graph_title Number of GNSS SVs used and holdover state
graph_order num_sv holdover
graph_vlabel Number of GNSS SVs and holdover state
graph_category dabmod
graph_info This graph shows the number of Satellite Vehicles the GNSS receiver uses (Field 7 of GNGGA NMEA sentence), and if it is in holdover

num_sv.info Num SVs
num_sv.label Num SVs
num_sv.min 0
num_sv.max 20

holdover.info Holdover
holdover.label Holdover
holdover.min 0
holdover.max 1

multigraph sdr_sensors
graph_title SDR Sensors
graph_order temp
graph_vlabel SDR Sensors
graph_category dabmod
graph_info This graph shows the device temperature in Celsius

temp.info Device temperature in Celsius
temp.label Celsius
temp.min 0
temp.max 100"""

ctx = zmq.Context()

class RCException(Exception):
    pass

def do_transaction(message_parts, sock):
    """To a send + receive transaction, quit whole program on timeout"""
    if isinstance(message_parts, str):
        sys.stderr.write("do_transaction expects a list!\n");
        sys.exit(1)

    for i, part in enumerate(message_parts):
        if i == len(message_parts) - 1:
            f = 0
        else:
            f = zmq.SNDMORE
        sock.send(part, flags=f)

    poller = zmq.Poller()
    poller.register(sock, zmq.POLLIN)

    socks = dict(poller.poll(1000))
    if socks:
        if socks.get(sock) == zmq.POLLIN:
            rxpackets = sock.recv_multipart()
            return rxpackets

    raise RCException("Could not receive data for command '{}'\n".format(
        message_parts))

def connect():
    """Create a connection to the dabmod RC

    returns: the socket"""

    sock = zmq.Socket(ctx, zmq.REQ)
    sock.set(zmq.LINGER, 5)
    sock.connect("tcp://localhost:9400")

    try:
        ping_answer = do_transaction([b"ping"], sock)

        if not ping_answer == [b"ok"]:
            sys.stderr.write("Wrong answer to ping\n")
            sys.exit(1)
    except RCException as e:
        print("connect failed because: {}".format(e))
        sys.exit(1)

    return sock

def get_rc_value(module, name, sock):
    try:
        parts = do_transaction([b"get", module.encode(), name.encode()], sock)
        if len(parts) != 1:
            sys.stderr.write("Received unexpected multipart message {}\n".format(
                parts))
            sys.exit(1)
        return parts[0].decode()
    except RCException as e:
        print("get {} {} fail: {}".format(module, name, e))
        return ""

def handle_re(graph_name, re, rc_value, group_number=1):
    match = re.search(rc_value)
    if match:
        return "{}.value {}\n".format(graph_name, match.group(group_number))
    else:
        return "{}.value U\n".format(graph_name)

re_double_value = re.compile(r"(\d+\.\d+)", re.X)
re_int_value = re.compile(r"(\d+)", re.X)

if len(sys.argv) == 1:
    sock = connect()

    munin_values = ""

    munin_values += "multigraph ofdm_clip_stats\n"
    ofdm_clip_stats = get_rc_value("ofdm", "clip_stats", sock)
    re_clip_samples = re.compile(r"(\d+\.\d+)%\ samples\ clipped", re.X)
    munin_values += handle_re("clip_ratio", re_clip_samples, ofdm_clip_stats)

    re_clip_errors = re.compile(r"(\d+\.\d+)%\ errors\ clipped", re.X)
    munin_values += handle_re("errorclip_ratio",
            re_clip_errors, ofdm_clip_stats)

    munin_values += "multigraph ofdm_clip_stats_mer\n"
    re_clip_mer = re.compile(r"MER\ after\ CFR:\ (\d+\.\d+)", re.X)
    munin_values += handle_re("mer",
            re_clip_mer, ofdm_clip_stats)

    munin_values += "multigraph ofdm_papr\n"
    ofdm_papr_stats = get_rc_value("ofdm", "papr", sock)

    def muninise_papr(papr):
        if "N/A" in papr:
            return "U"
        else:
            return float(papr.strip())

    # Format is as follows:
    # "PAPR [dB]: " << std::fixed <<
    #   (papr_before == 0 ? string("N/A") : to_string(papr_before)) <<
    #   ", " <<
    #   (papr_after == 0 ? string("N/A") : to_string(papr_after));
    try:
        _, _, both_papr = ofdm_papr_stats.partition(":")
        papr_before, papr_after = both_papr.split(",")
        papr_before = muninise_papr(papr_before)
        munin_values += "before_cfr.value {}\n".format(papr_before)
    except:
        munin_values += "before_cfr.value U\n"

    try:
        _, _, both_papr = ofdm_papr_stats.partition(":")
        papr_before, papr_after = both_papr.split(",")
        papr_after = muninise_papr(papr_after)
        munin_values += "after_cfr.value {}\n".format(papr_after)
    except:
        munin_values += "after_cfr.value U\n"


    munin_values += "multigraph tist_offset\n"
    tist_offset = get_rc_value("tist", "offset", sock)
    munin_values += handle_re("offset", re_double_value, tist_offset)

    # Plotting FCT is not useful because it overflows in 6s, and the poll
    # interval is usually 5min

    tist_timestamp = get_rc_value("tist", "timestamp", sock)
    re_tist_timestamp = re.compile(r"(\d+\.\d+)\ for\ frame\ FCT\ (\d+)", re.X)
    munin_values += "multigraph tist_timestamp\n"
    munin_values += handle_re("timestamp", re_tist_timestamp, tist_timestamp, 1)

    munin_values += "multigraph sdr_stats\n"
    sdr_underruns = get_rc_value("sdr", "underruns", sock)
    munin_values += handle_re("underruns", re_int_value, sdr_underruns)
    sdr_latepackets = get_rc_value("sdr", "latepackets", sock)
    munin_values += handle_re("latepackets", re_int_value, sdr_latepackets)

    munin_values += "multigraph sdr_gpsdo_sv_holdover\n"
    try:
        gps_num_sv = get_rc_value("sdr", "gpsdo_num_sv", sock)
        munin_values += "num_sv.value {}\n".format(gps_num_sv)
    except:
        munin_values += "num_sv.value U\n"

    try:
        gps_holdover = get_rc_value("sdr", "gpsdo_holdover", sock)
        munin_values += "holdover.value {}\n".format(gps_holdover)
    except:
        munin_values += "holdover.value U\n"

    munin_values += "multigraph sdr_sensors\n"
    try:
        sdr_temp = get_rc_value("sdr", "temp", sock)
        munin_values += "temp.value {}\n".format(sdr_temp)
    except:
        munin_values += "temp.value U\n"

    munin_values += "multigraph sdr_frames\n"
    sdr_frames = get_rc_value("sdr", "frames", sock)
    munin_values += handle_re("frames", re_int_value, sdr_frames)

    print(munin_values)

elif len(sys.argv) == 2 and sys.argv[1] == "config":
    # No need to connect
    print(config_all)
else:
    sys.stderr.write("Invalid command line arguments")
    sys.exit(1)

