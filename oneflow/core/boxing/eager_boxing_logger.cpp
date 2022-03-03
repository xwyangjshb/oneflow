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
#include "oneflow/core/common/global.h"
#include "oneflow/core/common/decorator.h"
#include "oneflow/core/boxing/eager_boxing_logger.h"
#include "oneflow/core/boxing/boxing_interpreter_status.h"

namespace oneflow {

namespace {

class NullEagerBoxingLogger final : public EagerBoxingLogger {
 public:
  OF_DISALLOW_COPY_AND_MOVE(NullEagerBoxingLogger);
  NullEagerBoxingLogger() = default;
  ~NullEagerBoxingLogger() override = default;

  void Log(const BoxingInterpreterStatus& status, const std::string& prefix) const override {}
};

class NaiveEagerBoxingLogger final : public EagerBoxingLogger {
 public:
  OF_DISALLOW_COPY_AND_MOVE(NaiveEagerBoxingLogger);
  NaiveEagerBoxingLogger() = default;
  ~NaiveEagerBoxingLogger() override = default;

  void Log(const BoxingInterpreterStatus& status, const std::string& prefix) const override {
    VLOG(3) << prefix << "boxing interpreter route: " << (status.boxing_interpreter_routing());
    VLOG(3) << prefix << "Altered state of sbp: " << (status.nd_sbp_routing());
    VLOG(3) << prefix << "Altered state of placement: " << (status.placement_routing());
  }
};

const EagerBoxingLogger* CreateEagerBoxingLogger() {
  if (std::getenv("ONEFLOW_DEBUG_MODE") != nullptr) {
    return new NaiveEagerBoxingLogger();
  } else {
    return new NullEagerBoxingLogger();
  }
}

}  // namespace

COMMAND(Global<const EagerBoxingLogger>::SetAllocated(CreateEagerBoxingLogger()));

}  // namespace oneflow
