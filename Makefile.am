# Copyright (C) 2007, 2008, 2009, 2010 Her Majesty the Queen in Right
# of Canada (Communications Research Center Canada)

# This file is part of CRC-DabMod.
# 
# CRC-DabMod is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
# 
# CRC-DabMod is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with CRC-DabMod.  If not, see <http://www.gnu.org/licenses/>.

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST	=COPYING NEWS README AUTHORS ChangeLog TODO doc lib/kiss_fft129.tar.gz \
            src/kiss_fftsimd.c src/kiss_fftsimd.h


if IS_GIT_REPO
GITVERSION_FLAGS = -DGITVERSION="\"-`git describe --long --tags`\""
else
GITVERSION_FLAGS = -DGITVERSION="\"-modified\""
endif

if HAVE_INPUT_ZEROMQ_TEST
ZMQ_LIBS    =-lzmq
else
ZMQ_LIBS    =
endif

FFT_INC = -Ilib/kiss_fft129 -Ilib/kiss_fft129/tools
FFT_SRC = lib/kiss_fft129/kiss_fft.c \
		  lib/kiss_fft129/tools/kiss_fftr.c \
		  src/kiss_fftsimd.c
FFT_FLG = -ffast-math

.PHONY: kiss_fft129 reed-solomon-4.0

bin_PROGRAMS =  crc-dabmod

src/DabModulator.cpp:   kiss_fft129

BUILT_SOURCES:  kiss_fft129

kiss_fft129:
	if [ ! -e lib/kiss_fft129/kiss_fft.c ]; then \
		tar xzf ./lib/kiss_fft129.tar.gz -C ./lib; \
	fi

crc_dabmod_CPPFLAGS = $(FFT_INC) $(FFT_FLG) -msse -msse2 $(GITVERSION_FLAGS)
crc_dabmod_LDADD   = $(ZMQ_LIBS)
crc_dabmod_SOURCES = \
		src/BlockPartitioner.cpp src/BlockPartitioner.h \
		src/Buffer.cpp src/Buffer.h \
		src/CicEqualizer.cpp src/CicEqualizer.h \
		src/ConvEncoder.cpp src/ConvEncoder.h \
		src/DabMod.cpp \
		src/DabModulator.cpp src/DabModulator.h \
		src/DifferentialModulator.cpp src/DifferentialModulator.h \
		src/Eti.cpp src/Eti.h \
		src/EtiReader.cpp src/EtiReader.h \
		src/FicSource.cpp src/FicSource.h \
		src/FIRFilter.cpp src/FIRFilter.h \
		src/Flowgraph.cpp src/Flowgraph.h \
		src/FrameMultiplexer.cpp src/FrameMultiplexer.h \
		src/FrequencyInterleaver.cpp src/FrequencyInterleaver.h \
		src/GainControl.cpp src/GainControl.h \
		src/GuardIntervalInserter.cpp src/GuardIntervalInserter.h \
		src/InputFileReader.cpp src/InputMemory.cpp \
		src/InputMemory.h src/InputReader.h src/InputZeroMQReader.cpp \
		src/kiss_fftsimd.c src/kiss_fftsimd.h \
		src/Log.cpp src/Log.h \
		src/ModCodec.cpp src/ModCodec.h \
		src/ModFormat.cpp src/ModFormat.h \
		src/ModInput.cpp src/ModInput.h \
		src/ModMux.cpp src/ModMux.h \
		src/ModOutput.cpp src/ModOutput.h \
		src/ModPlugin.cpp src/ModPlugin.h \
		src/NullSymbol.cpp src/NullSymbol.h \
		src/OfdmGenerator.cpp src/OfdmGenerator.h \
		src/OutputFile.cpp src/OutputFile.h \
		src/OutputMemory.cpp src/OutputMemory.h \
		src/OutputUHD.cpp src/OutputUHD.h \
		src/PcDebug.h \
		src/PhaseReference.cpp src/PhaseReference.h \
		src/porting.c src/porting.h \
		src/PrbsGenerator.cpp src/PrbsGenerator.h \
		src/PuncturingEncoder.cpp src/PuncturingEncoder.h \
		src/PuncturingRule.cpp src/PuncturingRule.h \
		src/QpskSymbolMapper.cpp src/QpskSymbolMapper.h \
		src/RemoteControl.cpp src/RemoteControl.h \
		src/Resampler.cpp src/Resampler.h \
		src/SignalMultiplexer.cpp src/SignalMultiplexer.h \
		src/SubchannelSource.cpp src/SubchannelSource.h \
		src/ThreadsafeQueue.h \
		src/TimeInterleaver.cpp src/TimeInterleaver.h \
		src/TimestampDecoder.cpp src/TimestampDecoder.h

nodist_crc_dabmod_SOURCES	=$(FFT_SRC)

dist_bin_SCRIPTS	= ./python/crc-dwap.py

clean-local:
	rm -rf lib/kiss_fft129
