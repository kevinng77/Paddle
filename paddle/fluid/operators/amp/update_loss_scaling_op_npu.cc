/* Copyright (c) 2021 PaddlePaddle Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#include <cmath>
#include <vector>

#include "paddle/fluid/framework/data_type.h"
#include "paddle/fluid/framework/op_registry.h"
#include "paddle/fluid/operators/amp/fp16_type_traits.h"
#include "paddle/fluid/platform/device/npu/npu_op_runner.h"

DECLARE_int32(min_loss_scaling);

namespace paddle {
namespace operators {

using Tensor = phi::DenseTensor;

template <typename T>
void Update(const platform::NPUDeviceContext& ctx,
            const std::vector<bool> found_inf_vec,
            const phi::DenseTensor* pre_loss_scaling_tensor,
            const phi::DenseTensor* good_in_tensor,
            const phi::DenseTensor* bad_in_tensor,
            const int incr_every_n_steps,
            const int decr_every_n_nan_or_inf,
            const float incr_ratio,
            const float decr_ratio,
            phi::DenseTensor* updated_loss_scaling_tensor,
            phi::DenseTensor* good_out_tensor,
            phi::DenseTensor* bad_out_tensor) {
  auto place = ctx.GetPlace();
  auto stream = ctx.stream();
  if (found_inf_vec[0]) {
    // good_out_data = 0
    auto g = good_out_tensor->mutable_data<int>(place);
    platform::NPUMemsetAsync(static_cast<void*>(g),
                             0,
                             good_out_tensor->numel() * sizeof(int),
                             stream);
    // bad_out_data = bad_in_data + 1
    Tensor factor_tensor(bad_out_tensor->dtype());
    factor_tensor.mutable_data<int>({1}, place);
    FillNpuTensorWithConstant<int>(&factor_tensor, static_cast<int>(1));
    const auto& runner_p2 = NpuOpRunner(
        "Add", {*bad_in_tensor, factor_tensor}, {*bad_out_tensor}, {});
    runner_p2.Run(stream);

    std::vector<int> bad_out_data;
    paddle::framework::TensorToVector(*bad_out_tensor, ctx, &bad_out_data);
    if (bad_out_data[0] >= decr_every_n_nan_or_inf) {
      const auto& runner_p3 = NpuOpRunner("Power",
                                          {*pre_loss_scaling_tensor},
                                          {*updated_loss_scaling_tensor},
                                          {{"power", static_cast<float>(1)},
                                           {"scale", decr_ratio},
                                           {"shift", static_cast<float>(0)}});

      runner_p3.Run(stream);

      std::vector<T> new_loss_scaling;
      paddle::framework::TensorToVector(
          *updated_loss_scaling_tensor, ctx, &new_loss_scaling);
      float min_value = 1.0;
      if (FLAGS_min_loss_scaling > 1) {
        min_value = static_cast<float>(FLAGS_min_loss_scaling);
      }

      if (new_loss_scaling[0] < min_value) {
        // updated_loss_scaling_data = 1
        const auto& runner_p4 =
            NpuOpRunner("Power",
                        {*pre_loss_scaling_tensor},
                        {*updated_loss_scaling_tensor},
                        {{"power", static_cast<float>(1)},
                         {"scale", static_cast<float>(0)},
                         {"shift", static_cast<float>(min_value)}});

        runner_p4.Run(stream);
      }

      // bad_out_data = 0
      auto b = bad_out_tensor->mutable_data<int>(place);
      platform::NPUMemsetAsync(static_cast<void*>(b),
                               0,
                               bad_out_tensor->numel() * sizeof(int),
                               stream);
    }
  } else {
    // bad_out_data = 0
    auto b = bad_out_tensor->mutable_data<int>(place);
    platform::NPUMemsetAsync(static_cast<void*>(b),
                             0,
                             bad_out_tensor->numel() * sizeof(int),
                             stream);

    // good_out_data = good_in_data + 1
    Tensor factor_tensor(good_out_tensor->dtype());
    factor_tensor.mutable_data<int>({1}, place);
    FillNpuTensorWithConstant<int>(&factor_tensor, static_cast<int>(1));
    const auto& runner_p2 = NpuOpRunner(
        "Add", {*good_in_tensor, factor_tensor}, {*good_out_tensor}, {});
    runner_p2.Run(stream);

    std::vector<int> good_out_data;
    paddle::framework::TensorToVector(*good_out_tensor, ctx, &good_out_data);

    if (good_out_data[0] >= incr_every_n_steps) {
      const auto& runner_p3 = NpuOpRunner("Power",
                                          {*pre_loss_scaling_tensor},
                                          {*updated_loss_scaling_tensor},
                                          {{"power", static_cast<float>(1)},
                                           {"scale", incr_ratio},
                                           {"shift", static_cast<float>(0)}});
      runner_p3.Run(stream);

      std::vector<T> new_loss_scaling;
      paddle::framework::TensorToVector(
          *updated_loss_scaling_tensor, ctx, &new_loss_scaling);
      if (!std::isfinite(new_loss_scaling[0])) {
        // updated_loss_scaling_data = pre_loss_scaling_data
        const auto& runner_p4 = NpuOpRunner("Power",
                                            {*pre_loss_scaling_tensor},
                                            {*updated_loss_scaling_tensor},
                                            {{"power", static_cast<float>(1)},
                                             {"scale", static_cast<float>(1)},
                                             {"shift", static_cast<float>(0)}});

        runner_p4.Run(stream);
      }
      // good_out_data = 0
      auto g = good_out_tensor->mutable_data<int>(place);
      platform::NPUMemsetAsync(static_cast<void*>(g),
                               0,
                               good_out_tensor->numel() * sizeof(int),
                               stream);
    }
  }
}

template <typename T>
class UpdateLossScalingFunctor {
 public:
  void operator()(const platform::NPUDeviceContext& dev_ctx,
                  const std::vector<bool> found_inf_vec,
                  const phi::DenseTensor* pre_loss_scaling_tensor,
                  const phi::DenseTensor* good_in_tensor,
                  const phi::DenseTensor* bad_in_tensor,
                  const int incr_every_n_steps,
                  const int decr_every_n_nan_or_inf,
                  const float incr_ratio,
                  const float decr_ratio,
                  phi::DenseTensor* updated_loss_scaling_tensor,
                  phi::DenseTensor* good_out_tensor,
                  phi::DenseTensor* bad_out_tensor) const {
    Update<T>(dev_ctx,
              found_inf_vec,
              pre_loss_scaling_tensor,
              good_in_tensor,
              bad_in_tensor,
              incr_every_n_steps,
              decr_every_n_nan_or_inf,
              incr_ratio,
              decr_ratio,
              updated_loss_scaling_tensor,
              good_out_tensor,
              bad_out_tensor);
  }
};

template <typename T>
class LazyZerosNPU {
 public:
  void operator()(const platform::NPUDeviceContext& dev_ctx,
                  const std::vector<bool> found_inf_vec,
                  const std::vector<const phi::DenseTensor*>& xs,
                  const std::vector<phi::DenseTensor*>& outs) const {
    if (!xs.size()) {
      return;
    }
    auto place = dev_ctx.GetPlace();
    auto stream = dev_ctx.stream();
    phi::DenseTensor* zero_tensor = nullptr;
    void* zero_ptr = nullptr;
    if (found_inf_vec[0]) {
      int max_num = -1;
      for (size_t i = 0; i < xs.size(); ++i) {
        auto* out = outs[i];
        int num = out->numel();
        if (max_num < num) {
          max_num = num;
          zero_tensor = out;
        }
      }

      zero_tensor->mutable_data<T>(place);
      const auto& runner_zeros =
          NpuOpRunner("ZerosLike", {*zero_tensor}, {*zero_tensor});
      runner_zeros.Run(stream);
      zero_tensor->check_memory_size();
      zero_ptr = zero_tensor->data();
    }

    for (size_t i = 0; i < xs.size(); ++i) {
      auto* out = outs[i];
      auto* x = xs[i];
      auto dst_ptr = out->mutable_data<T>(place);
      if (!found_inf_vec[0]) {
        framework::TensorCopy(*x, place, dev_ctx, out);
      } else if (zero_ptr != dst_ptr) {
        auto size = out->numel() * phi::SizeOf(out->dtype());
        memory::Copy(place, dst_ptr, place, zero_ptr, size, stream);
      }
    }
  }
};

template <typename DeviceContext, typename T>
class UpdateLossScalingNPUKernel : public framework::OpKernel<T> {
  using MPDType = typename details::MPTypeTrait<T>::Type;

 public:
  void Compute(const framework::ExecutionContext& ctx) const override {
    auto& dev_ctx = ctx.template device_context<DeviceContext>();

    const auto xs = ctx.MultiInput<phi::DenseTensor>("X");
    auto outs = ctx.MultiOutput<phi::DenseTensor>("Out");
    const auto* found_inf = ctx.Input<phi::DenseTensor>("FoundInfinite");
    PADDLE_ENFORCE_EQ(found_inf->numel(),
                      1,
                      platform::errors::InvalidArgument(
                          "FoundInfinite must has only one element."));

    std::vector<bool> found_inf_vec;
    paddle::framework::TensorToVector(
        *found_inf, ctx.device_context(), &found_inf_vec);

    LazyZerosNPU<T>{}(dev_ctx, found_inf_vec, xs, outs);
    const bool stop_update = ctx.Attr<bool>("stop_update");
    if (stop_update) {
      return;
    }

    const auto* pre_loss_scaling =
        ctx.Input<phi::DenseTensor>("PrevLossScaling");
    const auto* good_in = ctx.Input<phi::DenseTensor>("InGoodSteps");
    const auto* bad_in = ctx.Input<phi::DenseTensor>("InBadSteps");
    auto* updated_loss_scaling = ctx.Output<phi::DenseTensor>("LossScaling");
    auto* good_out = ctx.Output<phi::DenseTensor>("OutGoodSteps");
    auto* bad_out = ctx.Output<phi::DenseTensor>("OutBadSteps");

    updated_loss_scaling->mutable_data<MPDType>(dev_ctx.GetPlace());
    good_out->mutable_data<int>(dev_ctx.GetPlace());
    bad_out->mutable_data<int>(dev_ctx.GetPlace());

    const int incr_every_n_steps = ctx.Attr<int>("incr_every_n_steps");
    const int decr_every_n_nan_or_inf =
        ctx.Attr<int>("decr_every_n_nan_or_inf");
    const float incr_ratio = ctx.Attr<float>("incr_ratio");
    const float decr_ratio = ctx.Attr<float>("decr_ratio");
    UpdateLossScalingFunctor<MPDType>{}(dev_ctx,
                                        found_inf_vec,
                                        pre_loss_scaling,
                                        good_in,
                                        bad_in,
                                        incr_every_n_steps,
                                        decr_every_n_nan_or_inf,
                                        incr_ratio,
                                        decr_ratio,
                                        updated_loss_scaling,
                                        good_out,
                                        bad_out);
  }
};

}  // namespace operators
}  // namespace paddle

namespace ops = paddle::operators;

REGISTER_OP_NPU_KERNEL(
    update_loss_scaling,
    ops::UpdateLossScalingNPUKernel<paddle::platform::NPUDeviceContext, float>,
    ops::UpdateLossScalingNPUKernel<paddle::platform::NPUDeviceContext,
                                    double>);
