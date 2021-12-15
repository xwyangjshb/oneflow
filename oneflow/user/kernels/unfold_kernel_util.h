/*
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
*/
#ifndef ONEFLOW_USER_KERNELS_UNFOLD_KERNEL_UTIL_H_
#define ONEFLOW_USER_KERNELS_UNFOLD_KERNEL_UTIL_H_

#include "oneflow/core/ep/include/stream.h"
#include "oneflow/core/common/shape.h"
#include "oneflow/core/common/nd_index_offset_helper.h"
#include "oneflow/core/common/switch_func.h"
#include "oneflow/core/ndarray/xpu_util.h"

namespace oneflow {

namespace user_op {

constexpr int kUnfoldPaddingValue = 0;

// NDIM range: (1, 2, 3)
// SDIM range: (1, 2), 1 indicates channels_last, 2 indicates channels_first
template<typename INDEX_T, int NDIM, int SDIM>
struct UnfoldParams {
  static constexpr int kInputNDim = NDIM + 2;
  static constexpr int kOutputNDim = NDIM * 2 + 2;
  static constexpr int kInputChannelDim = (2 - SDIM) * NDIM + 1;
  static constexpr int kOutputChannelDim = (2 - SDIM) * NDIM * 2 + 1;
  static_assert(kInputChannelDim < kInputNDim, "");
  static_assert(kOutputChannelDim < kOutputNDim, "");
  UnfoldParams(const int64_t batch_size, const int64_t channels, const int64_t* spatial_dims,
               const int32_t* kernel_size, const int32_t* padding, const int32_t* stride,
               const int32_t* dilation);
  INDEX_T in_elem_cnt;
  INDEX_T out_elem_cnt;
  INDEX_T dims[NDIM];
  int padding[NDIM];
  int stride[NDIM];
  int dilation[NDIM];
  NdIndexOffsetHelper<INDEX_T, kInputNDim> in_index_helper;
  NdIndexOffsetHelper<INDEX_T, kOutputNDim> out_index_helper;
};

// NDIM range: (1, 2, 3)
// SDIM range: (1, 2), 1 indicates channels_last, 2 indicates channels_first
template<typename INDEX_T, int NDIM, int SDIM>
UnfoldParams<INDEX_T, NDIM, SDIM>::UnfoldParams(const int64_t batch_size, const int64_t channels,
                                                const int64_t* spatial_dims,
                                                const int32_t* kernel_size, const int32_t* padding,
                                                const int32_t* stride, const int32_t* dilation)
    : in_index_helper(0), out_index_helper(0) {
  INDEX_T input_dims[kInputNDim] = {0};
  INDEX_T output_dims[kOutputNDim] = {0};
  in_elem_cnt = batch_size * channels;
  out_elem_cnt = batch_size * channels;
  input_dims[0] = batch_size;
  output_dims[0] = batch_size;
  input_dims[kInputChannelDim] = channels;
  output_dims[kOutputChannelDim] = channels;
  for (int d = 0; d < NDIM; ++d) {
    this->in_elem_cnt *= spatial_dims[d];
    this->dims[d] = spatial_dims[d];
    this->padding[d] = padding[d];
    this->stride[d] = stride[d];
    this->dilation[d] = dilation[d];
    input_dims[SDIM + d] = spatial_dims[d];
    output_dims[SDIM + d] = kernel_size[d];
    output_dims[SDIM + NDIM + d] =
        (spatial_dims[d] + 2 * padding[d] - dilation[d] * (kernel_size[d] - 1) - 1) / stride[d] + 1;
    out_elem_cnt *= output_dims[SDIM + d] * output_dims[SDIM + NDIM + d];
  }
  in_index_helper = NdIndexOffsetHelper<INDEX_T, kInputNDim>(input_dims);
  out_index_helper = NdIndexOffsetHelper<INDEX_T, kOutputNDim>(output_dims);
}

// index_a format: (N, C, di, hi, wi, db, hb, wb) or (N, di, hi, wi, db, hb, wb, C)
// index_b format: (N, C, D, H, W) or (N, D, H, W, C)
// return: true indicates out-of-bound, otherwise in-bound
template<typename INDEX_T, int NDIM, int SDIM>
OF_DEVICE_FUNC bool UnfoldIndexTransform(const UnfoldParams<INDEX_T, NDIM, SDIM>& params,
                                         const INDEX_T* index_a, INDEX_T* index_b) {
  // batch dim index transform
  index_b[0] = index_a[0];
  // channel dim index transform
  using ParamType = UnfoldParams<INDEX_T, NDIM, SDIM>;
  index_b[ParamType::kInputChannelDim] = index_a[ParamType::kOutputChannelDim];
// spatial dim index transform
#ifdef __CUDA_ARCH__
#pragma unroll
#endif
  // D,H,W spatial dim index transform
  for (int64_t d = 0; d < NDIM; ++d) {
    INDEX_T idx = index_a[SDIM + NDIM + d] * params.stride[d]
                  + index_a[SDIM + d] * params.dilation[d] - params.padding[d];
    if (idx < 0 || idx >= params.dims[d]) return true;
    index_b[SDIM + d] = idx;
  }
  return false;
}

template<DeviceType device_type, typename T, typename INDEX_T, int NDIM, int SDIM>
struct UnfoldKernelUtil {
  static void Forward(ep::Stream* stream, const UnfoldParams<INDEX_T, NDIM, SDIM>* params,
                      const T* input_ptr, T* output_ptr);
};

#define SPATIAL_NDIM_SEQ OF_PP_MAKE_TUPLE_SEQ(1) OF_PP_MAKE_TUPLE_SEQ(2) OF_PP_MAKE_TUPLE_SEQ(3)
#define SPATIAL_DIM_SEQ OF_PP_MAKE_TUPLE_SEQ(1) OF_PP_MAKE_TUPLE_SEQ(2)

#define INSTANTIATE_UNFOLD_KERNEL_UTIL(device, dtype, itype, ndim, sdim) \
  template struct UnfoldKernelUtil<device, dtype, itype, ndim, sdim>;

#define INSTANTIATE_UNFOLD_KERNEL_UTIL_WITH_TYPE_PAIR(device, dtype_pair, itype_pair, ndim, sdim) \
  INSTANTIATE_UNFOLD_KERNEL_UTIL(device, OF_PP_PAIR_FIRST(dtype_pair),                            \
                                 OF_PP_PAIR_FIRST(itype_pair), ndim, sdim)

#define INSTANTIATE_UNFOLD_KERNEL_UTIL_FOR_DEVICE(device)                                         \
  OF_PP_SEQ_PRODUCT_FOR_EACH_TUPLE(INSTANTIATE_UNFOLD_KERNEL_UTIL_WITH_TYPE_PAIR, (device),       \
                                   FLOATING_DATA_TYPE_SEQ, INDEX_DATA_TYPE_SEQ, SPATIAL_NDIM_SEQ, \
                                   SPATIAL_DIM_SEQ)

}  // namespace user_op

}  // namespace oneflow

#endif  // ONEFLOW_USER_KERNELS_UNFOLD_KERNEL_UTIL_H_