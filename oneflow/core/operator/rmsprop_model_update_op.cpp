#include "oneflow/core/operator/rmsprop_model_update_op.h"

namespace oneflow {

void RMSPropModelUpdateOp::MdUpdtVirtualInitFromOpConf() {
  EnrollDataTmpBn("mean_square");
}

const PbMessage& RMSPropModelUpdateOp::GetSpecialConf() const {
  return op_conf().rmsprop_mdupdt_conf();
}

void RMSPropModelUpdateOp::InferBlobDescs(
    std::function<BlobDesc*(const std::string)> GetBlobDesc4BnInOp,
    const ParallelContext* parallel_ctx) const {
  const BlobDesc* model_blob_desc = GetBlobDesc4BnInOp("model");
  CHECK_EQ(model_blob_desc->data_type(),
           JobDesc::Singleton()->DefaultDataType());
  CHECK_EQ(model_blob_desc->has_data_id_field(), false);
  *GetBlobDesc4BnInOp("mean_square") = *model_blob_desc;
}

REGISTER_OP(OperatorConf::kRmspropMdupdtConf, RMSPropModelUpdateOp);

}  // namespace oneflow
