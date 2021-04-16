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
#include <mutex>
#include "oneflow/core/job/cluster_instruction.h"
#include "oneflow/core/job/cluster_instruction.pb.h"
#include "oneflow/core/control/ctrl_server.h"
#include "oneflow/core/control/ctrl_client.h"
#include "oneflow/core/control/global_process_ctx.h"
#include "oneflow/core/job/env_desc.h"

namespace oneflow {

namespace {

std::string GetHaltAckCtrlKey(int64_t machine_id) {
  return "HaltAckCtrlKey/" + std::to_string(machine_id);
}

// return unique sequential key
// because ctrl key is not allowed to push/pull twice
std::string GetClusterInstructionKey() {
  static int64_t seq = 0;
  return "ClusterInstructionKey/" + std::to_string(seq++);
}

class ObsoleteCtrlKeys {
 public:
  ObsoleteCtrlKeys() = default;
  ~ObsoleteCtrlKeys() = default;

  template<typename CallbackT>
  void ForEach(const CallbackT& Callback) const {
    std::unique_lock<std::mutex> lck(mutex_);
    for (const std::string& k : keys_) { Callback(k); }
  }

  void Clear() {
    std::unique_lock<std::mutex> lck(mutex_);
    keys_.clear();
  }
  void Add(const std::string& key) {
    std::unique_lock<std::mutex> lck(mutex_);
    keys_.push_back(key);
  }

 private:
  mutable std::mutex mutex_;
  std::vector<std::string> keys_;
};

COMMAND(Global<ObsoleteCtrlKeys>::SetAllocated(new ObsoleteCtrlKeys()));

void OccasionallyClearCtrlKV(const std::string& key) {
  static std::atomic<int64_t> seq(0LL);
  const static int64_t interval = 65536;
  Global<ObsoleteCtrlKeys>::Get()->Add(key);
  // 1 instead of 0 is better for avoid clearing no ctrl kv
  if ((seq++) % interval == 1) {
    OF_ENV_BARRIER();
    if (GlobalProcessCtx::IsThisProcessMaster()) {
      Global<ObsoleteCtrlKeys>::Get()->ForEach(
          [](const std::string& k) { Global<CtrlClient>::Get()->ClearMasterKV(k); });
    }
    Global<ObsoleteCtrlKeys>::Get()->Clear();
    OF_ENV_BARRIER();
  }
}

void PushClusterInstruction(const ClusterInstructionProto& cluster_instruction,
                            const std::string& key) {
  Global<CtrlClient>::Get()->PushMasterKV(key, cluster_instruction);
  OccasionallyClearCtrlKV(key);
}

void PullClusterInstruction(ClusterInstructionProto* cluster_instruction) {
  const std::string& key = GetClusterInstructionKey();
  LOG(INFO) << "PullClusterInstruction: " << key;
  Global<CtrlClient>::Get()->PullMasterKV(key, cluster_instruction);
  OccasionallyClearCtrlKV(key);
}

}  // namespace

void ClusterInstruction::NewSessionBarrier() {
  OF_ENV_BARRIER();
  Global<CtrlClient>::Get()->Clear();
  Global<ObsoleteCtrlKeys>::Get()->Clear();
  OF_ENV_BARRIER();
}

void ClusterInstruction::MasterSendSessionStart() {
  ClusterInstructionProto cluster_instruction;
  cluster_instruction.mutable_cluster_ctrl_session_start();
  const std::string& key = GetClusterInstructionKey();
  LOG(INFO) << "MasterSendSessionStart: " << key;
  PushClusterInstruction(cluster_instruction, key);
  NewSessionBarrier();
}

void ClusterInstruction::MasterSendHalt() {
  ClusterInstructionProto cluster_instruction;
  cluster_instruction.mutable_cluster_ctrl_halt();
  const std::string& key = GetClusterInstructionKey();
  LOG(INFO) << "MasterSendHalt: " << key;
  PushClusterInstruction(cluster_instruction, key);
  HaltBarrier();
}

void ClusterInstruction::MasterSendAbort() {
  LOG(INFO) << "sending abort instruction";
  ClusterInstructionProto cluster_instruction;
  cluster_instruction.mutable_cluster_ctrl_abort();
  const std::string& key = GetClusterInstructionKey();
  LOG(ERROR) << "MasterSendAbort: " << key;
  PushClusterInstruction(cluster_instruction, key);
}

void ClusterInstruction::MasterSendEagerInstruction(
    const ClusterInstructionProto& cluster_instruction) {
  CHECK(cluster_instruction.has_eager_instruction());
  const std::string& key = GetClusterInstructionKey();
  LOG(INFO) << "MasterSendEagerInstruction: " << key;
  PushClusterInstruction(cluster_instruction, key);
}

void ClusterInstruction::WorkerReceiveInstruction(ClusterInstructionProto* cluster_instruction) {
  PullClusterInstruction(cluster_instruction);
}

void ClusterInstruction::HaltBarrier() { OF_ENV_BARRIER(); }

void ClusterInstruction::EagerSyncBarrier() {
  // TODO(jianhao): update here after eager instructions are run asynchronously
  OF_ENV_BARRIER();
}

}  // namespace oneflow
