#define CA_PRIVATE_IMPLEMENTATION
#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define MTK_PRIVATE_IMPLEMENTATION

#include <iostream>

#include <Metal/Metal.hpp>
#include <AppKit/AppKit.hpp>
#include <MetalKit/MetalKit.hpp>

//
// @Note(impcuong): Abbrev-list
//  + mtl  := Metal
//  + NS   := NeXTSTEP
//  + CG   := CoreGraphic
//  + res  := resource
//  + enc  := encode
//  + txt  := texture
//  + desc := descriptor
//

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

struct Visible_Vert
{
  float4 pos [[position]];
  float2 coord;
};

vertex Visible_Vert draw_vert_shader(uint vert_id [[vertex_id]])
{
  float2 pos[6] = {
    float2(-1, -1), float2(1, -1), float2(-1, 1),
    float2(-1, 1),  float2(1, -1), float2(1, 1)
  };
  float2 coords[6] = {
    float2(0, 1), float2(1, 1), float2(0, 0),
    float2(0, 0), float2(1, 1), float2(1, 0)
  };

  Visible_Vert vert;
  vert.pos = float4(pos[vert_id], 0, 1);
  vert.coord = coords[vert_id];
  return vert;
}

fragment float4 draw_frag_shader(Visible_Vert in [[stage_in]],
    texture2d<float> txt [[texture(0)]])
{
  constexpr sampler screen_obj(address::clamp_to_edge, filter::linear);
  return txt.sample(screen_obj, in.coord);
}
)";

const float _W = 512.0;
const float _H = 512.0;

class Renderer : public MTK::ViewDelegate
{
public:
  MTL::CommandQueue *queue;
  MTL::RenderPipelineState *pstate;
  MTL::Texture *texture;

  Renderer(MTL::CommandQueue *q, MTL::RenderPipelineState *p, MTL::Texture *t)
    : queue(q), pstate(p), texture(t) {}

  void drawInMTKView(MTK::View *view) override
  {
    MTL::CommandBuffer *cmd = queue->commandBuffer();
    MTL::RenderPassDescriptor *rpd = view->currentRenderPassDescriptor();

    if (rpd)
    {
      MTL::RenderCommandEncoder *enc = cmd->renderCommandEncoder(rpd);
      enc->setRenderPipelineState(pstate);
      enc->setFragmentTexture(texture, 0 /*index=*/);
      enc->drawPrimitives(MTL::PrimitiveTypeTriangle,
          NS::UInteger(0) /*vertexStart=*/, NS::UInteger(6) /*vertexCount=*/);
      enc->endEncoding();

      cmd->presentDrawable(view->currentDrawable());
    }
    cmd->commit();
  }

  void drawableSizeWillChange(MTK::View *view, CGSize size) override {}
};

