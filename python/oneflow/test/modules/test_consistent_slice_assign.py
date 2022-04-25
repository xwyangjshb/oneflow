import unittest
from collections import OrderedDict

import numpy as np
import oneflow as flow
import oneflow.unittest

from oneflow.test_utils.automated_test_util import *


def _test_logical_slice_assign(test_case, placement, sbp):
    x = random_tensor(2, 4, 4, requires_grad=False).oneflow
    x_numpy = x.detach().cpu().numpy()

    x = x.to_global(placement=placement, sbp=sbp)
    x[:, :2] = 3 
    if flow.env.get_rank() == 0:
        print("===", x)
    x_numpy[:, :2] = 3
    if flow.env.get_rank() == 0:
        print("---", x_numpy)

    test_case.assertTrue(x.sbp in [(oneflow.sbp.broadcast,)])
    test_case.assertTrue(np.array_equal(x.numpy(), x_numpy))


class TestGlobalLogicalSliceAssign(flow.unittest.TestCase):
    @globaltest
    def test_logical_slice_assign(test_case):
        for placement in all_placement():
            for sbp in all_sbp(placement, max_dim=2):
                # logical slice assign not support 2d sbp currently
                # logical slice assign only support broadcast currently
                if len(sbp) > 1 or sbp[0] != flow.sbp.broadcast:
                    continue
                _test_logical_slice_assign(test_case, placement, sbp)


if __name__ == "__main__":
    unittest.main()
