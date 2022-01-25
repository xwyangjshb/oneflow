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

@autotest(n=10, check_graph=False)
def test_dot_impl(test_case, placement, sbp):
    # device = random_device()
    k = random(1000, 10000)
    x = random_pytorch_tensor(ndim=1, dim0=k).to_consistent(placement=placement, sbp=sbp)
    y = random_pytorch_tensor(ndim=1, dim0=k).to_consistent(placement=placement, sbp=sbp)
    z = torch.dot(x, y)
    return z

class TestDotConsistent(flow.unittest.TestCase):
    @consistent
    def test_dot(test_case):
        for placement in all_placement():
            for sbp in all_sbp(placement, max_dim=1):
                test_dot_impl(test_case, placement, sbp)

if __name__ == "__main__":
    unittest.main()