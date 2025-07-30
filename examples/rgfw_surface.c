#define RGFW_WEBGPU
#define RGFW_IMPLEMENTATION
#include <external/RGFW.h>

#include <external/incbin.h>
#include <stdio.h>



const char simpleShaderWGSL[] =
"struct VertexInput {\n"
"    @location(0) position: vec2f\n"
"};\n"
"struct VertexOutput {\n"
"    @builtin(position) position: vec4f\n"
"};\n"
""
"@vertex\n"
"fn vs_main(in: VertexInput) -> VertexOutput {\n"
"    var out: VertexOutput;\n"
"    out.position = vec4f(in.position.x, in.position.y, 0.0f, 1.0f);\n"
"    return out;\n"
"}\n"
"\n"
"@fragment\n"
"fn fs_main(in: VertexOutput) -> @location(0) vec4f {\n"
"    return vec4f(1,0,0,1);\n"
"}\n";
#ifndef EMSCRIPTEN
    INCBIN(simple_shaderSpirv, "../resources/simple_shader.spv");
#endif
#ifndef STRVIEW
    #define STRVIEW(X) (WGPUStringView){X, sizeof(X) - 1}
#endif

#include <stdint.h>

/* ---------- POSIX / Unix-like ---------- */
#if defined(__unix__) || defined(__APPLE__)
  #include <time.h>

  static inline uint64_t nanoTime(void)
  {
      struct timespec ts;
  #if defined(CLOCK_MONOTONIC_RAW)        /* Linux, FreeBSD */
      clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  #else                                   /* macOS 10.12+, other POSIX */
      clock_gettime(CLOCK_MONOTONIC, &ts);
  #endif
      return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
  }

/* ---------- Windows ---------- */
#elif defined(_WIN32)
  #include <windows.h>

  static inline uint64_t nanoTime(void)
  {
      static LARGE_INTEGER freq = { 0 };
      if (freq.QuadPart == 0)               /* one-time init */
          QueryPerformanceFrequency(&freq);

      LARGE_INTEGER counter;
      QueryPerformanceCounter(&counter);
      /* scale ticks → ns: (ticks * 1e9) / freq */
      return (uint64_t)((counter.QuadPart * 1000000000ULL) / freq.QuadPart);
  }

#else
  #error "Platform not supported"
#endif

