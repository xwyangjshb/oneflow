#ifndef ONEFLOW_CORE_VM_VPU_INSTRUCTION_MSG_H_
#define ONEFLOW_CORE_VM_VPU_INSTRUCTION_MSG_H_

#include <cstring>
#include <mutex>
#include "oneflow/core/common/flat_msg.h"
#include "oneflow/core/common/object_msg.h"
#include "oneflow/core/vm/vm_stream_desc.msg.h"
#include "oneflow/core/vm/mirrored_object.msg.h"
#include "oneflow/core/vm/vm_stream_type.h"
#include "oneflow/core/vm/vm_instruction_msg_observer.h"

namespace oneflow {

static const int kVmInstructionOperandLimit = 64;

// clang-format off
BEGIN_FLAT_MSG(VmInstructionProto);
  FLAT_MSG_DEFINE_OPTIONAL(int64_t, vm_instruction_id);
  FLAT_MSG_DEFINE_OPTIONAL(VmStreamTypeId, vm_stream_type_id);
  FLAT_MSG_DEFINE_OPTIONAL(VmInstructionOpcode, opcode);
  FLAT_MSG_DEFINE_REPEATED(VmInstructionOperand, operand, kVmInstructionOperandLimit);
  FLAT_MSG_DEFINE_OPTIONAL(VmStreamMask, vm_stream_mask);
END_FLAT_MSG(VmInstructionProto);
// clang-format on

// clang-format off
BEGIN_OBJECT_MSG(VmInstructionMsg);
  PUBLIC void __Init__() { __Init__(&VmInstructionMsgNoneObserver::NewObserver); }
  PUBLIC template<typename NewObserverT>
  void __Init__(const NewObserverT& NewObserver) { mutable_observer()->__Init__(NewObserver); }

  // fields
  OBJECT_MSG_DEFINE_FLAT_MSG(VmInstructionProto, vm_instruction_proto);
  OBJECT_MSG_DEFINE_OPTIONAL(Wrapper4CppObject<VmInstructionMsgObserver>, observer);

  // links
  OBJECT_MSG_DEFINE_LIST_LINK(vm_instr_msg_link);
END_OBJECT_MSG(VmInstructionMsg);
// clang-format on

class VmInstrChain;
// clang-format off
BEGIN_OBJECT_MSG(VmInstrChainEdge);
  // methods
  PUBLIC void __Init__(VmInstrChain* src_vm_instr_chain, VmInstrChain* dst_vm_instr_chain) {
    set_src_vm_instr_chain(src_vm_instr_chain);
    set_dst_vm_instr_chain(dst_vm_instr_chain);
  }
  // links
  OBJECT_MSG_DEFINE_SKIPLIST_KEY(10, VmInstrChain*, src_vm_instr_chain);
  OBJECT_MSG_DEFINE_SKIPLIST_KEY(10, VmInstrChain*, dst_vm_instr_chain);
END_OBJECT_MSG(VmInstrChainEdge);
// clang-format on

// clang-format off
BEGIN_OBJECT_MSG(VmInstruction);
  // methods
  PUBLIC void __Init__(VmInstrChain* vm_instr_chain, VmInstructionMsg* vm_instr_msg) {
    set_vm_instr_chain(vm_instr_chain);
    reset_vm_instr_msg(vm_instr_msg);
  }
  // fields
  OBJECT_MSG_DEFINE_OPTIONAL(VmInstructionMsg, vm_instr_msg);
  OBJECT_MSG_DEFINE_RAW_PTR(VmInstrChain, vm_instr_chain);

  // links
  OBJECT_MSG_DEFINE_LIST_LINK(vm_instruction_link);
  OBJECT_MSG_DEFINE_SKIPLIST_HEAD(MirroredObjectAccess, mirrored_object_id, mirrored_object_id2access);
END_OBJECT_MSG(VmInstruction);
// clang-format on

class VmStream;
// clang-format off
BEGIN_OBJECT_MSG(VmInstrChain);
  // methods
  PUBLIC void __Init__(VmInstructionMsg* vm_instr_msg, VmStream* vm_stream);

  // fields
  OBJECT_MSG_DEFINE_RAW_PTR(VmStream, vm_stream); 

  // links
  OBJECT_MSG_DEFINE_LIST_LINK(vm_instr_chain_link);
  OBJECT_MSG_DEFINE_LIST_HEAD(VmInstruction, vm_instruction_link, vm_instruction_list);
  OBJECT_MSG_DEFINE_SKIPLIST_HEAD(VmInstrChainEdge, src_vm_instr_chain, in_edges);
  OBJECT_MSG_DEFINE_SKIPLIST_HEAD(VmInstrChainEdge, dst_vm_instr_chain, out_edges);
END_OBJECT_MSG(VmInstrChain);
// clang-format on

static const int kVmInstructionStatusBufferLength = 1024;

// clang-format off
BEGIN_FLAT_MSG(VmInstructionStatusBuffer);
  FLAT_MSG_DEFINE_REPEATED(char, buffer, kVmInstructionStatusBufferLength);
END_FLAT_MSG(VmInstructionStatusBuffer);
// clang-format on

// clang-format off
BEGIN_OBJECT_MSG(VmInstrChainPackage);
  // methods
  PUBLIC void __Init__() {}
  PUBLIC void __Init__(VmStream* vm_stream);
  PUBLIC void __Delete__();
  PUBLIC bool Done() const;

  // fields
  OBJECT_MSG_DEFINE_FLAT_MSG(VmInstructionStatusBuffer, status_buffer);
  OBJECT_MSG_DEFINE_RAW_PTR(VmStream, vm_stream);
  OBJECT_MSG_DEFINE_RAW_PTR(const VmStreamType, vm_stream_type);

  // links
  OBJECT_MSG_DEFINE_LIST_HEAD(VmInstrChain, vm_instr_chain_link, vm_instr_chain_list);
  OBJECT_MSG_DEFINE_LIST_LINK(waiting_pkg_link);
  OBJECT_MSG_DEFINE_LIST_LINK(vm_instr_chain_pkg_link);
END_OBJECT_MSG(VmInstrChainPackage);
// clang-format on

}  // namespace oneflow

#endif  // ONEFLOW_CORE_VM_VPU_INSTRUCTION_MSG_H_
