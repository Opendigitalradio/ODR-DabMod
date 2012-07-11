/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)
 */
/*
   This file is part of CRC-DADMOD.

   CRC-DADMOD is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   CRC-DADMOD is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with CRC-DADMOD.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include "porting.h"

#include "DabModulator.h"
#include "InputMemory.h"
#include "OutputFile.h"
#include "PcDebug.h"

#include <complex>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdexcept>
#include <signal.h>
#include <string.h>

#ifdef HAVE_NETINET_IN_H
#   include <netinet/in.h>
#endif

#ifdef HAVE_DECL__MM_MALLOC
#   include <mm_malloc.h>
#else
#   define memalign(a, b)   malloc(b)
#endif


typedef std::complex<float> complexf;


bool running = true;

void signalHandler(int signalNb)
{
    PDEBUG("signalHandler(%i)\n", signalNb);

    running = false;
}


void printUsage(char* progName, FILE* out = stderr)
{
    fprintf(out, "Usage:\n");
    fprintf(out, "\t%s"
            " [input [output]]"
            " [-a amplitude]"
            " [-c clockrate]"
            " [-f]"
            " [-g gainMode]"
            " [-h]"
            " [-l]"
            " [-m dabMode]"
            " [-r samplingRate]"
            "\n", progName);
    fprintf(out, "Where:\n");
    fprintf(out, "input:    ETI input filename (default: stdin).\n");
    fprintf(out, "output:   COFDM output filename (default: stdout).\n");
    fprintf(out, "-a:       Apply amplitude gain.\n");
    fprintf(out, "-c:       Set the DAC clock rate.\n");
    fprintf(out, "-f:       (deprecated) Set fifo input.\n");
    fprintf(out, "-g:       Set computation gain mode: "
            "%u FIX, %u MAX, %u VAR\n", GAIN_FIX, GAIN_MAX, GAIN_VAR);
    fprintf(out, "-h:       Print this help.\n");
    fprintf(out, "-l:       Loop file when reach end of file.\n");
    fprintf(out, "-m:       Set DAB mode: (0: auto, 1-4: force).\n");
    fprintf(out, "-r:       Set output sampling rate (default: 2048000).\n");
}


void printVersion(FILE *out = stderr)
{
    fprintf(out, "Welcome to %s %s, compiled at %s, %s\n\n",
            PACKAGE, VERSION, __DATE__, __TIME__);
    fprintf(out, "CRC-DABMOD is copyright (C) Her Majesty the Queen in Right of Canada,\n"
            "    2009, 2010, 2011, 2012 Communications Research Centre (CRC).\n"
            "\n"
            "    This program is available free of charge and is licensed to you on a\n"
            "    non-exclusive basis; you may not redistribute it.\n"
            "\n"
            "    This program is provided \"AS IS\" in the hope that it will be useful, but\n"
            "    WITHOUT ANY WARRANTY with respect to its accurancy or usefulness; witout\n"
            "    even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR\n"
            "    PURPOSE and NONINFRINGEMENT.\n"
            "\n"
            "    In no event shall CRC be LIABLE for any LOSS, DAMAGE or COST that may be\n"
            "    incurred in connection with the use of this software.\n"
            "\n"
            "CRC-DABMOD makes use of the following open source packages:\n"
            "    Kiss FFT v1.2.9 (Revised BSD) - http://kissfft.sourceforge.net/\n"
           );

}


int main(int argc, char* argv[])
{
    int ret = 0;
    bool loop = false;
    char* inputName;
    char* outputName;
    FILE* inputFile = NULL;
    uint32_t sync = 0;
    uint32_t nbFrames = 0;
    uint32_t frame = 0;
    uint16_t frameSize = 0;
    size_t outputRate = 2048000;
    size_t clockRate = 0;
    unsigned dabMode = 0;
    float amplitude = 1.0f;
    GainMode gainMode = GAIN_VAR;
    Buffer data;

    Flowgraph* flowgraph = NULL;
    DabModulator* modulator = NULL;
    InputMemory* input = NULL;
    OutputFile* output = NULL;

    signal(SIGINT, signalHandler);

    while (true) {
        int c = getopt(argc, argv, "a:c:fg:hlm:r:V");
        if (c == -1) {
            break;
        }
        switch (c) {
        case 'a':
            amplitude = strtof(optarg, NULL);
            break;
        case 'c':
            clockRate = strtol(optarg, NULL, 0);
            break;
        case 'f':
            fprintf(stderr, "Option -f deprecated!\n");
            break;
        case 'g':
            gainMode = (GainMode)strtol(optarg, NULL, 0);
            break;
        case 'l':
            loop = true;
            break;
        case 'm':
            dabMode = strtol(optarg, NULL, 0);
            break;
        case 'r':
            outputRate = strtol(optarg, NULL, 0);
            break;
        case 'V':
            printVersion();
            goto END_MAIN;
            break;
        case '?':
        case 'h':
            printUsage(argv[0]);
            goto END_MAIN;
            break;
        default:
            fprintf(stderr, "Option '%c' not coded yet!\n", c);
            ret = -1;
            goto END_MAIN;
        }
    }
    // Setting ETI input filename
    if (optind < argc) {
        inputName = argv[optind++];
    } else {
        inputName = (char*)"/dev/stdin";
    }
    // Setting COFDM output filename
    if (optind < argc) {
        outputName = argv[optind++];
    } else {
        outputName = (char*)"/dev/stdout";
    }
    // Checking unused arguments
    if (optind != argc) {
        fprintf(stderr, "Invalid arguments:");
        while (optind != argc) {
            fprintf(stderr, " %s", argv[optind++]);
        }
        fprintf(stderr, "\n");
        printUsage(argv[0]);
        ret = -1;
        goto END_MAIN;
    }

    // Print settings
    fprintf(stderr, "Input\n");
    fprintf(stderr, "  Name: %s\n", inputName);
    fprintf(stderr, "Output\n");
    fprintf(stderr, "  Name: %s\n", outputName);
    fprintf(stderr, "  Sampling rate: ");
    if (outputRate > 1000) {
        if (outputRate > 1000000) {
            fprintf(stderr, "%.3g mHz\n", outputRate / 1000000.0f);
        } else {
            fprintf(stderr, "%.3g kHz\n", outputRate / 1000.0f);
        }
    } else {
        fprintf(stderr, "%zu Hz\n", outputRate);
    }

    // Opening ETI input file
    inputFile = fopen(inputName, "r");
    if (inputFile == NULL) {
        fprintf(stderr, "Unable to open input file!\n");
        perror(inputName);
        ret = -1;
        goto END_MAIN;
    }
    // Opening COFDM output file
    if (outputName != NULL) {
        output = new OutputFile(outputName);
    }

    flowgraph = new Flowgraph();
    data.setLength(6144);
    input = new InputMemory(&data);
    modulator = new DabModulator(outputRate, clockRate, dabMode, gainMode, amplitude);
    flowgraph->connect(input, modulator);
    flowgraph->connect(modulator, output);

    try {
        while (running) {
            enum EtiStreamType {
                ETI_STREAM_TYPE_NONE = 0,
                ETI_STREAM_TYPE_RAW,
                ETI_STREAM_TYPE_STREAMED,
                ETI_STREAM_TYPE_FRAMED,
            };
            EtiStreamType streamType = ETI_STREAM_TYPE_NONE;

            struct stat inputFileStat;
            fstat(fileno(inputFile), &inputFileStat);
            size_t inputFileLength = inputFileStat.st_size;

            if (fread(&sync, sizeof(sync), 1, inputFile) != 1) {
                fprintf(stderr, "Unable to read sync in input file!\n");
                perror(inputName);
                ret = -1;
                goto END_MAIN;
            }
            if ((sync == 0x49c5f8ff) || (sync == 0xb63a07ff)) {
                streamType = ETI_STREAM_TYPE_RAW;
                if (inputFileLength > 0) {
                    nbFrames = inputFileLength / 6144;
                } else {
                    nbFrames = (uint32_t)-1;
                }
                if (fseek(inputFile, -sizeof(sync), SEEK_CUR) != 0) {
                    if (fread(data.getData(), 6144 - sizeof(sync), 1, inputFile)
                            != 1) {
                        fprintf(stderr, "Unable to seek in input file!\n");
                        ret = -1;
                        goto END_MAIN;
                    }
                }
                goto START;
            }

            nbFrames = sync;
            if (fread(&frameSize, sizeof(frameSize), 1, inputFile) != 1) {
                fprintf(stderr, "Unable to read frame size in input file!\n");
                perror(inputName);
                ret = -1;
                goto END_MAIN;
            }
            sync >>= 16;
            sync &= 0xffff;
            sync |= ((uint32_t)frameSize) << 16;

            if ((sync == 0x49c5f8ff) || (sync == 0xb63a07ff)) {
                streamType = ETI_STREAM_TYPE_STREAMED;
                frameSize = nbFrames & 0xffff;
                if (inputFileLength > 0) {
                    nbFrames = inputFileLength / (frameSize + 2);
                } else {
                    nbFrames = (uint32_t)-1;
                }
                if (fseek(inputFile, -6, SEEK_CUR) != 0) {
                    if (fread(data.getData(), frameSize - 4, 1, inputFile)
                            != 1) {
                        fprintf(stderr, "Unable to seek in input file!\n");
                        ret = -1;
                        goto END_MAIN;
                    }
                }
                goto START;
            }

            if (fread(&sync, sizeof(sync), 1, inputFile) != 1) {
                fprintf(stderr, "Unable to read nb frame in input file!\n");
                perror(inputName);
                ret = -1;
                goto END_MAIN;
            }
            if ((sync == 0x49c5f8ff) || (sync == 0xb63a07ff)) {
                streamType = ETI_STREAM_TYPE_FRAMED;
                if (fseek(inputFile, -6, SEEK_CUR) != 0) {
                    if (fread(data.getData(), frameSize - 4, 1, inputFile)
                            != 1) {
                        fprintf(stderr, "Unable to seek in input file!\n");
                        ret = -1;
                        goto END_MAIN;
                    }
                }
                goto START;
            }

            for (size_t i = 10; i < 6144 + 10; ++i) {
                sync >>= 8;
                sync &= 0xffffff;
                if (fread((uint8_t*)&sync + 3, 1, 1, inputFile) != 1) {
                    fprintf(stderr, "Unable to read in input file!\n");
                    ret = 1;
                    goto END_MAIN;
                }
                if ((sync == 0x49c5f8ff) || (sync == 0xb63a07ff)) {
                    streamType = ETI_STREAM_TYPE_RAW;
                    if (inputFileLength > 0) {
                        nbFrames = (inputFileLength - i) / 6144;
                    } else {
                        nbFrames = (uint32_t)-1;
                    }
                    if (fseek(inputFile, -sizeof(sync), SEEK_CUR) != 0) {
                        if (fread(data.getData(), 6144 - sizeof(sync), 1, inputFile)
                                != 1) {
                            fprintf(stderr, "Unable to seek in input file!\n");
                            ret = -1;
                            goto END_MAIN;
                        }
                    }
                    goto START;
                }
            }

            fprintf(stderr, "Bad input file format!\n");
            ret = -1;
            goto END_MAIN;

START:
            fprintf(stderr, "Input file format: ");
            switch (streamType) {
            case ETI_STREAM_TYPE_RAW:
                fprintf(stderr, "raw");
                break;
            case ETI_STREAM_TYPE_STREAMED:
                fprintf(stderr, "streamed");
                break;
            case ETI_STREAM_TYPE_FRAMED:
                fprintf(stderr, "framed");
                break;
            default:
                fprintf(stderr, "unknown\n");
                ret = -1;
                goto END_MAIN;
            }
            fprintf(stderr, "\n");
            fprintf(stderr, "Input file length: %zu\n", inputFileLength);
            fprintf(stderr, "Input file nb frames: %u\n", nbFrames);

            for (frame = 0; frame < nbFrames; ++frame) {
                if (!running) {
                    break;
                }

                PDEBUG("*****************************************\n");
                PDEBUG("* Reading frame %u\n", frame);
                PDEBUG("*****************************************\n");
                if (streamType == ETI_STREAM_TYPE_RAW) {
                    frameSize = 6144;
                } else {
                    if (fread(&frameSize, sizeof(frameSize), 1, inputFile)
                            != 1) {
                        PDEBUG("End of file!\n");
                        goto END_MAIN;
                    }
                }
                PDEBUG("Frame size: %i\n", frameSize);

                if (fread(data.getData(), frameSize, 1, inputFile) != 1) {
                    fprintf(stderr,
                            "Unable to read %i data bytes in input file!\n",
                            frameSize);
                    perror(inputName);
                    ret = -1;
                    goto END_MAIN;
                }
                memset(&((uint8_t*)data.getData())[frameSize], 0x55, 6144 - frameSize);

                ////////////////////////////////////////////////////////////////
                // Proccessing data
                ////////////////////////////////////////////////////////////////
                flowgraph->run();
            }
            fprintf(stderr, "End of file reached.\n");
            if (!loop) {
                running = false;
            } else {
                fprintf(stderr, "Rewinding file.\n");
                rewind(inputFile);
            }
        }
    } catch (std::exception& e) {
        fprintf(stderr, "EXCEPTION: %s\n", e.what());
        ret = -1;
        goto END_MAIN;
    }

END_MAIN:
    ////////////////////////////////////////////////////////////////////////
    // Cleaning things
    ////////////////////////////////////////////////////////////////////////
    fprintf(stderr, "\n\n");
    fprintf(stderr, "%u DAB frames encoded\n", frame);
    fprintf(stderr, "%f seconds encoded\n", (float)frame * 0.024f);

    fprintf(stderr, "\nCleaning flowgraph...\n");
    delete flowgraph;

    // Cif
    fprintf(stderr, "\nCleaning buffers...\n");

    fprintf(stderr, "\nClosing input file...\n");
    if (inputFile != NULL) {
        fclose(inputFile);
    }

    return ret;
}