int main()
{
  NS::AutoreleasePool *res_pool = NS::AutoreleasePool::alloc()->init();

  NS::SharedPtr<MTL::Device> mtl_dev = NS::TransferPtr(MTL::CreateSystemDefaultDevice());
  NS::UInteger alloc_sz = mtl_dev.get()->currentAllocatedSize();
  std::cout << "alloc_sz = " << alloc_sz << "\n";
  MTL::CommandQueue *mtl_cmd_queue = mtl_dev.get()->newCommandQueue();

  // ----- //

  using MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB;

  NS::Error *mtl_err = nullptr;
  NS::String *lib_name = NS::String::string(mtl_shader, NS::UTF8StringEncoding);
  MTL::Library *mtl_lib = mtl_dev->newLibrary(lib_name, nullptr /*options=*/, &mtl_err);

  NS::String *compute_func_name = NS::String::string("add_same_sz_arr_pair", NS::UTF8StringEncoding);
  MTL::Function *mtl_compute_func = mtl_lib->newFunction(compute_func_name);
  MTL::ComputePipelineState *mtl_compute_pstate = mtl_dev->newComputePipelineState(mtl_compute_func, &mtl_err);

  NS::String *draw_vert_func_name = NS::String::string("draw_vert_shader", NS::UTF8StringEncoding);
  MTL::Function *mtl_draw_vert_func = mtl_lib->newFunction(draw_vert_func_name);

  NS::String *draw_frag_func_name = NS::String::string("draw_frag_shader", NS::UTF8StringEncoding);
  MTL::Function *mtl_draw_frag_func = mtl_lib->newFunction(draw_frag_func_name);

  MTL::RenderPipelineDescriptor *mtl_render_pdesc = MTL::RenderPipelineDescriptor::alloc()->init();
  mtl_render_pdesc->setVertexFunction(mtl_draw_vert_func);
  mtl_render_pdesc->setFragmentFunction(mtl_draw_frag_func);
  mtl_render_pdesc->colorAttachments()->object(0 /*attachmentIndex=*/)->setPixelFormat(PixelFormatBGRA8Unorm_sRGB);

  MTL::RenderPipelineState *mtl_render_pstate = mtl_dev->newRenderPipelineState(mtl_render_pdesc, &mtl_err);

  // ----- //

  const size_t arr_sz = _W * _H;
  const float real_sz = sizeof(float) * arr_sz;

  MTL::Buffer *buf_f = mtl_dev->newBuffer(real_sz, MTL::ResourceStorageModeShared);
  MTL::Buffer *buf_s = mtl_dev->newBuffer(real_sz, MTL::ResourceStorageModeShared);
  MTL::Buffer *buf_r = mtl_dev->newBuffer(real_sz, MTL::ResourceStorageModeShared);

  float *f = static_cast<float *>(buf_f->contents()); // typeof(buf_f->contents()) := <void *>
  float *s = static_cast<float *>(buf_s->contents());
  for (int i = 0; i < arr_sz; i++)
  {
    f[i] = 1;
    s[i] = i;
  }

  // ----- //

  MTL::CommandBuffer *mtl_cmd_buf = mtl_cmd_queue->commandBuffer();
  MTL::ComputeCommandEncoder *mtl_cmd_enc = mtl_cmd_buf->computeCommandEncoder();
  mtl_cmd_enc->setComputePipelineState(mtl_compute_pstate);
  mtl_cmd_enc->setBuffer(buf_f, 0 /*offset=*/, 0 /*index=*/);
  mtl_cmd_enc->setBuffer(buf_s, 0 /*offset=*/, 1 /*index=*/);
  mtl_cmd_enc->setBuffer(buf_r, 0 /*offset=*/, 2 /*index=*/);
  mtl_cmd_enc->dispatchThreads(MTL::Size::Make(arr_sz /*width=*/, 1 /*height=*/, 1 /*depth=*/) /*threadsPerGrid=*/,
      MTL::Size::Make(mtl_compute_pstate->maxTotalThreadsPerThreadgroup(), 1, 1) /*threadsPerThreadgroup=*/);
  mtl_cmd_enc->endEncoding();

  mtl_cmd_buf->commit();
  mtl_cmd_buf->waitUntilCompleted();

  // ----- //

  float *res = static_cast<float *>(buf_r->contents());
  for (int i = 0; i < arr_sz / 1000; i++)
  {
    std::cout << "f: " << f[i] << ", s: " << s[i] << "\n";
    std::cout << f[i] << " + " << s[i] << " = " << res[i] << "\n";
  }

  buf_f->release();
  buf_s->release();
  buf_r->release();

  mtl_compute_pstate->release();
  mtl_compute_func->release();

  // ------ //

  //
  // Creates texture to display the precomputed data.
  //

  MTL::TextureDescriptor *mtl_txt_desc = MTL::TextureDescriptor::alloc()->init();
  mtl_txt_desc->setWidth(_W);
  mtl_txt_desc->setHeight(_H);
  mtl_txt_desc->setPixelFormat(PixelFormatBGRA8Unorm_sRGB);
  mtl_txt_desc->setUsage(MTL::TextureUsageShaderRead);

  MTL::Texture *mtl_txt = mtl_dev->newTexture(mtl_txt_desc /*descriptor=*/);

  //
  // Converts our float-array to RGBA pixels
  //

  float min_val = *std::min_element(res, res + arr_sz);
  float max_val = *std::max_element(res, res + arr_sz);

  std::vector<uint8_t> pixels(arr_sz * 4);
  for (int i = 0; i < arr_sz; i++)
  {
    float norm = (res[i] - min_val) / (max_val - min_val);
    // @Note: Normalizes to 0-255 range.
    uint8_t color = static_cast<uint8_t>(norm * 255.0f);
    pixels[i * 4 + 0] = color; // R
    pixels[i * 4 + 1] = color; // G
    pixels[i * 4 + 2] = color; // B
    pixels[i * 4 + 3] = 255;   // A
  }

  //
  // Uploads to this texture.
  //

  MTL::Region mtl_region(0, 0, _W, _H);
  mtl_txt->replaceRegion(mtl_region, 0 /*level=*/, pixels.data() /*pixelBytes=*/, _W * 4 /*bytesPerRow=*/);

  // ----- //

  //
  // Setups phase.
  //

  // @Docs: https://en.wikipedia.org/wiki/NeXTSTEP
  NS::Application *app = NS::Application::sharedApplication();

  using NS::StringEncoding::UTF8StringEncoding;

  CGRect content_rect = {
    .origin = {.x = 100.0, .y = 100.0},
    .size = {.width = _W, .height = _H}
  };
  NS::Window *app_window = NS::Window::alloc()->init(
      content_rect,
      NS::WindowStyleMaskClosable | NS::WindowStyleMaskTitled /*styleMask=*/,
      NS::BackingStoreBuffered /*backing=*/,
      false /*defer=*/
  );

  assert(mtl_dev.get() != nullptr);
  // From: <Foundation/NSObject.hpp>
  // namespace NS
  // {
  //   template <class _Class, class _Base = class Object>
  //   class _NS_EXPORT Referencing : public _Base
  //   {
  //     public:
  //       _Class   *retain();
  //       void     release();
  //       _Class   *autorelease();
  //       UInteger retainCount() const;
  //   };
  // }
  //
  // From: <MetalKit/MTKView.hpp>
  // class View : public NS::Referencing<MTK::View, NS::View> { ... }
  MTK::View *app_view = MTK::View::alloc()->init(content_rect /*frame=*/, mtl_dev.get() /*pDevice=*/);
  assert(app_view != nullptr);

  app_view->setColorPixelFormat(PixelFormatBGRA8Unorm_sRGB);
  app_view->setClearColor(MTL::ClearColor::Make(0.5, 0.5, 0.5, 1.0));

  app_window->setContentView(app_view /*pContentView=*/);
  app_window->setTitle(NS::String::string("Welcome to the other side", UTF8StringEncoding));
  app_window->makeKeyAndOrderFront(nullptr /*pSender=*/);

  app->activateIgnoringOtherApps(true);

  //
  // Renders phase.
  //

  Renderer *renderer = new Renderer(mtl_cmd_queue, mtl_render_pstate, mtl_txt);
  app_view->setDelegate(renderer);
  app_view->setPaused(false);
  app_view->setEnableSetNeedsDisplay(true);

  app->run(); // @Note: Triggering signal.

  delete renderer;

  mtl_draw_vert_func->release();
  mtl_draw_frag_func->release();

  app_view->release();
  app_window->release();
  app->release();

  // ----- //

  mtl_lib->release();
  mtl_dev->release();

  res_pool->release();
}
