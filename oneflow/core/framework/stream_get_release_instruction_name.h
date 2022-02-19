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
#ifndef ONEFLOW_CORE_FRAMEWORK_STREAM_GET_RELEASE_INSTRUCTION_NAME_H_
#define ONEFLOW_CORE_FRAMEWORK_STREAM_GET_RELEASE_INSTRUCTION_NAME_H_

#include <glog/logging.h>
#include <string>
#include "oneflow/core/common/stream_role.h"
#include "oneflow/core/common/device_type.h"
#include "oneflow/core/common/maybe.h"
#include "oneflow/core/framework/to_string.h"

namespace oneflow {

struct GetReleaseInstructionName {
  static Maybe<std::string> Case(SR<StreamRole::kInvalid>, DeviceType device_type) {  // NOLINT
    UNIMPLEMENTED_THEN_RETURN();
  }
  static Maybe<std::string> Case(SR<StreamRole::kCompute>, DeviceType device_type) {
    return *JUST(DeviceTag4DeviceType(device_type)) + ".ReleaseTensor";
  }
  static Maybe<std::string> Case(SR<StreamRole::kHost2Device>, DeviceType device_type) {
    CHECK_EQ_OR_RETURN(device_type, kCUDA);
    return std::string("cuda_h2d.ReleaseTensor");
  }
  static Maybe<std::string> Case(SR<StreamRole::kDevice2Host>, DeviceType device_type) {
    CHECK_EQ_OR_RETURN(device_type, kCUDA);
    return std::string("cuda_d2h.ReleaseTensor");
  }
  static Maybe<std::string> Case(SR<StreamRole::kSyncedLaunchedCC>, DeviceType device_type) {
    if (device_type == kCPU) { return std::string("comm_net.ReleaseTensor"); }
    CHECK_EQ_OR_RETURN(device_type, kCUDA);
    return std::string("sync_launched_nccl.ReleaseTensor");
  }
  static Maybe<std::string> Case(SR<StreamRole::kAsyncedLaunchedCC>, DeviceType device_type) {
    if (device_type == kCPU) { return std::string("comm_net.ReleaseTensor"); }
    CHECK_EQ_OR_RETURN(device_type, kCUDA);
    return std::string("async_launched_nccl.ReleaseTensor");
  }
  static Maybe<std::string> Case(SR<StreamRole::kCriticalSection>, DeviceType device_type) {
    UNIMPLEMENTED_THEN_RETURN();
  }
};

}  // namespace oneflow

#endif  // ONEFLOW_CORE_FRAMEWORK_STREAM_GET_RELEASE_INSTRUCTION_NAME_H_