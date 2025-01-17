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
#include "oneflow/core/eager/eager_blob_object.h"
#include "oneflow/core/vm/allocator.h"
#include "oneflow/core/framework/to_string.h"
#include "oneflow/core/framework/shut_down_util.h"
#include "oneflow/core/common/shape_vec.h"
#include "oneflow/core/device/cpu_device_context.h"

namespace oneflow {
namespace vm {

EagerBlobObject::EagerBlobObject(const std::shared_ptr<MemoryCase>& mem_case,
                                 const std::shared_ptr<Shape>& shape, DataType data_type,
                                 const std::shared_ptr<TensorStorage>& tensor_storage,
                                 const intrusive::shared_ptr<LocalDepObject>& dep_object)
    : is_dynamic_(false),
      mem_case_(mem_case),
      data_type_(data_type),
      shape_(shape),
      storage_offset_(0),
      tensor_storage_(tensor_storage),
      is_shape_synced_(true),
      compute_local_dep_object_(dep_object),
      blob_desc_(shape, data_type) {
  CHECK(static_cast<bool>(shape));
  CHECK(static_cast<bool>(tensor_storage));
}

Blob* EagerBlobObject::blob() {
  if (!blob_) {
    blob_.reset(new Blob(*mem_case_, &blob_desc_, mut_header_ptr(), mut_dptr<char>()));
  }
  return blob_.get();
}

void EagerBlobObject::set_storage_offset(const int64_t offset) { storage_offset_ = offset; }

Maybe<void> EagerBlobObject::TryAllocateBlobBodyMemory(DeviceCtx* device_ctx) {
  const bool pin_memory = EagerBlobObject::pin_memory();
  vm::Allocator* allocator = nullptr;
  if (pin_memory) {
    CHECK_EQ_OR_RETURN(device_ctx->device_type(), DeviceType::kCPU)
        << Error::RuntimeError() << "cannot pin tensor with device: " << device_ctx->device_type()
        << ", only dense CPU tensors can be pinned.";
    allocator = dynamic_cast<CpuDeviceCtx*>(device_ctx)->mut_pin_memory_allocator();
    if (allocator == nullptr) {
      // for some reason, the pin_memory_allocator will fail to create
      // e.g. with no CUDA library support and only can use oneflow in cpu only mode
      return Error::RuntimeError()
             << "create pin_memory allocator failed for some reason. mostly, this error has "
                "occurred because you are trying to use some CUDA functionality, but the CUDA "
                "library has not been loaded by the dynamic linker for some reason.";
    }
  } else {
    allocator = device_ctx->mut_allocator();
  }
  CHECK_NOTNULL_OR_RETURN(allocator) << Error::RuntimeError() << "allocator created failed!";
  size_t required_body_bytes = AlignedByteSizeOfBlobBody();
  if (required_body_bytes == 0) {
    CHECK_ISNULL_OR_RETURN(tensor_storage_->blob_dptr());
    return Maybe<void>::Ok();
  }
  if (tensor_storage_->blob_dptr() != nullptr) {
    CHECK_GE_OR_RETURN(tensor_storage_->blob_bytes(), ByteSizeOfBlobBody())
        << "This blob has been allocated memory, but less than needed space.";
    return Maybe<void>::Ok();
  }
  {
    // reset tensor_storage_;
    const auto& Free = [allocator, required_body_bytes](char* dptr) {
      if (IsShuttingDown()) { return; }
      allocator->Deallocate(dptr, required_body_bytes);
    };
    char* dptr = nullptr;
    allocator->Allocate(&dptr, required_body_bytes);
    tensor_storage_->set_blob_dptr(std::unique_ptr<char, std::function<void(char*)>>(dptr, Free),
                                   required_body_bytes);

    InitNonPODTypeEagerBlobObjectIfNeed(tensor_storage_->non_pod_allocator(), this);
  }
  return Maybe<void>::Ok();
}

}  // namespace vm
}  // namespace oneflow
