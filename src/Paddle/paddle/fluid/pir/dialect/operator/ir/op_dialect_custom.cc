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

#include "paddle/fluid/pir/dialect/operator/ir/op_dialect.h"
#include "paddle/fluid/framework/custom_operator_utils.h"
#include "paddle/fluid/pir/dialect/operator/interface/vjp.h"
#include "paddle/fluid/pir/dialect/operator/ir/api_builder.h"
#include "paddle/fluid/pir/dialect/operator/ir/manual_api.h"
#include "paddle/fluid/pir/dialect/operator/ir/op_attribute.h"
#include "paddle/fluid/pir/dialect/operator/ir/op_type.h"
#include "paddle/fluid/pir/dialect/operator/trait/inplace.h"
#include "paddle/fluid/pir/dialect/operator/utils/utils.h"
#include "paddle/pir/include/core/builtin_type_interfaces.h"
#include "paddle/pir/include/core/interface_value.h"
#include "paddle/pir/include/core/ir_printer.h"
#include "paddle/phi/infermeta/spmd_rules/rules.h"
#include "paddle/fluid/pir/dialect/distributed/ir/dist_tools.h"
#include "paddle/fluid/pir/dialect/distributed/ir/dist_type.h"

namespace paddle::dialect {

// Shared print helpers (forward declarations from op_dialect.cc are not
// available across TUs, so replicate the minimal set needed here).

static void PrintTypeImpl(pir::Type type, std::ostream& os) {
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

static void PrintAttributeImpl(pir::Attribute attr, std::ostream& os) {
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

// --- IdManager & AttributeManager (used by CustomOpDialect) ---

class IdManager {
 public:
  static IdManager& Instance() {
    static IdManager instance;
    return instance;
  }

  IdManager() : ids_() {}

  ~IdManager() {  // NOLINT
    for (auto id : ids_) {
      delete id;
    }
    ids_.clear();
  }

  pir::TypeId CreateId() {
    pir::detail::UniqueingId* unique_id = new pir::detail::UniqueingId();
    ids_.push_back(unique_id);
    return ids_.back()->id();
  }

 private:
  std::vector<pir::detail::UniqueingId*> ids_;
};

class AttributeManager {
 public:
  static AttributeManager& Instance() {
    static AttributeManager instance;
    return instance;
  }

  AttributeManager() : char_pointers_(), pointers_size_() {}

  ~AttributeManager() {  // NOLINT
    for (size_t i = 0; i < char_pointers_.size(); i++) {
      for (size_t j = 0; j < pointers_size_[i]; j++) {
        delete char_pointers_[i][j];
      }
      delete char_pointers_[i];
    }
    char_pointers_.clear();
    pointers_size_.clear();
  }

  const char** ToCharPointers(const std::vector<std::string>& attr_names) {
    const char** char_pointers = new const char*[attr_names.size()];
    for (size_t i = 0; i < attr_names.size(); i++) {
      const std::string& attr_name = attr_names[i];
      char* ptr = new char[attr_name.size() + 1];
      snprintf(ptr, attr_name.size() + 1, "%s", attr_name.c_str());
      char_pointers[i] = ptr;
    }
    pointers_size_.push_back(attr_names.size());
    char_pointers_.push_back(char_pointers);
    return char_pointers;
  }

 private:
  std::vector<const char**> char_pointers_;
  std::vector<uint32_t> pointers_size_;
};

// --- CustomOpInfoInterfaceModel ---

struct CustomOpInfoInterfaceModel : public OpYamlInfoInterface::Concept {
  static OpInfoTuple GetPirOpInfo(const std::string& pir_op_name) {
    const auto& op_meta =
        paddle::framework::detail::GetOpInfoByPirName(pir_op_name);
    const auto* grad_op_meta_ptr =
        paddle::framework::detail::GetGradOpInfoByFwdPirName(pir_op_name);
    std::vector<paddle::dialect::OpInputInfo> inputs_info;
    std::vector<paddle::dialect::OpAttributeInfo> attributes_info;
    std::vector<paddle::dialect::OpOutputInfo> outputs_info;
    std::vector<std::string> param_names;
    // translate input info
    auto& op_input_names = OpMetaInfoHelper::GetInputs(op_meta);
    for (const auto& input_name : op_input_names) {
      param_names.push_back(input_name);
      bool is_optional = false;
      bool with_grad_semantic = false;
      std::string input_type = "paddle::dialect::DenseTensorType";
      if (paddle::framework::detail::IsOptionalVar(input_name)) {
        is_optional = true;
      }
      if (paddle::framework::detail::IsDuplicableVar(input_name)) {
        input_type = "pir::VectorType<paddle::dialect::DenseTensorType>";
      }
      if (grad_op_meta_ptr) {
        const auto& grad_op_name =
            OpMetaInfoHelper::GetOpName(*grad_op_meta_ptr);
        auto& grad_op_output_names =
            OpMetaInfoHelper::GetOutputs(*grad_op_meta_ptr);
        bool is_double_grad_op =
            (grad_op_name.find(paddle::framework::kDoubleGradSuffix) !=
             grad_op_name.npos)
                ? true
                : false;
        for (auto& grad_op_output_name : grad_op_output_names) {
          auto fwd_input_name = paddle::framework::detail::NoGrad(
              grad_op_output_name, is_double_grad_op);
          if (input_name == fwd_input_name) {
            with_grad_semantic = true;
            break;
          }
        }
      }
      // Now, we only support dense tensor as input.
      inputs_info.push_back(paddle::dialect::OpInputInfo{input_name,
                                                         input_type,
                                                         is_optional,
                                                         false,
                                                         false,
                                                         with_grad_semantic});
    }

    // translate attr info
    auto& op_attrs = OpMetaInfoHelper::GetAttrs(op_meta);
    for (const auto& op_attr : op_attrs) {
      auto attr_name_and_type = paddle::ParseAttrStr(op_attr);
      auto attr_name = attr_name_and_type[0];
      auto attr_type_str = attr_name_and_type[1];
      param_names.push_back(attr_name);
      if (CppTypeToAttrTypeMap().count(attr_type_str) == 0) {
        PADDLE_THROW(common::errors::Unimplemented(
            "Unsupported `%s` type value as custom attribute now. "
            "Supported data types include `bool`, `int`, `float`, "
            "`int64_t`, `std::string`, `std::vector<int>`, "
            "`std::vector<float>`, `std::vector<int64_t>`, "
            "`std::vector<std::string>`, Please check whether "
            "the attribute data type and data type string are matched.",
            attr_type_str));
      }
      std::string attr_pir_type = CppTypeToAttrTypeMap().at(attr_type_str);
      attributes_info.emplace_back(attr_name, attr_pir_type, "");
    }

    // translate output info
    auto& op_output_names = OpMetaInfoHelper::GetOutputs(op_meta);
    for (const auto& output_name : op_output_names) {
      bool is_optional = false;
      if (paddle::framework::detail::IsOptionalVar(output_name)) {
        is_optional = true;
      }
      // Now, we only support dense tensor as output.
      outputs_info.push_back(paddle::dialect::OpOutputInfo{
          output_name, "paddle::dialect::DenseTensorType", is_optional, false});
    }

    auto& inplace_maps = OpMetaInfoHelper::GetInplaceReverseMap(op_meta);

    if (!inplace_maps.empty()) {
      VLOG(3) << "Register Custom Operator: op inplace_map: "
              << string::join_strings(inplace_maps, ',', [](auto& pair) {
                   return pair.first + ": " + pair.second;
                 });
    }

    std::vector<std::pair<std::string, std::string>> vec_inplace;
    for (const auto& inplace_map : inplace_maps) {
      vec_inplace.emplace_back(inplace_map);
    }

    // we only need kernel params name in run_time_info
    paddle::dialect::OpRunTimeInfo run_time_info =
        paddle::dialect::OpRunTimeInfo(
            "", {}, "", param_names, {}, {}, vec_inplace, {});

    return std::make_tuple(
        inputs_info, attributes_info, outputs_info, run_time_info, "");
  }

  CustomOpInfoInterfaceModel() : OpYamlInfoInterface::Concept(GetPirOpInfo) {}
};

// --- CustomOpVjpInterfaceModel ---

struct CustomOpVjpInterfaceModel : public VjpInterface::Concept {
  static std::vector<std::vector<pir::Value>> CustomOpVjp(
      pir::Operation* op,
      const std::vector<std::vector<pir::Value>>& inputs,
      const std::vector<std::vector<pir::Value>>& outputs,
      const std::vector<std::vector<pir::Value>>& out_grads,
      const std::vector<std::vector<bool>>& stop_gradients) {
    std::string pir_op_name = op->name();
    const auto& fwd_op_meta_info =
        paddle::framework::detail::GetOpInfoByPirName(pir_op_name);
    const auto& fwd_inputs_name =
        paddle::OpMetaInfoHelper::GetInputs(fwd_op_meta_info);
    const auto& fwd_attrs_name =
        paddle::OpMetaInfoHelper::GetAttrs(fwd_op_meta_info);
    const auto& fwd_outputs_name =
        paddle::OpMetaInfoHelper::GetOutputs(fwd_op_meta_info);

    const auto* bwd_op_meta_info_ptr =
        paddle::framework::detail::GetGradOpInfoByFwdPirName(pir_op_name);
    if (bwd_op_meta_info_ptr == nullptr) {
      PADDLE_THROW("Custom op : " + pir_op_name + " doesn't support its grad.");
    }
    const auto& bwd_op_meta_info = *bwd_op_meta_info_ptr;
    const auto& bwd_inputs_name =
        paddle::OpMetaInfoHelper::GetInputs(bwd_op_meta_info);
    const auto& bwd_outputs_name =
        paddle::OpMetaInfoHelper::GetOutputs(bwd_op_meta_info);
    const auto& bwd_inplace_map =
        paddle::OpMetaInfoHelper::GetInplaceMap(bwd_op_meta_info);
    const auto& bwd_op_name =
        paddle::OpMetaInfoHelper::GetOpName(bwd_op_meta_info);
    std::string bwd_pir_op_name =
        paddle::framework::kCustomDialectPrefix + bwd_op_name;
    if (!bwd_inplace_map.empty()) {
      // inplace case
      bwd_pir_op_name += "_";
    }
    auto infershape_func = OpMetaInfoHelper::GetInferShapeFn(bwd_op_meta_info);
    auto inferdtype_func = OpMetaInfoHelper::GetInferDtypeFn(bwd_op_meta_info);
    PADDLE_ENFORCE_EQ(
        inputs.size(),
        fwd_inputs_name.size(),
        common::errors::InvalidArgument(
            "Custom op: %s inputs size should be %d, but now is %d.",
            pir_op_name,
            fwd_inputs_name.size(),
            inputs.size()));
    PADDLE_ENFORCE_EQ(
        outputs.size(),
        fwd_outputs_name.size(),
        common::errors::InvalidArgument(
            "Custom op: %s outputs size should be %d, but now is %d.",
            pir_op_name,
            fwd_outputs_name.size(),
            outputs.size()));

    PADDLE_ENFORCE_EQ(
        out_grads.size(),
        fwd_outputs_name.size(),
        common::errors::InvalidArgument(
            "Custom op: %s outputs grad size should be %d, but now is %d.",
            pir_op_name,
            fwd_outputs_name.size(),
            out_grads.size()));
    bool is_double_grad_op =
        (bwd_pir_op_name.find(paddle::framework::kDoubleGradSuffix) !=
         bwd_pir_op_name.npos)
            ? true
            : false;
    pir::IrContext* ctx = pir::IrContext::Instance();
    pir::OpInfo pir_info = ctx->GetRegisteredOpInfo(bwd_pir_op_name);
    pir::OperationArgument argument(pir_info);
    std::vector<pir::Value> argument_inputs;
    std::vector<pir::Type> argument_outputs;

    std::vector<std::vector<int64_t>> input_shapes;
    std::vector<DataType> input_dtypes;
    std::unordered_map<std::string, int> input_name2id_map;
    std::vector<std::vector<std::vector<int64_t>>> vec_input_shapes;
    std::vector<std::vector<DataType>> vec_input_dtypes;
    std::unordered_map<std::string, int> vec_input_name2id_map;
    std::vector<paddle::any> custom_attrs;
    auto GetInputLocation =
        [&](const std::string& grad_op_input_name) -> std::pair<int, int> {
      auto fwd_inputs_name_iter = std::find(
          fwd_inputs_name.begin(), fwd_inputs_name.end(), grad_op_input_name);
      auto fwd_outputs_name_iter = std::find(
          fwd_outputs_name.begin(), fwd_outputs_name.end(), grad_op_input_name);
      bool is_grad_var = paddle::framework::detail::IsGradVar(
          grad_op_input_name, is_double_grad_op);
      if (fwd_inputs_name_iter != fwd_inputs_name.end()) {
        int index =
            std::distance(fwd_inputs_name.begin(), fwd_inputs_name_iter);
        return std::make_pair(0, index);
      } else if (fwd_outputs_name_iter != fwd_outputs_name.end()) {
        int index =
            std::distance(fwd_outputs_name.begin(), fwd_outputs_name_iter);
        return std::make_pair(1, index);
      } else if (is_grad_var) {
        auto fwd_output_name = paddle::framework::detail::NoGrad(
            grad_op_input_name, is_double_grad_op);
        fwd_outputs_name_iter = std::find(
            fwd_outputs_name.begin(), fwd_outputs_name.end(), fwd_output_name);
        if (fwd_outputs_name_iter != fwd_outputs_name.end()) {
          int index =
              std::distance(fwd_outputs_name.begin(), fwd_outputs_name_iter);
          return std::make_pair(2, index);
        } else {
          PADDLE_THROW(common::errors::NotFound(
              "Can't find the grad op input:%s, please check your register "
              "grad op whether has correct input name",
              grad_op_input_name));
        }
      } else {
        PADDLE_THROW(common::errors::NotFound(
            "Can't find the grad op input:%s, please check your register grad "
            "op whether has correct input name",
            grad_op_input_name));
      }
    };
    // Construct custom grad op inputs
    int input_index = 0;
    int vec_input_index = 0;
    for (const auto& bwd_input_name : bwd_inputs_name) {
      const auto input_location = GetInputLocation(bwd_input_name);
      std::vector<pir::Value> input_values;
      if (input_location.first == 0) {
        // grad op input is in inputs
        input_values = inputs[input_location.second];
      } else if (input_location.first == 1) {
        // grad op input is in outputs
        input_values = outputs[input_location.second];
      } else {
        // grad op input is in out_grads
        input_values = out_grads[input_location.second];
      }
      if (paddle::framework::detail::IsDuplicableVar(bwd_input_name)) {
        std::vector<std::vector<int64_t>> tmp_input_shapes;
        std::vector<phi::DataType> tmp_input_dtypes;
        pir::Value input_value;
        vec_input_name2id_map[bwd_input_name] = vec_input_index;
        vec_input_index++;
        bool is_optional =
            (input_values.size() == 1 && input_values[0].impl() == nullptr);
        if (!is_optional) {
          for (auto& input_value : input_values) {
            paddle::dialect::DenseTensorType input_tensor =
                input_value.type().dyn_cast<paddle::dialect::DenseTensorType>();
            tmp_input_shapes.push_back(phi::vectorize(input_tensor.dims()));
            tmp_input_dtypes.push_back(
                paddle::dialect::TransToPhiDataType(input_tensor.dtype()));
          }
          input_value = paddle::dialect::builtin_combine(input_values);
        }
        vec_input_shapes.push_back(tmp_input_shapes);
        vec_input_dtypes.push_back(tmp_input_dtypes);
        argument_inputs.push_back(input_value);
      } else {
        std::vector<int64_t> tmp_input_shape;
        phi::DataType tmp_input_dtype = DataType::UNDEFINED;
        input_name2id_map[bwd_input_name] = input_index;
        input_index++;
        pir::Value input_value = input_values[0];  // NOLINT
        if (input_value.impl() != nullptr) {
          paddle::dialect::DenseTensorType input_tensor =
              input_value.type().dyn_cast<paddle::dialect::DenseTensorType>();
          tmp_input_shape = phi::vectorize(input_tensor.dims());
          tmp_input_dtype =
              paddle::dialect::TransToPhiDataType(input_tensor.dtype());
        }
        input_shapes.push_back(tmp_input_shape);
        input_dtypes.push_back(tmp_input_dtype);

        argument_inputs.push_back(input_value);
      }
    }
    argument.AddInputs(argument_inputs);
    // Construct custom grad op attr
    for (const auto& fwd_attr : fwd_attrs_name) {
      std::vector<std::string> attr_name_and_type =
          paddle::ParseAttrStr(fwd_attr);
      auto fwd_attr_name = attr_name_and_type[0];
      auto fwd_op_attr = op->attribute(fwd_attr_name);
      custom_attrs.push_back(paddle::dialect::TransAttrToAny(fwd_op_attr));
      argument.AddAttribute(fwd_attr_name, fwd_op_attr);
    }
    // Run Compile InferMeta
    std::vector<std::vector<int64_t>> output_shapes =
        paddle::framework::RunInferShape(infershape_func,
                                         bwd_op_meta_info,
                                         input_shapes,
                                         input_name2id_map,
                                         vec_input_shapes,
                                         vec_input_name2id_map,
                                         custom_attrs);
    std::vector<phi::DataType> output_dtypes =
        paddle::framework::RunInferDtype(inferdtype_func,
                                         bwd_op_meta_info,
                                         input_dtypes,
                                         input_name2id_map,
                                         vec_input_dtypes,
                                         vec_input_name2id_map,
                                         custom_attrs);
    dialect::ProcessMeshAttribute op_mesh;
    bool run_auto_parallel = false;
    std::vector<pir::Attribute> dist_result_attrs;
    phi::distributed::SpmdInfo spmd_info;
    if (dialect::HasDistInput(argument_inputs, &op_mesh)) {
      VLOG(7) << "Custom Grad Op: " << pir_op_name << " InferSPMD";
      run_auto_parallel = true;
      spmd_info = paddle::framework::RunInferSpmd(bwd_op_meta_info,
                                                  pir_op_name,
                                                  op_mesh,
                                                  argument_inputs,
                                                  custom_attrs);
    }

    size_t all_values_num = 0;
    // output name -> value num (that output should hold)
    std::unordered_map<std::string, size_t> output_name2value_num;
    for (const auto& bwd_output_name : bwd_outputs_name) {
      const auto& bwd_input =
          paddle::framework::detail::NoGrad(bwd_output_name, is_double_grad_op);

      if (paddle::framework::detail::IsDuplicableVar(bwd_output_name)) {
        auto index = vec_input_name2id_map[bwd_input];
        auto& vec_input_shape = vec_input_shapes[index];
        output_name2value_num[bwd_output_name] = vec_input_shape.size();
      } else {
        auto index = input_name2id_map[bwd_input];
        // input_shapes[index] is dim of tensor, if the dim doesn't have
        // element, it must be a optional tensor that is None in custom operator
        output_name2value_num[bwd_output_name] =
            input_shapes[index].size() == 0 ? 0 : 1;
      }
      all_values_num += output_name2value_num[bwd_output_name];
    }

    PADDLE_ENFORCE_EQ(
        output_shapes.size(),
        all_values_num,
        common::errors::InvalidArgument(
            "The number of output shapes after running custom operator's "
            "InferShapeFunc is wrong, "
            "expected contains %d Tensors' shape, but actually contains %d "
            "Tensors' shape",
            all_values_num,
            output_shapes.size()));

    PADDLE_ENFORCE_EQ(
        output_dtypes.size(),
        all_values_num,
        common::errors::InvalidArgument(
            "The number of output dtypes after running custom operator's "
            "InferDtypeFunc is wrong, "
            "expected contains %d Tensors' dtype, but actually contains %d "
            "Tensors' dtype",
            all_values_num,
            output_dtypes.size()));

    if (run_auto_parallel) {
      PADDLE_ENFORCE_EQ(
          spmd_info.second.size(),
          all_values_num,
          common::errors::InvalidArgument(
              "The number of output dist_attr after running custom operator's "
              "InferSPMD is wrong, "
              "expected contains %d Tensors' dist_attr, but actually contains "
              "%d "
              "Tensors' dist_attr",
              all_values_num,
              spmd_info.second.size()));
    }

    // Construct custom grad op outputs
    size_t value_index = 0;
    for (const auto& bwd_output_name : bwd_outputs_name) {
      auto value_num = output_name2value_num[bwd_output_name];
      if (value_num == 0) {
        // Optional value condition
        pir::Type out_type;
        argument_outputs.push_back(out_type);
        continue;
      }
      if (paddle::framework::detail::IsDuplicableVar(bwd_output_name)) {
        std::vector<pir::Type> out_types;
        std::vector<pir::Attribute> dist_attrs;
        for (size_t j = 0; j < value_num; ++j) {
          auto ddims = phi::make_ddim(output_shapes[value_index]);
          auto dtype = output_dtypes[value_index];
          phi::DataLayout layout{DataLayout::NCHW};
          phi::LegacyLoD lod;
          auto type = paddle::dialect::DenseTensorType::get(
              pir::IrContext::Instance(),
              paddle::dialect::TransToIrDataType(dtype),
              ddims,
              layout,
              lod,
              0);
          if (run_auto_parallel) {
            auto dist_attr =
                dialect::CvtToPirAttr(spmd_info.second[value_index]);
            out_types.push_back(dialect::CvtToPirDistType(type, dist_attr));
            dist_attrs.push_back(dist_attr);
          } else {
            out_types.push_back(std::move(type));
          }
          value_index++;
        }
        pir::Type out_vector_type =
            pir::VectorType::get(pir::IrContext::Instance(), out_types);
        argument_outputs.push_back(out_vector_type);
        if (run_auto_parallel) {
          dist_result_attrs.push_back(
              pir::ArrayAttribute::get(pir::IrContext::Instance(), dist_attrs));
        }
      } else {
        auto ddims = phi::make_ddim(output_shapes[value_index]);
        auto dtype = output_dtypes[value_index];
        phi::DataLayout layout{DataLayout::NCHW};
        phi::LegacyLoD lod;
        auto out_type = paddle::dialect::DenseTensorType::get(
            pir::IrContext::Instance(),
            paddle::dialect::TransToIrDataType(dtype),
            ddims,
            layout,
            lod,
            0);
        if (run_auto_parallel) {
          auto dist_attr = dialect::CvtToPirAttr(spmd_info.second[value_index]);
          argument_outputs.push_back(
              dialect::CvtToPirDistType(out_type, dist_attr));
          dist_result_attrs.push_back(dist_attr);
        } else {
          argument_outputs.push_back(out_type);
        }
        value_index++;
      }
    }
    argument.AddOutputs(argument_outputs.begin(), argument_outputs.end());

    // construct operator_dist_attr
    if (run_auto_parallel) {
      std::vector<pir::Attribute> dist_operand_attrs;
      for (auto& arg_dist : spmd_info.first) {
        dist_operand_attrs.push_back(dialect::CvtToPirAttr(arg_dist));
      }
      auto op_dist_attr = dialect::OperationDistAttribute::get(
          ctx, op_mesh, dist_operand_attrs, dist_result_attrs);
      std::ostringstream print_stream;
      print_stream << op_dist_attr;
      VLOG(7) << "Custom Op: " << pir_op_name
              << " InferSPMD Operator dist attr: " << print_stream.str();
      argument.AddAttribute(
          kAttrOpDistAttr,
          dialect::OperationDistAttribute::get(
              ctx, op_mesh, dist_operand_attrs, dist_result_attrs));
    }
    // Build Operation
    std::vector<pir::Value> op_results;
    pir::Operation* bwd_op =
        paddle::dialect::ApiBuilder::Instance().GetBuilder()->Build(
            std::move(argument));

    // Init result
    std::vector<std::vector<pir::Value>> res;
    res.resize(stop_gradients.size());
    for (size_t i = 0; i < stop_gradients.size(); ++i) {
      res[i].resize(stop_gradients[i].size());
    }

    auto GetInputGradientIndex = [&](const std::string& bwd_output_name,
                                     bool is_double_grad_op) -> size_t {
      size_t gradient_vec_index = 0;
      const auto& fwd_input =
          paddle::framework::detail::NoGrad(bwd_output_name, is_double_grad_op);
      auto fwd_inputs_name_iter =
          std::find(fwd_inputs_name.begin(), fwd_inputs_name.end(), fwd_input);
      size_t input_index =
          std::distance(fwd_inputs_name.begin(), fwd_inputs_name_iter);
      for (size_t i = 0; i < input_index; ++i) {
        for (const auto& bwd_output_name : bwd_outputs_name) {
          const auto& fwd_input_name_tmp = paddle::framework::detail::NoGrad(
              bwd_output_name, is_double_grad_op);
          if (fwd_input_name_tmp == fwd_inputs_name[i]) {
            gradient_vec_index++;
            break;
          }
        }
      }
      return gradient_vec_index;
    };

    // Build result and apply stop gradients
    for (size_t i = 0; i < bwd_outputs_name.size(); ++i) {
      const auto& bwd_output_name = bwd_outputs_name.at(i);
      const auto& fwd_input =
          paddle::framework::detail::NoGrad(bwd_output_name, is_double_grad_op);
      auto fwd_inputs_name_iter =
          std::find(fwd_inputs_name.begin(), fwd_inputs_name.end(), fwd_input);
      if (paddle::framework::detail::IsDuplicableVar(bwd_output_name)) {
        PADDLE_ENFORCE_NE(
            fwd_inputs_name_iter,
            fwd_inputs_name.end(),
            common::errors::InvalidArgument(
                "Custom op: %s output %s is a Vec output. It should have the "
                "forward input that need calculate gradients.",
                pir_op_name,
                bwd_output_name));
        int index = GetInputGradientIndex(bwd_output_name, is_double_grad_op);
        if (bwd_op->result(i).type().dyn_cast<pir::VectorType>()) {
          auto split_op =
              ApiBuilder::Instance().GetBuilder()->Build<pir::SplitOp>(
                  bwd_op->result(i));
          res[index] = split_op.outputs();
        } else {
          // optional output condition
          pir::Value empty_value;
          res[index][0] = empty_value;
        }
      } else {
        if (fwd_inputs_name_iter != fwd_inputs_name.end()) {
          int index = GetInputGradientIndex(bwd_output_name, is_double_grad_op);
          res[index][0] = bwd_op->result(i);
        } else {
          res[0][0] = bwd_op->result(0);
        }
      }
    }
    return res;
  }

  CustomOpVjpInterfaceModel() : VjpInterface::Concept(CustomOpVjp) {}
};

// --- CustomOpDialect ---

CustomOpDialect::CustomOpDialect(pir::IrContext* context)
    : pir::Dialect(name(), context, pir::TypeId::get<CustomOpDialect>()) {}

void CustomOpDialect::PrintType(pir::Type type, std::ostream& os) const {
  PrintTypeImpl(type, os);
}

void CustomOpDialect::PrintAttribute(pir::Attribute attr,
                                     std::ostream& os) const {
  PrintAttributeImpl(attr, os);
}

pir::OpPrintFn CustomOpDialect::PrintOperation(const pir::Operation& op) const {
  return nullptr;
}

void CustomOpDialect::RegisterCustomOp(const paddle::OpMetaInfo& op_meta) {
  pir::TypeId id = IdManager::Instance().CreateId();
  std::string op_name = paddle::framework::kCustomDialectPrefix +
                        OpMetaInfoHelper::GetOpName(op_meta);
  std::vector<pir::TypeId> traits;

  auto& inplace_map = OpMetaInfoHelper::GetInplaceMap(op_meta);
  if (!inplace_map.empty()) {
    op_name += "_";
    traits.push_back(pir::TypeId::get<paddle::dialect::InplaceTrait>());
  }

  char* op_name_c = new char[op_name.size() + 1];
  snprintf(op_name_c, op_name.size() + 1, "%s", op_name.c_str());
  op_names_.push_back(op_name_c);

  auto& op_attrs = OpMetaInfoHelper::GetAttrs(op_meta);
  std::vector<std::string> attr_names;
  for (const auto& op_attr : op_attrs) {
    auto attr_name_and_type = paddle::ParseAttrStr(op_attr);
    auto attr_name = attr_name_and_type[0];
    attr_names.push_back(attr_name);
  }
  const char** attr_name =
      AttributeManager::Instance().ToCharPointers(attr_names);
  uint32_t attr_num = attr_names.size();

  std::set<pir::InterfaceValue> interface_values;
  pir::InterfaceValue op_info_interface =
      pir::InterfaceValue::Get<OpYamlInfoInterface,
                               CustomOpInfoInterfaceModel>();
  interface_values.insert(std::move(op_info_interface));

  if (paddle::framework::detail::HasGradOp(op_name)) {
    pir::InterfaceValue vjp_interface =
        pir::InterfaceValue::Get<VjpInterface, CustomOpVjpInterfaceModel>();
    interface_values.insert(std::move(vjp_interface));
  }

  pir::VerifyPtr verify_func = [](pir::Operation* op) {};
  ir_context()->RegisterOpInfo(this,
                               id,
                               op_names_.back(),
                               std::move(interface_values),
                               traits,
                               attr_num,
                               attr_name,
                               verify_func,
                               verify_func);
}

// --- CustomEngineDialect ---

CustomEngineDialect::CustomEngineDialect(pir::IrContext* context)
    : pir::Dialect(name(), context, pir::TypeId::get<CustomEngineDialect>()) {}

void CustomEngineDialect::PrintType(pir::Type type, std::ostream& os) const {
  PrintTypeImpl(type, os);
}

void CustomEngineDialect::PrintAttribute(pir::Attribute attr,
                                         std::ostream& os) const {
  PrintAttributeImpl(attr, os);
}

pir::OpPrintFn CustomEngineDialect::PrintOperation(
    const pir::Operation& op) const {
  return nullptr;
}

}  // namespace paddle::dialect

IR_DEFINE_EXPLICIT_TYPE_ID(paddle::dialect::CustomOpDialect)
IR_DEFINE_EXPLICIT_TYPE_ID(paddle::dialect::CustomEngineDialect)
