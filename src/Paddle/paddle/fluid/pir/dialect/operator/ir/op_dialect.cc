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

#include "paddle/fluid/pir/dialect/operator/ir/op_dialect.h"
#include "paddle/fluid/framework/custom_operator_utils.h"
#include "paddle/fluid/pir/dialect/operator/interface/layout_transformation.h"
#include "paddle/fluid/pir/dialect/operator/interface/vjp.h"
#include "paddle/fluid/pir/dialect/operator/ir/api_builder.h"
#include "paddle/fluid/pir/dialect/operator/ir/control_flow_op.h"
#include "paddle/fluid/pir/dialect/operator/ir/manual_api.h"
#include "paddle/fluid/pir/dialect/operator/ir/manual_pylayer_op.h"
#include "paddle/fluid/pir/dialect/operator/ir/op_attribute.h"
#include "paddle/fluid/pir/dialect/operator/ir/op_type.h"
#include "paddle/fluid/pir/dialect/operator/ir/pd_op.h"
#include "paddle/fluid/pir/dialect/operator/ir/type_storage.h"
#include "paddle/fluid/pir/dialect/operator/trait/inplace.h"
#include "paddle/fluid/pir/dialect/operator/transforms/param_to_variable.h"
#include "paddle/fluid/pir/dialect/operator/utils/utils.h"
#include "paddle/pir/include/core/builtin_type_interfaces.h"
#include "paddle/pir/include/core/interface_value.h"
#include "paddle/pir/include/core/ir_printer.h"
#include "paddle/pir/include/core/utils.h"
#include "paddle/pir/include/dialect/control_flow/ir/cf_dialect.h"
#include "paddle/pir/include/dialect/control_flow/ir/cf_op.h"
#include "paddle/pir/include/dialect/shape/ir/shape_attribute.h"
#ifdef PADDLE_WITH_DNNL
#include "paddle/fluid/pir/dialect/operator/ir/manual_onednn_op.h"
#endif
#include "paddle/fluid/pir/dialect/distributed/ir/dist_tools.h"
#include "paddle/fluid/pir/dialect/distributed/ir/dist_type.h"
#include "paddle/fluid/pir/dialect/operator/ir/tensorrt_op.h"
#include "paddle/phi/infermeta/spmd_rules/rules.h"
#include "paddle/pir/include/core/attribute.h"

