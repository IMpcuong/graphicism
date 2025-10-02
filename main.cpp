#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION

#include <iostream>

#include "Metal.hpp"

const char *mtl_shader = R"(
#include <metal_stdlib>

using namespace metal;

kernel void add_same_sz_arr_pair(
    device const float *f [[buffer(0)]],
    device const float *s [[buffer(1)]],
    device float *r [[buffer(2)]],
    uint id [[thread_position_in_grid]])
{
  r[id] = f[id] + s[id];
}
)";

int main()
{
  NS::SharedPtr<MTL::Device> mtl_dev = NS::TransferPtr(MTL::CreateSystemDefaultDevice());
  NS::UInteger alloc_sz = mtl_dev->currentAllocatedSize();
  std::cout << "alloc_sz = " << alloc_sz << "\n";
  MTL::CommandQueue *mtl_cmd_queue = mtl_dev->newCommandQueue();

  // ----- //

  NS::Error *mtl_err = nullptr;
  NS::String *lib_name = NS::String::string(mtl_shader, NS::UTF8StringEncoding);
  MTL::Library *mtl_lib = mtl_dev->newLibrary(lib_name, nullptr /*options=*/, &mtl_err);
  NS::String *func_name = NS::String::string("add_same_sz_arr_pair", NS::UTF8StringEncoding);
  MTL::Function *mtl_func = mtl_lib->newFunction(func_name);
  MTL::ComputePipelineState *mtl_pstate = mtl_dev->newComputePipelineState(mtl_func, &mtl_err);

  // ----- //

  const int arr_sz = 1024;
  const int real_sz = sizeof(float) * arr_sz;

  MTL::Buffer *buf_f = mtl_dev->newBuffer(real_sz, MTL::ResourceStorageModeShared);
  MTL::Buffer *buf_s = mtl_dev->newBuffer(real_sz, MTL::ResourceStorageModeShared);
  MTL::Buffer *buf_r = mtl_dev->newBuffer(real_sz, MTL::ResourceStorageModeShared);

  float *f = static_cast<float *>(buf_f->contents()); // typeof(buf_f->contents()) := <void *>
  float *s = static_cast<float *>(buf_s->contents());
  for (int i = 0; i < arr_sz; i++)
  {
    f[i] = i;
    s[i] = i * 7;
  }

  // ----- //

  MTL::CommandBuffer *mtl_cmd_buf = mtl_cmd_queue->commandBuffer();
  MTL::ComputeCommandEncoder *mtl_cmd_enc = mtl_cmd_buf->computeCommandEncoder();
  mtl_cmd_enc->setComputePipelineState(mtl_pstate);
  mtl_cmd_enc->setBuffer(buf_f, 0 /*offset=*/, 0 /*index=*/);
  mtl_cmd_enc->setBuffer(buf_s, 0 /*offset=*/, 1 /*index=*/);
  mtl_cmd_enc->setBuffer(buf_r, 0 /*offset=*/, 2 /*index=*/);
  mtl_cmd_enc->dispatchThreads(MTL::Size::Make(arr_sz, 1, 1) /*threadsPerGrid=*/,
      MTL::Size::Make(mtl_pstate->maxTotalThreadsPerThreadgroup(), 1, 1) /*threadsPerThreadgroup=*/);
  mtl_cmd_enc->endEncoding();

  mtl_cmd_buf->commit();
  mtl_cmd_buf->waitUntilCompleted();

  // ----- //

  float *res = static_cast<float *>(buf_r->contents());
  for (int i = 0; i < arr_sz / 10; i++)
  {
    std::cout << "f: " << f[i] << "\n";
    std::cout << "s: " << s[i] << "\n";
    std::cout << f[i] << " + " << s[i] << " = " << res[i] << "\n";
  }

  // ----- //

  buf_f->release();
  buf_s->release();
  buf_r->release();

  mtl_pstate->release();
  mtl_func->release();
  mtl_lib->release();

  mtl_cmd_queue->release();
  mtl_dev->release();
}
