// Copyright (c) 2021 PaddlePaddle Authors. All Rights Reserved.
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

#pragma once

#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "paddle/fluid/framework/executor_gc_helper.h"
#include "paddle/fluid/framework/garbage_collector.h"
#include "paddle/fluid/framework/new_executor/interpreter/execution_config.h"
#include "paddle/fluid/framework/new_executor/new_executor_defs.h"
#include "paddle/fluid/framework/new_executor/workqueue/workqueue.h"
#include "paddle/fluid/framework/new_executor/workqueue/workqueue_utils.h"
#include "paddle/fluid/framework/op_info.h"
#include "paddle/fluid/framework/op_registry.h"
#include "paddle/fluid/framework/operator.h"
#include "paddle/fluid/framework/program_desc.h"
#include "paddle/fluid/framework/scope.h"
#include "paddle/fluid/framework/tensor.h"
#include "paddle/fluid/framework/variable.h"
#include "paddle/fluid/framework/variable_helper.h"
#include "paddle/fluid/platform/device_context.h"
#include "paddle/fluid/platform/init.h"

using AtomicVectorSizeT = std::vector<std::atomic<size_t>>;

namespace paddle {
namespace framework {
namespace interpreter {
class AsyncWorkQueue {
 public:
  AsyncWorkQueue(size_t host_num_threads,
                 size_t deivce_num_threads,
                 EventsWaiter* waiter);

  // void WaitEmpty() { queue_group_->WaitQueueGroupEmpty(); }

  void AddTask(const OpFuncType& op_func_type, std::function<void()> fn);

  void Cancel() { queue_group_->Cancel(); }

  size_t QueueNumThreads(size_t idx) {
    return queue_group_->QueueNumThreads(idx);
  }

 private:
  size_t host_num_thread_;
  std::unique_ptr<WorkQueueGroup> queue_group_;
};

bool IsCommunicationOp(const std::string& op_name);

bool IsCommunicationOp(const Instruction& instr);

bool IsCpuOp(const Instruction& instr);

bool IsMemcpyD2H(const Instruction& instr);

bool IsMemcpyH2D(const Instruction& instr);

bool IsMemcpyOp(const Instruction& instr);

bool IsSupportedHeterPlace(const phi::Place& place);

void AddFetch(const std::vector<std::string>& fetch_names,
              framework::BlockDesc* block);

void BuildOpFuncList(const platform::Place& place,
                     const framework::BlockDesc& block,
                     const std::set<std::string>& skip_gc_vars,
                     std::vector<OpFuncNode>* vec_func_list,
                     VariableScope* scope,
                     const ExecutionConfig& execution_config,
                     bool use_local_scope = true);

void BuildVariableScope(const framework::BlockDesc& block,
                        VariableScope* var_scope,
                        bool use_local_scope = true);

void LogDeviceMemoryStats(const platform::Place& place);

}  // namespace interpreter
}  // namespace framework
}  // namespace paddle
