"""
Copyright 2020 The OneFlow Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

import unittest
from collections import OrderedDict

import numpy as np
from test_util import GenArgList

import oneflow as flow
import oneflow.unittest

from oneflow.test_utils.automated_test_util import *

@autotest(n=10, auto_backward=False, check_graph=False)
def test_empty_impl(test_case, ndim, placement, sbp):
    dim0 = np.random.randint(1, 5) * 8
    dim1 = np.random.randint(1, 5) * 8
    dim2 = np.random.randint(1, 5) * 8
    dim3 = np.random.randint(1, 5) * 8
    dim4 = np.random.randint(1, 5) * 8
    if ndim==1:
        x = torch.empty(dim0)
    elif ndim==2:
        x = torch.empty(dim0, dim1)
    elif ndim==3:
        x = torch.empty(dim0, dim1, dim2)
    elif ndim==4:
        x = torch.empty(dim0, dim1, dim2, dim3)
    elif ndim==5:
        x = torch.empty(dim0, dim1, dim2, dim3, dim4)

    # y = x.to_consistent(placement=placement, sbp=sbp)
    return x

class TestEmptyConsistent(flow.unittest.TestCase):
    @consistent
    def test_empty(test_case):
        # random ndim in range [1,5]
        ndim = np.random.randint(1, 6)
        for placement in all_placement():
            for sbp in all_sbp(placement, max_dim=ndim):
                test_empty_impl(test_case, ndim, placement, sbp)


if __name__ == "__main__":
    unittest.main()