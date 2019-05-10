// Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#pragma once

#include <NvInfer.h>
#include <core/session/onnxruntime_c_api.h>
#include "src/core/backend.h"
#include "src/core/model_config.pb.h"
#include "src/core/scheduler.h"
#include "src/core/status.h"

namespace nvidia { namespace inferenceserver {

class OnnxBackend : public InferenceBackend {
 public:
  OnnxBackend() = default;
  OnnxBackend(OnnxBackend&&) = default;

  Status Init(const std::string& path, const ModelConfig& config);

  // Create a context for execution for each instance for the
  // serialized plans specified in 'models'.
  Status CreateExecutionContexts(
      OrtEnv* env, const std::unordered_map<std::string, std::string>& paths);
  Status CreateExecutionContext(
      const std::string& instance_name, const int gpu_device, OrtEnv* env,
      OrtSessionOptions* base_session_options,
      const std::unordered_map<std::string, std::string>& paths);

 private:
  // Run model on the context associated with 'runner_idx' to
  // execute for one or more requests.
  void Run(
      uint32_t runner_idx, std::vector<Scheduler::Payload>* payloads,
      std::function<void(Status)> OnCompleteQueuedPayloads);

 private:
  DISALLOW_COPY_AND_ASSIGN(OnnxBackend);
  friend std::ostream& operator<<(std::ostream&, const OnnxBackend&);

  // For each model instance there is a context.
  struct Context {
    // GPU device number that indicates that no gpu is available for a
    // context (which is an invalid state since TensorRT requires a
    // GPU).
    static constexpr int NO_GPU_DEVICE = -1;

    // Max batch size value that indicates batching is not supported.
    static constexpr int NO_BATCHING = 0;

    Context(
        const std::string& name, const int gpu_device,
        const int max_batch_size);
    ~Context();

    DISALLOW_MOVE(Context);
    DISALLOW_COPY_AND_ASSIGN(Context);

    Status ValidateInputs(
        const ::google::protobuf::RepeatedPtrField<ModelInput>& ios);
    Status ValidateOutputs(
        const ::google::protobuf::RepeatedPtrField<ModelOutput>& ios);

    // Run model to execute for one or more requests. This function
    // assumes that it is only called by the single runner thread that
    // is assigned to this context. A non-OK return status indicates
    // an internal error that prevents any of the of requests from
    // completing. If an error is isolate to a single request payload
    // it will be reported in that payload.
    Status Run(
        const OnnxBackend* base, std::vector<Scheduler::Payload>* payloads);

    // Set an input tensor from one or more payloads.
    Status SetInputTensor(
        const std::string& name, const DataType datatype, const DimsList& dims,
        size_t total_batch_size, std::vector<Scheduler::Payload>* payloads,
        std::vector<std::unique_ptr<char[]>>* input_buffers,
        std::vector<const char*>* input_names);

    // Read output tensors into one or more payloads accordingly.
    Status ReadOutputTensors(
        const OnnxBackend* base, size_t total_batch_size,
        const std::vector<const char*>& output_names,
        std::vector<Scheduler::Payload>* payloads);

    // Release the Onnx Runtime resources allocated for the run, if any.
    void ReleaseOrtRunResources();

    // Name of the model instance
    std::string name_;

    // The GPU index active when this context was created.
    int gpu_device_;

    // Maximum batch size to allow. This is the minimum of what is
    // supported by the model and what is requested in the
    // configuration.
    int max_batch_size_;

    // Onnx Runtime variables that are used across runs
    OrtSession* session_;
    OrtAllocatorInfo* allocator_info_;

    // Onnx Runtime variables that will be reset and used for every run
    std::vector<OrtValue*> input_tensors_;
    std::vector<OrtValue*> output_tensors_;
  };

  std::vector<std::unique_ptr<Context>> contexts_;
};

std::ostream& operator<<(std::ostream& out, const OnnxBackend& pb);

}}  // namespace nvidia::inferenceserver
