# Copyright (C) 2007, 2008, 2009, 2010 Her Majesty the Queen in Right
# of Canada (Communications Research Center Canada)
#
#  Copyright (C) 2014, 2015
#  Matthias P. Braendli, matthias.braendli@mpb.li

#   http://opendigitalradio.org

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

EXTRA_DIST = COPYING NEWS README.md AUTHORS ChangeLog TODO doc

if IS_GIT_REPO
GITVERSION_FLAGS = -DGITVERSION="\"`git describe --dirty`\""
else
GITVERSION_FLAGS =
endif

bin_PROGRAMS = odr-dabmod

FFT_LDADD=

odr_dabmod_CPPFLAGS = -Wall -Isrc \
					  $(GITVERSION_FLAGS)
odr_dabmod_LDADD    = $(FFT_LDADD)
odr_dabmod_SOURCES  = src/DabMod.cpp \
					  src/PcDebug.h \
					  src/porting.c \
					  src/porting.h \
					  src/DabModulator.cpp \
					  src/DabModulator.h \
					  src/Buffer.cpp \
					  src/Buffer.h \
					  src/ModCodec.cpp \
					  src/ModCodec.h \
					  src/ModPlugin.cpp \
					  src/ModPlugin.h \
					  src/ModFormat.cpp \
					  src/ModFormat.h \
					  src/EtiReader.cpp \
					  src/EtiReader.h \
					  src/Eti.cpp \
					  src/Eti.h \
					  src/FicSource.cpp \
					  src/FicSource.h \
					  src/FIRFilter.cpp \
					  src/FIRFilter.h \
					  src/ModInput.cpp \
					  src/ModInput.h \
					  src/PuncturingRule.cpp \
					  src/PuncturingRule.h \
					  src/PuncturingEncoder.cpp \
					  src/PuncturingEncoder.h \
					  src/SubchannelSource.cpp \
					  src/SubchannelSource.h \
					  src/Flowgraph.cpp \
					  src/Flowgraph.h \
					  src/GainControl.cpp \
					  src/GainControl.h \
					  src/OutputMemory.cpp \
					  src/OutputMemory.h \
					  src/OutputZeroMQ.cpp \
					  src/OutputZeroMQ.h \
					  src/TimestampDecoder.h \
					  src/TimestampDecoder.cpp \
					  src/OutputUHD.cpp \
					  src/OutputUHD.h \
					  src/ModOutput.cpp \
					  src/ModOutput.h \
					  src/InputMemory.cpp \
					  src/InputMemory.h \
					  src/InputFileReader.cpp \
					  src/InputZeroMQReader.cpp \
					  src/InputReader.h \
					  src/OutputFile.cpp \
					  src/OutputFile.h \
					  src/FrameMultiplexer.cpp \
					  src/FrameMultiplexer.h \
					  src/ModMux.cpp \
					  src/ModMux.h \
					  src/PrbsGenerator.cpp \
					  src/PrbsGenerator.h \
					  src/BlockPartitioner.cpp \
					  src/BlockPartitioner.h \
					  src/QpskSymbolMapper.cpp \
					  src/QpskSymbolMapper.h \
					  src/FrequencyInterleaver.cpp \
					  src/FrequencyInterleaver.h \
					  src/PhaseReference.cpp \
					  src/PhaseReference.h \
					  src/DifferentialModulator.cpp \
					  src/DifferentialModulator.h \
					  src/NullSymbol.cpp \
					  src/NullSymbol.h \
					  src/SignalMultiplexer.cpp \
					  src/SignalMultiplexer.h \
					  src/CicEqualizer.cpp \
					  src/CicEqualizer.h \
					  src/OfdmGenerator.cpp \
					  src/OfdmGenerator.h \
					  src/GuardIntervalInserter.cpp \
					  src/GuardIntervalInserter.h \
					  src/Resampler.cpp \
					  src/Resampler.h \
					  src/ConvEncoder.cpp \
					  src/ConvEncoder.h \
					  src/TimeInterleaver.cpp \
					  src/TimeInterleaver.h \
					  src/ThreadsafeQueue.h \
					  src/Log.cpp \
					  src/Log.h \
					  src/RemoteControl.cpp \
					  src/RemoteControl.h \
					  src/FormatConverter.cpp \
					  src/FormatConverter.h \
					  src/Utils.cpp \
					  src/Utils.h \
					  src/TII.cpp \
					  src/TII.h \
					  src/zmq.hpp

dist_bin_SCRIPTS = src/crc-dwap.py

