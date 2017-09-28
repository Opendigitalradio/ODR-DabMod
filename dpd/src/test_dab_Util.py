# -*- coding: utf-8 -*-
#
# Test code for DAB util
#
# http://www.opendigitalradio.org
# Licence: The MIT License, see notice at the end of this file

from unittest import TestCase

import numpy as np
import pandas as pd
import src.Dab_Util as DU

class TestDab_Util(TestCase):

    def test_subsample_align(self, sample_orig=r'../test_data/orig_rough_aligned.dat',
                             sample_rec =r'../test_data/recored_rough_aligned.dat',
                             length = 10240, max_size = 1000000):
        du = DU
        res1 = []
        res2 = []
        for i in range(10):
            start = np.random.randint(50, max_size)
            r = np.random.randint(-50, 50)

            s1 = du.fromfile(sample_orig, offset=start+r, length=length)
            s2 = du.fromfile(sample_rec, offset=start, length=length)

            res1.append(du.lag_upsampling(s2, s1, 32))

            s1_aligned, s2_aligned = du.subsample_align(s1, s2)

            res2.append(du.lag_upsampling(s2_aligned, s1_aligned, 32))

        error_rate = np.mean(np.array(res2) != 0)
        self.assertEqual(error_rate, 0.0, "The error rate for aligning was %.2f%%"
                         % error_rate * 100)

#def test_using_aligned_pair(sample_orig=r'../data/orig_rough_aligned.dat', sample_rec =r'../data/recored_rough_aligned.dat', length = 10240, max_size = 1000000):
#    res = []
#    for i in tqdm(range(100)):
#        start = np.random.randint(50, max_size)
#        r = np.random.randint(-50, 50)
#
#        s1 = du.fromfile(sample_orig, offset=start+r, length=length)
#        s2 = du.fromfile(sample_rec, offset=start, length=length)
#
#        res.append({'offset':r,
#                    '1':r - du.lag_upsampling(s2, s1, n_up=1),
#                    '2':r - du.lag_upsampling(s2, s1, n_up=2),
#                    '3':r - du.lag_upsampling(s2, s1, n_up=3),
#                    '4':r - du.lag_upsampling(s2, s1, n_up=4),
#                    '8':r - du.lag_upsampling(s2, s1, n_up=8),
#                    '16':r - du.lag_upsampling(s2, s1, n_up=16),
#                    '32':r - du.lag_upsampling(s2, s1, n_up=32),
#                    })
#    df = pd.DataFrame(res)
#    df = df.reindex_axis(sorted(df.columns), axis=1)
#    print(df.describe())
#
#
#print("Align using upsampling")
#for n_up in [1, 2, 3, 4, 7, 8, 16]:
#   correct_ratio = test_phase_offset(lambda x,y: du.lag_upsampling(x,y,n_up), tol=1./n_up)
#   print("%.1f%% of the tested offsets were measured within tolerance %.4f for n_up = %d" % (correct_ratio * 100, 1./n_up, n_up))
#test_using_aligned_pair()
#
#print("Phase alignment")
#test_subsample_alignment()


# The MIT License (MIT)
#
# Copyright (c) 2017 Andreas Steger
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