namespace paddle::dialect {

struct CombineOpInferSymbolicShapeInterfaceModel
    : public InferSymbolicShapeInterface::Concept {
  static inline bool InferSymbolicShape(
      pir::Operation* op, pir::InferSymbolicShapeContext* infer_context) {
    if (op->operand(0).type().dyn_cast<DenseTensorType>()) {
      const auto shape_data_list = [&] {
        symbol::TensorListShapeOrDataDimExprs shape_data_list;
        for (size_t i = 0; i < op->num_operands(); ++i) {
          PADDLE_ENFORCE_NOT_NULL(
              op->operand(i).type().dyn_cast<DenseTensorType>(),
              common::errors::InvalidArgument(
                  "The operand at index %d must be a DenseTensorArray. "
                  "Currently InferSymbolicShape of CombineOp only accepts "
                  "inputs that are either all DenseTensors or all "
                  "DenseTensorArrays.",
                  i));
          shape_data_list.emplace_back(
              infer_context->GetShapeOrDataForValue(op->operand_source(i))
                  .dyn_cast<symbol::TensorShapeOrDataDimExprs>());
        }
        return shape_data_list;
      }();
      symbol::ShapeOrDataDimExprs shape_data{shape_data_list};
      infer_context->SetShapeOrDataForValue(op->result(0), shape_data);
      return true;
    } else if (op->operand(0).type().dyn_cast<DenseTensorArrayType>()) {
      // Note: Return NullShapeOrDataDimExpr for CombineOp with all
      // DenseTensorArrayType. The logic is designed for add_n_array op.
      // TODO(ooooo): Actually RankedTensorArrayListShapeOrDataDimExprs is
      // better.
      for (size_t i = 0; i < op->num_operands(); ++i) {
        PADDLE_ENFORCE_NOT_NULL(
            op->operand(i).type().dyn_cast<DenseTensorArrayType>(),
            common::errors::InvalidArgument(
                "The operand at index %d must be a DenseTensorArray. Currently "
                "InferSymbolicShape of CombineOp only accepts inputs that are "
                "either all DenseTensors or all DenseTensorArrays.",
                i));
      }
      return true;
    } else {
      PADDLE_THROW(common::errors::InvalidArgument(
          "Currently InferSymbolicShape of CombineOp only accepts "
          "inputs that are either all DenseTensors or all DenseTensorArrays."));
    }
  }

  CombineOpInferSymbolicShapeInterfaceModel()
      : InferSymbolicShapeInterface::Concept(InferSymbolicShape) {}
};

struct ConstantOpInferSymbolicShapeInterfaceModel
    : public InferSymbolicShapeInterface::Concept {
  static inline bool InferSymbolicShape(
      pir::Operation* op, pir::InferSymbolicShapeContext* infer_context) {
    PADDLE_ENFORCE_NOT_NULL(
        op->result(0).type().dyn_cast<DenseTensorType>(),
        common::errors::InvalidArgument(
            "Currently InferSymbolicShape of ConstantOp only support "
            "DenseTensorType result."));

    const std::vector<symbol::DimExpr> out_dims = [op] {
      std::vector<symbol::DimExpr> dims;
      const std::vector<int64_t> result_dims = common::vectorize(
          op->result(0).type().dyn_cast<pir::DenseTensorType>().dims());
      for (size_t i = 0; i < result_dims.size(); i++) {
        dims.emplace_back(result_dims[i]);
      }
      return dims;
    }();

    infer_context->SetShapeOrDataForValue(
        op->result(0),
        symbol::ShapeOrDataDimExprs{
            symbol::TensorShapeOrDataDimExprs(out_dims)});

    return true;
  }

  ConstantOpInferSymbolicShapeInterfaceModel()
      : InferSymbolicShapeInterface::Concept(InferSymbolicShape) {}
};

struct ParameterOpInferSymbolicShapeInterfaceModel
    : public InferSymbolicShapeInterface::Concept {
  static inline bool InferSymbolicShape(
      pir::Operation* op, pir::InferSymbolicShapeContext* infer_context) {
    pir::Value res0 = op->result(0);

    std::vector<int64_t> dims =
        common::vectorize(res0.type().dyn_cast<pir::DenseTensorType>().dims());

    // TODO(zhangbopd): check whether it's right for other cases
    std::vector<symbol::DimExpr> sym_shape;
    for (int64_t dim : dims) {
      symbol::DimExpr dim_expr;
      if (dim == -1) {
        symbol::DimExpr res_dim_expr(infer_context->GetNextSymName());
        dim_expr = res_dim_expr;
      } else {
        symbol::DimExpr res_dim_expr(dim);
        dim_expr = res_dim_expr;
      }
      sym_shape.push_back(dim_expr);
    }

    symbol::ShapeOrDataDimExprs shape_data{
        symbol::TensorShapeOrDataDimExprs(sym_shape)};

    infer_context->SetShapeOrDataForValue(res0, shape_data);

    return true;
  }

  ParameterOpInferSymbolicShapeInterfaceModel()
      : InferSymbolicShapeInterface::Concept(InferSymbolicShape) {}
};

struct SetParameterOpInferSymbolicShapeInterfaceModel
    : public InferSymbolicShapeInterface::Concept {
  static inline bool InferSymbolicShape(
      pir::Operation* op, pir::InferSymbolicShapeContext* infer_context) {
    return true;
  }

  SetParameterOpInferSymbolicShapeInterfaceModel()
      : InferSymbolicShapeInterface::Concept(InferSymbolicShape) {}
};

struct ShadowOutputOpInferSymbolicShapeInterfaceModel
    : public InferSymbolicShapeInterface::Concept {
  static inline bool InferSymbolicShape(
      pir::Operation* op, pir::InferSymbolicShapeContext* infer_context) {
    pir::Value operand_source = op->operand_source(0);
    auto input_shapeordata =
        infer_context->GetShapeOrDataForValue(operand_source);

    symbol::ShapeOrDataDimExprs shape_data = input_shapeordata;
    pir::shape::SetShapeAttrForOp(op, shape_data);

    return true;
  }

  ShadowOutputOpInferSymbolicShapeInterfaceModel()
      : InferSymbolicShapeInterface::Concept(InferSymbolicShape) {}
};

struct SliceOpInferSymbolicShapeInterfaceModel
    : public InferSymbolicShapeInterface::Concept {
  static inline bool InferSymbolicShape(
      pir::Operation* op, pir::InferSymbolicShapeContext* infer_context) {
    const auto index =
        op->attributes().at("index").dyn_cast<pir::Int32Attribute>().data();
    const auto& input_shape =
        infer_context->GetShapeOrDataForValue(op->operand_source(0));
    PADDLE_ENFORCE_EQ(input_shape.isa<symbol::TensorListShapeOrDataDimExprs>(),
                      true,
                      common::errors::InvalidArgument(
                          "Input shape can not be converted, please check"));
    const symbol::TensorListShapeOrDataDimExprs& data_shape_list =
        input_shape.dyn_cast<symbol::TensorListShapeOrDataDimExprs>();
    const symbol::TensorShapeOrDataDimExprs& output_shape =
        data_shape_list[index];
    infer_context->SetShapeOrDataForValue(op->result(0), output_shape);

    return true;
  }

  SliceOpInferSymbolicShapeInterfaceModel()
      : InferSymbolicShapeInterface::Concept(InferSymbolicShape) {}
};

struct SplitOpInferSymbolicShapeInterfaceModel
    : public InferSymbolicShapeInterface::Concept {
  static inline bool InferSymbolicShape(
      pir::Operation* op, pir::InferSymbolicShapeContext* infer_context) {
    const symbol::TensorListShapeOrDataDimExprs& shape_data_list =
        infer_context->GetShapeOrDataForValue(op->operand_source(0))
            .dyn_cast<symbol::TensorListShapeOrDataDimExprs>();

    for (uint32_t rst_idx = 0; rst_idx < op->num_results(); rst_idx++) {
      infer_context->SetShapeOrDataForValue(
          op->result(rst_idx),
          symbol::ShapeOrDataDimExprs{shape_data_list.at(rst_idx)});
    }
    return true;
  }

  SplitOpInferSymbolicShapeInterfaceModel()
      : InferSymbolicShapeInterface::Concept(InferSymbolicShape) {}
};

struct YieldOpInferSymbolicShapeInterfaceModel
    : public InferSymbolicShapeInterface::Concept {
  static inline bool InferSymbolicShape(
      pir::Operation* op, pir::InferSymbolicShapeContext* infer_context) {
    // Since YieldOp has no output, just return true
    return true;
  }

  YieldOpInferSymbolicShapeInterfaceModel()
      : InferSymbolicShapeInterface::Concept(InferSymbolicShape) {}
};

OperatorDialect::OperatorDialect(pir::IrContext* ctx)
    : pir::Dialect(name(), ctx, pir::TypeId::get<OperatorDialect>()) {
  initialize();
  ctx->GetOrRegisterDialect<::pir::ControlFlowDialect>();

  auto info = ctx->GetRegisteredOpInfo(pir::TuplePushOp::name());
  info.AttachInterface(
      pir::InterfaceValue::Get<VjpInterface, TuplePushOpVjpInterfaceModel>());

  info = ctx->GetRegisteredOpInfo(pir::CombineOp::name());
  info.AttachInterface(
      pir::InterfaceValue::Get<InferSymbolicShapeInterface,
                               CombineOpInferSymbolicShapeInterfaceModel>());
  info.AttachInterface(pir::InterfaceValue::Get<
                       LayoutTransformationInterface,
                       LayoutTransformationInterface::Model<pir::CombineOp>>());

  info = ctx->GetRegisteredOpInfo(pir::ParameterOp::name());
  info.AttachInterface(
      pir::InterfaceValue::Get<InferSymbolicShapeInterface,
                               ParameterOpInferSymbolicShapeInterfaceModel>());

  info = ctx->GetRegisteredOpInfo(pir::ShadowOutputOp::name());
  info.AttachInterface(pir::InterfaceValue::Get<
                       InferSymbolicShapeInterface,
                       ShadowOutputOpInferSymbolicShapeInterfaceModel>());

  info = ctx->GetRegisteredOpInfo(pir::SplitOp::name());
  info.AttachInterface(
      pir::InterfaceValue::Get<InferSymbolicShapeInterface,
                               SplitOpInferSymbolicShapeInterfaceModel>());

  info = ctx->GetRegisteredOpInfo(pir::YieldOp::name());
  info.AttachInterface(
      pir::InterfaceValue::Get<InferSymbolicShapeInterface,
                               YieldOpInferSymbolicShapeInterfaceModel>());

  info = ctx->GetRegisteredOpInfo(pir::SetParameterOp::name());
  info.AttachInterface(pir::InterfaceValue::Get<
                       InferSymbolicShapeInterface,
                       SetParameterOpInferSymbolicShapeInterfaceModel>());

  info = ctx->GetRegisteredOpInfo(pir::SliceOp::name());
  info.AttachInterface(
      pir::InterfaceValue::Get<InferSymbolicShapeInterface,
                               SliceOpInferSymbolicShapeInterfaceModel>());
}

void PrintTypeImpl(pir::Type type, std::ostream& os) {
  os << type.dialect().name();
  os << '.';
  if (auto selected_rows_type = type.dyn_cast<SelectedRowsType>()) {
    os << "selectedrows<";
    for (auto d : common::vectorize(selected_rows_type.dims())) {
      os << d;
      os << "x";
    }
    selected_rows_type.dtype().Print(os);
    os << ">";
  } else if (auto tensor_array_type = type.dyn_cast<DenseTensorArrayType>()) {
    os << "tensor_array<";
    tensor_array_type.dtype().Print(os);
    os << ">";
  } else if (auto sparse_coo_tensor_type =
                 type.dyn_cast<SparseCooTensorType>()) {
    os << "sparsecootensor<";
    for (auto d : common::vectorize(sparse_coo_tensor_type.dims())) {
      os << d;
      os << "x";
    }
    sparse_coo_tensor_type.dtype().Print(os);
    os << ">";
  }
}
void PrintAttributeImpl(pir::Attribute attr, std::ostream& os) {
  if (auto int_array_attr = attr.dyn_cast<IntArrayAttribute>()) {
    phi::IntArray data = int_array_attr.data();
    os << "[";
    const auto& inner_data = data.GetData();
    pir::detail::PrintInterleave(
        inner_data.begin(),
        inner_data.end(),
        [&os](int64_t i) { os << i; },
        [&os]() { os << ","; });
    os << "]";
  } else if (auto data_type_attr = attr.dyn_cast<DataTypeAttribute>()) {
    os << data_type_attr.data();
  } else if (auto place_type_attr = attr.dyn_cast<PlaceAttribute>()) {
    os << place_type_attr.data();
  } else if (auto data_layout_attr = attr.dyn_cast<DataLayoutAttribute>()) {
    os << data_layout_attr.data();
  } else {
    os << "<#AttrNotImplemented>";
  }
}

void PrintOperationImpl(const pir::Operation& op,
                        pir::IrPrinter& printer) {  // NOLINT
  if (auto if_op = op.dyn_cast<IfOp>()) {
    if_op.Print(printer);
  } else if (auto while_op = op.dyn_cast<WhileOp>()) {
    while_op.Print(printer);
  } else if (auto pylayer_op = op.dyn_cast<PyLayerOp>()) {
    pylayer_op.Print(printer);
  } else {
    printer.PrintGeneralOperation(op);
  }
}

// OperatorDialect::initialize() is defined in op_dialect_init.cc
// to avoid "file too big" errors from GNU assembler (binutils string table
// 10MB limit).

void OperatorDialect::PrintType(pir::Type type, std::ostream& os) const {
  PrintTypeImpl(type, os);
}

void OperatorDialect::PrintAttribute(pir::Attribute attr,
                                     std::ostream& os) const {
  PrintAttributeImpl(attr, os);
}

pir::OpPrintFn OperatorDialect::PrintOperation(const pir::Operation& op) const {
  if (op.isa<IfOp>() || op.isa<WhileOp>() || op.isa<PyLayerOp>()) {
    return PrintOperationImpl;
  }
  return nullptr;
}

}  // namespace paddle::dialect

IR_DEFINE_EXPLICIT_TYPE_ID(paddle::dialect::OperatorDialect)
