from unittest import TestCase

import numpy as np
import pandas as pd
import src.Dab_Util as DU

class TestDab_Util(TestCase):

    def test_subsample_align(self, sample_orig=r'../test_data/orig_rough_aligned.dat',
                             sample_rec =r'../test_data/recored_rough_aligned.dat',
                             length = 10240, max_size = 1000000):
        du = DU.Dab_Util(8196000)
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