void adapterCallbackFunction(
        WGPURequestAdapterStatus status,
        WGPUAdapter adapter,
        WGPUStringView label,
        void* userdata1,
        void* userdata2
    ){
    *((WGPUAdapter*)userdata1) = adapter;
}
void deviceCallbackFunction(
        WGPURequestDeviceStatus status,
        WGPUDevice device,
        WGPUStringView message,
        void* userdata1,
        void* userdata2
    ){
    *((WGPUDevice*)userdata1) = device;
}
typedef struct LoopContext{
    RGFW_window* window;
    WGPUSurface surface;
    WGPUDevice device;
    WGPUQueue queue;
    WGPUBuffer vertexBuffer;
    WGPURenderPipeline rp;
} LoopContext;
void mainloop(void* userdata){
    LoopContext* context = (LoopContext*)userdata;

    RGFW_event event;
    while (RGFW_window_checkEvent(context->window, &event)) {
        if (event.type == RGFW_quit) break;
        if (event.type == RGFW_mouseButtonPressed && event.button == RGFW_mouseLeft) {
            printf("You clicked at x: %d, y: %d\n", event.point.x, event.point.y);
        }
    }

    WGPUSurfaceTexture surfaceTexture = {};

    wgpuSurfaceGetCurrentTexture(context->surface, &surfaceTexture);
    if(surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal){
        wgpuSurfaceConfigure(context->surface, &(const WGPUSurfaceConfiguration){
            .alphaMode = WGPUCompositeAlphaMode_Opaque,
            .presentMode = WGPUPresentMode_Fifo,
            .device = context->device,
            .format = WGPUTextureFormat_BGRA8Unorm,
            .width = 1000,
            .height = 1000
        });
        wgpuSurfaceGetCurrentTexture(context->surface, &surfaceTexture);
    }
    WGPUTextureView surfaceView = wgpuTextureCreateView(surfaceTexture.texture, &(const WGPUTextureViewDescriptor){
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .format = WGPUTextureFormat_BGRA8Unorm,
        .dimension = WGPUTextureViewDimension_2D,
        .usage = WGPUTextureUsage_RenderAttachment,
        .aspect = WGPUTextureAspect_All,
    });
    WGPUCommandEncoder cenc = wgpuDeviceCreateCommandEncoder(context->device, NULL);
    WGPURenderPassColorAttachment colorAttachment = {
        .clearValue = (WGPUColor){0.5,0.2,0,0.5},
        .loadOp = WGPULoadOp_Clear,
        .storeOp = WGPUStoreOp_Store,
        .view = surfaceView
    };
    WGPURenderPassEncoder rpenc = wgpuCommandEncoderBeginRenderPass(cenc, &(const WGPURenderPassDescriptor){
        .colorAttachmentCount = 1,
        .colorAttachments = &colorAttachment,
    });
    //wgpuRenderPassEncoderSetScissorRect(rpenc, 0, 0, (frameCount / 8) % 1000, height);
    wgpuRenderPassEncoderSetPipeline(rpenc, context->rp);
    wgpuRenderPassEncoderSetVertexBuffer(rpenc, 0, context->vertexBuffer, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderDraw(rpenc, 3, 1, 0, 0);
    //wgpuRenderPassEncoderExecuteBundles(rpenc, 1, &renderBundle);
    wgpuRenderPassEncoderEnd(rpenc);
    WGPUCommandBuffer cBuffer = wgpuCommandEncoderFinish(cenc, NULL);
    wgpuQueueSubmit(context->queue, 1, &cBuffer);
    wgpuCommandEncoderRelease(cenc);
    wgpuCommandBufferRelease(cBuffer);
    wgpuRenderPassEncoderRelease(rpenc);
    wgpuTextureViewRelease(surfaceView);
    #ifndef EMSCRIPTEN
    wgpuSurfacePresent(context->surface);
    #endif
    //++frameCount;
    //uint64_t nextStamp = nanoTime();
    //if(nextStamp - stamp > ((uint64_t)1000000000ULL)){
    //    stamp = nextStamp;
    //    printf("FPS: %llu\n", (unsigned long long)frameCount);
    //    frameCount = 0;
    //}
}

int main(){
    #ifndef EMSCRIPTEN
    WGPUInstanceLayerSelection lsel = {
        .chain = {
            .next = NULL,
            .sType = WGPUSType_InstanceLayerSelection
        }
    };
    const char* layernames[] = {"VK_LAYER_KHRONOS_validation"};
    lsel.instanceLayers = layernames;
    lsel.instanceLayerCount = 1;
    #endif
    WGPUInstanceDescriptor instanceDescriptor = {
        .nextInChain =
        #if defined(NDEBUG) || defined(EMSCRIPTEN)
        NULL,
        #else
        &lsel.chain,
        #endif
        .capabilities = {
            .timedWaitAnyEnable = 1
        }
    };

    WGPUInstance instance = wgpuCreateInstance(&instanceDescriptor);

    WGPURequestAdapterOptions adapterOptions = {0};
    adapterOptions.featureLevel = WGPUFeatureLevel_Core;
    WGPUAdapter requestedAdapter = NULL;

    WGPURequestAdapterCallbackInfo adapterCallback = {
        .mode = WGPUCallbackMode_WaitAnyOnly,
        .callback = adapterCallbackFunction,
        .userdata1 = (void*)&requestedAdapter
    };


    WGPUFuture aFuture = wgpuInstanceRequestAdapter(instance, &adapterOptions, adapterCallback);
    WGPUFutureWaitInfo winfo = {
        .future = aFuture,
        .completed = 0
    };

    wgpuInstanceWaitAny(instance, 1, &winfo, ~0ull);
    WGPUStringView deviceLabel = {"WGPU Device", sizeof("WGPU Device") - 1};

    WGPUDeviceDescriptor deviceDescriptor = {
        .nextInChain = 0,
        .label = deviceLabel,
        .requiredFeatureCount = 0,
        .requiredFeatures = NULL,
        .requiredLimits = NULL,
        .defaultQueue = {0},
        .deviceLostCallbackInfo = {0},
        .uncapturedErrorCallbackInfo = {0},
    };

    WGPUDevice device = NULL;
    WGPURequestDeviceCallbackInfo requestDeviceCallbackInfo = {
        .callback = deviceCallbackFunction,
        .mode = WGPUCallbackMode_WaitAnyOnly,
        .userdata1 = &device
    };
    WGPUFuture requestDeviceFuture = wgpuAdapterRequestDevice(requestedAdapter, &deviceDescriptor, requestDeviceCallbackInfo);
    WGPUFutureWaitInfo requestDeviceFutureWaitInfo = {
        .future = requestDeviceFuture,
        .completed = 0
    };
    wgpuInstanceWaitAny(instance, 1, &requestDeviceFutureWaitInfo, ~0ull);

    WGPUQueue queue = wgpuDeviceGetQueue(device);
    RGFW_window* window = RGFW_createWindow("RGFW with WebGPU", (RGFW_rect){0,0,1280,720}, 0);

    WGPUSurface surface = RGFW_window_createSurface_WebGPU(window, instance);

    int width, height;
    WGPUSurfaceCapabilities caps = {0};
    wgpuSurfaceGetCapabilities(surface, requestedAdapter, &caps);

    WGPUPresentMode desiredPresentMode = WGPUPresentMode_Fifo;
    wgpuSurfaceConfigure(surface, &(const WGPUSurfaceConfiguration){
        .alphaMode = WGPUCompositeAlphaMode_Opaque,
        .presentMode = desiredPresentMode,
        .usage = WGPUTextureUsage_RenderAttachment,
        .device = device,
        .format = WGPUTextureFormat_BGRA8Unorm,
        .width = width,
        .height = height
    });

    #ifdef EMSCRIPTEN
    WGPUShaderSourceWGSL shaderSource = {
        .chain = {
            .sType = WGPUSType_ShaderSourceWGSL
        },
        .code = {simpleShaderWGSL, sizeof(simpleShaderWGSL) - 1}
    };
    #else
    WGPUShaderSourceSPIRV shaderSource = {
        .chain = {
            .sType = WGPUSType_ShaderSourceSPIRV
        },
        .code = (uint32_t*)gsimple_shaderSpirvData,
        .codeSize = gsimple_shaderSpirvSize,
    };
    #endif
    WGPUShaderModuleDescriptor shaderModuleDesc = {
        .nextInChain = &shaderSource.chain
    };
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderModuleDesc);
    WGPUVertexAttribute vbAttribute = {
        .nextInChain = NULL,
        .shaderLocation = 0,
        .format = WGPUVertexFormat_Float32x2,
        .offset = 0
    };
    WGPUVertexBufferLayout vbLayout = {
        .nextInChain = NULL,
        .arrayStride = sizeof(float) * 2,
        .attributeCount = 1,
        .attributes = &vbAttribute,
        .stepMode = WGPUVertexStepMode_Vertex
    };
    WGPUBlendState blendState = {
        .alpha = {
            .operation = WGPUBlendOperation_Add,
            .srcFactor = WGPUBlendFactor_One,
            .dstFactor = WGPUBlendFactor_One
        },
        .color = {
            .operation = WGPUBlendOperation_Add,
            .srcFactor = WGPUBlendFactor_One,
            .dstFactor = WGPUBlendFactor_One
        }
    };

    WGPUColorTargetState colorTargetState = {
        .writeMask = WGPUColorWriteMask_All,
        .format = WGPUTextureFormat_BGRA8Unorm,
        .blend = NULL
    };

    WGPUFragmentState fragmentState = {
        .entryPoint = STRVIEW("fs_main"),
        .module = shaderModule,
        .targetCount = 1,
        .targets = &colorTargetState
    };
    WGPUPipelineLayoutDescriptor pldesc = {0};
    WGPUPipelineLayout pllayout = wgpuDeviceCreatePipelineLayout(device, &pldesc);

    WGPURenderPipelineDescriptor rpdesc = {
        .vertex = {
            .bufferCount = 1,
            .buffers = &vbLayout,
            .module = shaderModule,
            .entryPoint = STRVIEW("vs_main")
        },
        .fragment = &fragmentState,
        .primitive = {
            .cullMode = WGPUCullMode_None,
            .frontFace = WGPUFrontFace_CCW,
            .topology = WGPUPrimitiveTopology_TriangleList
        },
        .layout = pllayout,
        .multisample = {
            .count = 1,
            .mask = 0xffffffff
        },
    };
    WGPURenderPipeline rp = wgpuDeviceCreateRenderPipeline(device, &rpdesc);
    const float scale = 0.2f;
    const float vertices[6] = {-scale,-scale,-scale,scale,scale,scale};

    WGPUBufferDescriptor bufferDescriptor = {
        .size = sizeof(vertices),
        .usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst
    };
    WGPUBuffer vertexBuffer = wgpuDeviceCreateBuffer(device, &bufferDescriptor);
    wgpuQueueWriteBuffer(queue, vertexBuffer, 0, vertices, sizeof(vertices));

    WGPURenderBundleEncoderDescriptor rbEncDesc = {
        .colorFormatCount = 1,
        .colorFormats = &colorTargetState.format,
        .depthStencilFormat = WGPUTextureFormat_Undefined,
        .sampleCount = 1
    };
    //WGPURenderBundleEncoder rbEnc = wgpuDeviceCreateRenderBundleEncoder(device, &rbEncDesc);
    //wgpuRenderBundleEncoderSetPipeline(rbEnc, rp);
    //wgpuRenderBundleEncoderSetVertexBuffer(rbEnc, 0, vertexBuffer, 0, WGPU_WHOLE_SIZE);
    //wgpuRenderBundleEncoderDraw(rbEnc, 3, 1, 0, 0);
    //WGPURenderBundle renderBundle = wgpuRenderBundleEncoderFinish(rbEnc, NULL);
    printf("Surface: %p\n", surface);
    uint64_t stamp = nanoTime();
    uint64_t frameCount = 0;
    LoopContext* ctx = calloc(1, sizeof(LoopContext));
    ctx->window = window;
    ctx->surface = surface;
    ctx->device = device;
    ctx->queue = queue;
    ctx->vertexBuffer = vertexBuffer;
    ctx->rp = rp;
    #ifndef EMSCRIPTEN
    while(!RGFW_window_shouldClose(window)){
        mainloop((void*)ctx);
    }
    #else

    emscripten_set_main_loop_arg(mainloop, 0, 0, ctx);
    //for(;;){}
    #endif
    //wgpuSurfaceRelease(surface);
}
