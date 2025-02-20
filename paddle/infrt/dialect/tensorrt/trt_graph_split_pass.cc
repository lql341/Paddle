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

#include "paddle/infrt/dialect/tensorrt/trt_graph_split_pass.h"

#include <mlir/IR/Builders.h>
#include "paddle/infrt/dialect/pd/ir/pd_ops.h"

namespace infrt {
namespace trt {
// Implementation of the trtGraphSplitPass。
void TRTGraphSplitPass::runOnFunction() {
  std::vector<infrt::pd::GraphOp> worklist;
  mlir::Block& block = getFunction().front();
  for (auto& op : block) {
    infrt::pd::GraphOp graph_op =
        ::llvm::dyn_cast_or_null<infrt::pd::GraphOp>(&op);
    if (nullptr != graph_op &&
        graph_op.getBody()->getOperations().size() <= min_subgraph_size_) {
      worklist.push_back(graph_op);
    }
  }
  while (!worklist.empty()) {
    infrt::pd::GraphOp graph_op = worklist.back();
    worklist.pop_back();
    mlir::Block* body = graph_op.getBody();
    auto return_op = body->getTerminator();
    graph_op.replaceAllUsesWith(return_op->getOperands());
    auto copy_range = body->without_terminator();
    block.getOperations().splice(mlir::Block::iterator(graph_op),
                                 body->getOperations(),
                                 copy_range.begin(),
                                 copy_range.end());
    graph_op.erase();
  }
}
}  // namespace trt
}  // namespace infrt
