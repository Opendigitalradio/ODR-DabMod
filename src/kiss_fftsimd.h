/*
   Copyright (C) 2011, 2012
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

#ifndef KISS_FFTSIMD_H
#define KISS_FFTSIMD_H

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <kiss_fft.h>


#define FFT_TYPE kiss_fft_cpx
#define FFT_PLAN kiss_fft_cfg
#define FFT_REAL(a) (a).r
#define FFT_IMAG(a) (a).i


#ifdef __SSE__
#include <xmmintrin.h>
union __u128 {
    __m128 m;
    float f[4];
};
#endif


#ifdef USE_SIMD

typedef struct {
    float r[4];
    float i[4];
} kiss_fft_complex;

typedef struct {
    float r;
    float i;
} complex_float;

void kiss_fft_pack(kiss_fft_complex *in, size_t in_size, size_t in_offset,
        kiss_fft_complex *out, size_t out_size, size_t out_offset,
        size_t stride, size_t n);
void kiss_fft_unpack(kiss_fft_complex *dst, kiss_fft_complex *src, size_t n, size_t offset, size_t stride);

#endif


#endif // KISS_FFTSIMD_H
