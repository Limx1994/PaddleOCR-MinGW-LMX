// Copyright (c) 2023 PaddlePaddle Authors. All Rights Reserved.
// Split from op_dialect.cc to work around GNU assembler string table 10MB limit.
// RegisterOps GET_OP_LIST1 + GET_OP_LIST2

#include "paddle/fluid/pir/dialect/operator/ir/op_dialect.h"
#include "paddle/fluid/pir/dialect/operator/ir/pd_op.h"

namespace paddle::dialect {

void RegisterOpsPart1(OperatorDialect* dialect) {
  dialect->RegisterOps<
#define GET_OP_LIST1
#include "paddle/fluid/pir/dialect/operator/ir/pd_op_info.cc"  // NOLINT
      >();
}

void RegisterOpsPart2(OperatorDialect* dialect) {
  dialect->RegisterOps<
#define GET_OP_LIST2
#include "paddle/fluid/pir/dialect/operator/ir/pd_op_info.cc"  // NOLINT
      >();
}

}  // namespace paddle::dialect
