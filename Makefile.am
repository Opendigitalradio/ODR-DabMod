# Copyright (C) 2007, 2008, 2009, 2010 Her Majesty the Queen in Right
# of Canada (Communications Research Center Canada)
#
#  Copyright (C) 2018
#  Matthias P. Braendli, matthias.braendli@mpb.li
#
#   http://opendigitalradio.org
#
# This file is part of ODR-DabMod.
#
# ODR-DabMod is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# ODR-DabMod is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with ODR-DabMod.  If not, see <http://www.gnu.org/licenses/>.

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = COPYING NEWS README.md INSTALL.md AUTHORS ChangeLog TODO doc \
			 lib/fec/README.md lib/fec/LICENSE \
			 lib/edi/README.md

if IS_GIT_REPO
GITVERSION_FLAGS = -DGITVERSION="\"`git describe --dirty`\""
else
GITVERSION_FLAGS =
endif

bin_PROGRAMS = odr-dabmod

odr_dabmod_CFLAGS   = -Wall -Isrc -Ilib \
					  $(GITVERSION_FLAGS)
odr_dabmod_CXXFLAGS = -Wall -Isrc -Ilib -std=c++11 \
					  $(GITVERSION_FLAGS) $(BOOST_CPPFLAGS)
odr_dabmod_LDADD    =  $(BOOST_LDFLAGS) $(BOOST_THREAD_LIB)
odr_dabmod_SOURCES  = src/DabMod.cpp \
					  src/PcDebug.h \
					  src/DabModulator.cpp \
					  src/DabModulator.h \
					  src/Buffer.cpp \
					  src/Buffer.h \
					  src/ConfigParser.cpp \
					  src/ConfigParser.h \
					  src/ModPlugin.cpp \
					  src/ModPlugin.h \
					  src/EtiReader.cpp \
					  src/EtiReader.h \
					  src/Eti.cpp \
					  src/Eti.h \
					  src/FicSource.cpp \
					  src/FicSource.h \
					  src/PuncturingRule.cpp \
					  src/PuncturingRule.h \
					  src/PuncturingEncoder.cpp \
					  src/PuncturingEncoder.h \
					  src/SubchannelSource.cpp \
					  src/SubchannelSource.h \
					  src/Flowgraph.cpp \
					  src/Flowgraph.h \
					  src/OutputMemory.cpp \
					  src/OutputMemory.h \
					  src/OutputZeroMQ.cpp \
					  src/OutputZeroMQ.h \
					  src/TimestampDecoder.h \
					  src/TimestampDecoder.cpp \
					  src/InputFileReader.cpp \
					  src/InputMemory.cpp \
					  src/InputMemory.h \
					  src/InputReader.h \
					  src/InputTcpReader.cpp \
					  src/InputZeroMQReader.cpp \
					  src/OutputFile.cpp \
					  src/OutputFile.h \
					  src/FrameMultiplexer.cpp \
					  src/FrameMultiplexer.h \
					  src/PrbsGenerator.cpp \
					  src/PrbsGenerator.h \
					  src/BlockPartitioner.cpp \
					  src/BlockPartitioner.h \
					  src/SignalMultiplexer.cpp \
					  src/SignalMultiplexer.h \
					  src/ConvEncoder.cpp \
					  src/ConvEncoder.h \
					  src/TimeInterleaver.cpp \
					  src/TimeInterleaver.h \
					  src/FormatConverter.cpp \
					  src/FormatConverter.h \
					  src/Utils.cpp \
					  src/Utils.h \
					  src/zmq.hpp \
					  lib/RemoteControl.cpp \
					  lib/RemoteControl.h \
					  lib/Log.cpp \
					  lib/Log.h \
					  lib/Globals.cpp \
					  lib/INIReader.h \
					  lib/crc.h \
					  lib/crc.c \
					  lib/Socket.h \
					  lib/Socket.cpp \
					  lib/ThreadsafeQueue.h \
					  lib/fec/char.h \
					  lib/fec/decode_rs_char.c \
					  lib/fec/decode_rs.h \
					  lib/fec/encode_rs_char.c \
					  lib/fec/encode_rs.h \
					  lib/fec/fec.h \
					  lib/fec/init_rs_char.c \
					  lib/fec/init_rs.h \
					  lib/fec/rs-common.h \
					  lib/edi/buffer_unpack.hpp \
					  lib/edi/common.hpp \
					  lib/edi/common.cpp \
					  lib/edi/eti.hpp \
					  lib/edi/eti.cpp \
					  lib/edi/ETIDecoder.hpp \
					  lib/edi/ETIDecoder.cpp \
					  lib/edi/PFT.hpp \
					  lib/edi/PFT.cpp

if !COMPILE_FOR_EASYDABV3
odr_dabmod_SOURCES += \
					  src/FIRFilter.cpp \
					  src/FIRFilter.h \
					  src/MemlessPoly.cpp \
					  src/MemlessPoly.h \
					  src/GainControl.cpp \
					  src/GainControl.h \
					  src/output/Feedback.cpp \
					  src/output/Feedback.h \
					  src/output/SDR.cpp \
					  src/output/SDR.h \
					  src/output/SDRDevice.h \
					  src/output/Soapy.cpp \
					  src/output/Soapy.h \
					  src/output/UHD.cpp \
					  src/output/UHD.h \
					  src/output/USRPTime.cpp \
					  src/output/USRPTime.h \
					  src/output/Lime.cpp \
					  src/output/Lime.h \
					  src/PhaseReference.cpp \
					  src/PhaseReference.h \
					  src/QpskSymbolMapper.cpp \
					  src/QpskSymbolMapper.h \
					  src/FrequencyInterleaver.cpp \
					  src/FrequencyInterleaver.h \
					  src/DifferentialModulator.cpp \
					  src/DifferentialModulator.h \
					  src/NullSymbol.cpp \
					  src/NullSymbol.h \
					  src/CicEqualizer.cpp \
					  src/CicEqualizer.h \
					  src/OfdmGenerator.cpp \
					  src/OfdmGenerator.h \
					  src/GuardIntervalInserter.cpp \
					  src/GuardIntervalInserter.h \
					  src/Resampler.cpp \
					  src/Resampler.h \
					  src/PAPRStats.cpp \
					  src/PAPRStats.h \
					  src/TII.cpp \
					  src/TII.h

odr_dabmod_LDADD   += $(UHD_LIBS) $(LIMESDR_LIBS) $(ADDITIONAL_UHD_LIBS)
endif

