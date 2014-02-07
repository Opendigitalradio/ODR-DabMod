/*
   Copyright (C) 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)
 */
/*
   This file is part of ODR-DabMod.

   ODR-DabMod is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   ODR-DabMod is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with ODR-DabMod.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "kiss_fftsimd.h"


#ifdef USE_SIMD
void kiss_fft_pack(kiss_fft_complex *in, size_t in_size, size_t in_offset,
        kiss_fft_complex *out, size_t out_size, size_t out_offset,
        size_t stride, size_t n)
{
    size_t i, j;
    complex_float *in_cplx = (complex_float*)in;

    for (i = 0; i < 4; ++i) {
        if (in_offset < in_size) {
            for (j = 0; j < n; ++j) {
                out[out_offset + j].r[i] = in_cplx[in_offset + j].r;
                out[out_offset + j].i[i] = in_cplx[in_offset + j].i;
            }
            in_offset += stride;
        }
    }
}
#endif
