// Copyright (c) 2023 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Split from op_dialect.cc to work around GNU assembler string table 10MB
// limit on MinGW GCC (binutils bug with -mbig-obj).
// This file contains OperatorDialect::initialize() which calls helper
// functions defined in op_dialect_register1.cc and op_dialect_register2.cc.

#include "paddle/fluid/pir/dialect/operator/ir/op_dialect.h"
#include "paddle/fluid/pir/dialect/operator/ir/control_flow_op.h"
#include "paddle/fluid/pir/dialect/operator/ir/manual_pylayer_op.h"
#include "paddle/fluid/pir/dialect/operator/ir/op_attribute.h"
#include "paddle/fluid/pir/dialect/operator/ir/op_type.h"
#include "paddle/fluid/pir/dialect/operator/ir/pd_op.h"
#include "paddle/fluid/pir/dialect/operator/ir/tensorrt_op.h"
#include "paddle/fluid/pir/dialect/operator/transforms/param_to_variable.h"
#include "paddle/pir/include/core/builtin_type_interfaces.h"
#ifdef PADDLE_WITH_DNNL
#include "paddle/fluid/pir/dialect/operator/ir/manual_onednn_op.h"
#endif

namespace paddle::dialect {

// Helper functions defined in separate TUs to avoid "file too big" errors
extern void RegisterOpsPart1(OperatorDialect* dialect);
extern void RegisterOpsPart2(OperatorDialect* dialect);
extern void RegisterOpsPart3(OperatorDialect* dialect);
extern void RegisterOpsPart4(OperatorDialect* dialect);

void OperatorDialect::initialize() {
  RegisterTypes<paddle::dialect::SelectedRowsType,
                paddle::dialect::SparseCooTensorType,
                paddle::dialect::SparseCsrTensorType,
                paddle::dialect::DenseTensorArrayType>();

  RegisterAttributes<paddle::dialect::IntArrayAttribute,
                     paddle::dialect::DataTypeAttribute,
                     paddle::dialect::PlaceAttribute,
                     paddle::dialect::DataLayoutAttribute>();

  // Split into 4 parts to avoid GNU assembler string table overflow
  RegisterOpsPart1(this);
  RegisterOpsPart2(this);
  RegisterOpsPart3(this);
  RegisterOpsPart4(this);

  RegisterOps<
#define GET_OP_LIST
#include "paddle/fluid/pir/dialect/operator/ir/control_flow_op.cc"  // NOLINT
      >();

  RegisterOps<
#define GET_OP_LIST
#include "paddle/fluid/pir/dialect/operator/ir/manual_op.cc"  // NOLINT
      >();

  RegisterOps<
#define GET_OP_LIST
#include "paddle/fluid/pir/dialect/operator/ir/manual_pylayer_op.cc"  // NOLINT
      >();

#ifdef PADDLE_WITH_DNNL
  RegisterOps<
#define GET_OP_LIST
#include "paddle/fluid/pir/dialect/operator/ir/manual_onednn_op.cc"  // NOLINT
      >();
#endif

  RegisterOps<TensorRTEngineOp>();

  RegisterInterfaces<ParameterConvertInterface>();
}

}  // namespace paddle::dialect
