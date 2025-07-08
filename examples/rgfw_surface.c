#define RGFW_IMPLEMENTATION
#define RGFW_USE_XDL
#include <external/RGFW.h>
#include <wgvk.h>
#include <wgvk.h>
#include <external/incbin.h>
#include <stdio.h>
//INCBIN(simple_shader, "../resources/simple_shader.wgsl");
INCBIN(simple_shaderSpirv, "../resources/simple_shader.spv");

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
WGPUSurface RGFW_GetWGPUSurface(WGPUInstance instance, RGFW_window* window);
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

int main(){
    
    WGPUInstanceLayerSelection lsel = {
        .chain = {
            .next = NULL,
            .sType = WGPUSType_InstanceValidationLayerSelection
        }
    };
    const char* layernames[] = {"VK_LAYER_KHRONOS_validation"};
    lsel.instanceLayers = layernames;
    lsel.instanceLayerCount = 1;
    
    WGPUInstanceDescriptor instanceDescriptor = {
        .nextInChain = 
        #ifdef NDEBUG
        NULL,
        #else
        &lsel.chain,
        #endif
        .capabilities = {0}
    };

    WGPUInstance instance = wgpuCreateInstance(&instanceDescriptor);

    WGPURequestAdapterOptions adapterOptions = {0};
    adapterOptions.featureLevel = WGPUFeatureLevel_Core;
    WGPURequestAdapterCallbackInfo adapterCallback = {0};
    adapterCallback.callback = adapterCallbackFunction;
    WGPUAdapter requestedAdapter;
    adapterCallback.userdata1 = (void*)&requestedAdapter;
    

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
    
    WGPUSurface surface = RGFW_GetWGPUSurface(instance, window);

    int width, height;
    WGPUSurfaceCapabilities caps = {0};
    WGPUPresentMode desiredPresentMode = WGPUPresentMode_Mailbox;

    wgpuSurfaceGetCapabilities(surface, requestedAdapter, &caps);
    
    wgpuSurfaceConfigure(surface, &(const WGPUSurfaceConfiguration){
        .alphaMode = WGPUCompositeAlphaMode_Opaque,
        .presentMode = desiredPresentMode,
        .device = device,
        .format = WGPUTextureFormat_BGRA8Unorm,
        .width = width,
        .height = height
    });
    
    
    //WGPUShaderSourceWGSL shaderSourceWgsl = {
    //    .chain = {
    //        .sType = WGPUSType_ShaderSourceWGSL
    //    },
    //    .code = {
    //        .data = (const char*)gsimple_shaderData,
    //        .length = gsimple_shaderSize
    //    }
    //};
    
    WGPUShaderSourceSPIRV shaderSourceSpirv = {
        .chain = {
            .sType = WGPUSType_ShaderSourceSPIRV
        },
        .code = (uint32_t*)gsimple_shaderSpirvData,
        .codeSize = gsimple_shaderSpirvSize,
    };

    WGPUShaderModuleDescriptor shaderModuleDesc = {
        .nextInChain = &shaderSourceSpirv.chain
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
    WGPURenderBundleEncoder rbEnc = wgpuDeviceCreateRenderBundleEncoder(device, &rbEncDesc);
    wgpuRenderBundleEncoderSetPipeline(rbEnc, rp);
    wgpuRenderBundleEncoderSetVertexBuffer(rbEnc, 0, vertexBuffer, 0, WGPU_WHOLE_SIZE);
    wgpuRenderBundleEncoderDraw(rbEnc, 3, 1, 0, 0);
    WGPURenderBundle renderBundle = wgpuRenderBundleEncoderFinish(rbEnc, NULL);    
    WGPUSurfaceTexture surfaceTexture;

    uint64_t stamp = nanoTime();
    uint64_t frameCount = 0;
    while(!RGFW_window_shouldClose(window)){
        while (RGFW_window_checkEvent(window)) {
            if (window->event.type == RGFW_quit) break;
            
            if (window->event.type == RGFW_mouseButtonPressed && window->event.button == RGFW_mouseLeft) {
                printf("You clicked at x: %d, y: %d\n", window->event.point.x, window->event.point.y);
            }
        }
        wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);
        if(surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal){
            
            wgpuSurfaceConfigure(surface, &(const WGPUSurfaceConfiguration){
                .alphaMode = WGPUCompositeAlphaMode_Opaque,
                .presentMode = desiredPresentMode,
                .device = device,
                .format = WGPUTextureFormat_BGRA8Unorm,
                .width = width,
                .height = height
            });
            wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);
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
        WGPUCommandEncoder cenc = wgpuDeviceCreateCommandEncoder(device, NULL);
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
        wgpuRenderPassEncoderSetPipeline(rpenc, rp);
        wgpuRenderPassEncoderSetVertexBuffer(rpenc, 0, vertexBuffer, 0, WGPU_WHOLE_SIZE);
        //wgpuRenderPassEncoderDraw(rpenc, 3, 1, 0, 0);
        wgpuRenderPassEncoderExecuteBundles(rpenc, 1, &renderBundle);
        wgpuRenderPassEncoderEnd(rpenc);
        
        WGPUCommandBuffer cBuffer = wgpuCommandEncoderFinish(cenc, NULL);
        wgpuQueueSubmit(queue, 1, &cBuffer);
        wgpuCommandEncoderRelease(cenc);
        wgpuCommandBufferRelease(cBuffer);
        wgpuRenderPassEncoderRelease(rpenc);
        wgpuTextureViewRelease(surfaceView);
        wgpuSurfacePresent(surface);
        ++frameCount;
        uint64_t nextStamp = nanoTime();
        if(nextStamp - stamp > ((uint64_t)1000000000ULL)){
            stamp = nextStamp;
            printf("FPS: %llu\n", (unsigned long long)frameCount);
            frameCount = 0;
        }
    }
    wgpuSurfaceRelease(surface);
}

WGPUSurface RGFW_GetWGPUSurface(WGPUInstance instance, RGFW_window* window) {
    if (!instance || !window) {
        fprintf(stderr, "Error: Invalid WGPUInstance or RGFW_window pointer.\n");
        return NULL;
    }

    WGPUSurfaceDescriptor surfaceDesc = {0};
    // The nextInChain will point to the platform-specific source struct

#if defined(RGFW_WINDOWS)
    // --- Windows Implementation ---
    WGPUSurfaceSourceWindowsHWND fromHwnd = {0};
    fromHwnd.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
    fromHwnd.hwnd = window->src.window; // Get HWND from RGFW window source
    if (!fromHwnd.hwnd) {
        fprintf(stderr, "RGFW Error: HWND is NULL for Windows window.\n");
        return NULL;
    }
    fromHwnd.hinstance = GetModuleHandle(NULL); // Get current process HINSTANCE

    surfaceDesc.nextInChain = (WGPUChainedStruct*)&fromHwnd.chain;
    return wgpuInstanceCreateSurface(instance, &surfaceDesc);

#elif defined(RGFW_MACOS) && !defined(RGFW_MACOS_X11) // Exclude X11 on Mac builds
    // --- macOS (Cocoa) Implementation ---
    NSView* nsView = (NSView*)window->src.view;
    if (!nsView) {
        fprintf(stderr, "RGFW Error: NSView is NULL for macOS window.\n");
        return NULL;
    }

    // Ensure the view is layer-backed before trying to get/set the layer
    // Note: RGFW might already do this depending on its configuration.
    // This call is generally safe to make even if already layer-backed.
    ((void (*)(id, SEL, BOOL))objc_msgSend)((id)nsView, sel_registerName("setWantsLayer:"), YES);

    id layer = ((id (*)(id, SEL))objc_msgSend)((id)nsView, sel_registerName("layer"));

    // Check if the layer exists and is already a CAMetalLayer
    if (layer && [layer isKindOfClass:[CAMetalLayer class]]) {
        // Layer exists and is the correct type, proceed
    } else if (!layer) {
        // Layer doesn't exist, create a CAMetalLayer and set it
        CAMetalLayer* metalLayer = [CAMetalLayer layer];
        if (!metalLayer) {
             fprintf(stderr, "RGFW Error: Failed to create CAMetalLayer.\n");
             return NULL;
        }
        ((void (*)(id, SEL, id))objc_msgSend)((id)nsView, sel_registerName("setLayer:"), metalLayer);
        layer = metalLayer; // Use the newly created layer
        // printf("Info: Created and assigned CAMetalLayer for NSView.\n");
    } else {
        // Layer exists but is NOT a CAMetalLayer - this is an issue.
        // The view's layer needs to be explicitly set to CAMetalLayer for WebGPU.
        // This might require changes in how RGFW initializes the view when WebGPU is intended.
        fprintf(stderr, "RGFW Error: NSView's existing layer is not a CAMetalLayer. Cannot create WebGPU surface.\n");
        return NULL;
    }

    // At this point, 'layer' should be a valid CAMetalLayer*
    WGPUSurfaceSourceMetalLayer fromMetal = {0};
    fromMetal.chain.sType = WGPUSType_SurfaceSourceMetalLayer;
    fromMetal.layer = (__bridge CAMetalLayer*)layer; // Use __bridge for ARC compatibility if mixing C/Obj-C

    surfaceDesc.nextInChain = (WGPUChainedStruct*)&fromMetal.chain;
    return wgpuInstanceCreateSurface(instance, &surfaceDesc);


#elif defined(RGFW_UNIX)
    // --- Unix (Wayland/X11) Implementation ---

    #if defined(RGFW_WAYLAND)
    // Check if Wayland is actively being used (if both X11 and Wayland are compiled)
    if (RGFW_usingWayland()) {
        if (window->src.wl_display && window->src.surface) {
            WGPUSurfaceSourceWaylandSurface fromWl = {0};
            fromWl.chain.sType = WGPUSType_SurfaceSourceWaylandSurface;
            fromWl.display = window->src.wl_display; // Get wl_display from RGFW
            fromWl.surface = window->src.surface;   // Get wl_surface from RGFW

            surfaceDesc.nextInChain = (WGPUChainedStruct*)&fromWl.chain;
            return wgpuInstanceCreateSurface(instance, &surfaceDesc);
        }
        fprintf(stderr, "RGFW Info: Using Wayland, but wl_display or wl_surface is NULL.\n");
        return NULL; // Cannot create Wayland surface without handles
    }
    #endif // RGFW_WAYLAND

    #if defined(RGFW_X11)
    // Fallback to X11 if Wayland isn't used or not compiled
    // (or if RGFW_usingWayland() returned false)
    {
        if (window->src.display && window->src.window) {
            WGPUSurfaceSourceXlibWindow fromXlib = {0};
            fromXlib.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
            fromXlib.display = window->src.display; // Get Display* from RGFW
            fromXlib.window = window->src.window;   // Get Window from RGFW

            surfaceDesc.nextInChain = (WGPUChainedStruct*)&fromXlib.chain;
            return wgpuInstanceCreateSurface(instance, &surfaceDesc);
        }
        fprintf(stderr, "RGFW Info: Using X11 (or fallback), but Display* or Window is NULL.\n");
        return NULL; // Cannot create X11 surface without handles
    }
    #endif // RGFW_X11

    // If RGFW_UNIX is defined but neither RGFW_WAYLAND nor RGFW_X11 resulted in a surface
    fprintf(stderr, "RGFW Error: RGFW_UNIX defined, but no Wayland or X11 surface could be created.\n");
    return NULL;

#elif defined(__EMSCRIPTEN__)
    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvasDesc = {0};
    canvasDesc.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
    canvasDesc.selector = (WGPUStringView){.data = "#canvas", .length = 7};
    
    surfaceDesc.nextInChain = &canvasDesc.chain;
    return wgpuInstanceCreateSurface(instance, &surfaceDesc);
#else
    // --- Other/Unsupported Platforms ---
    #warning "RGFW_GetWGPUSurface: Platform not explicitly supported (only Windows, macOS/Cocoa, Wayland, X11 implemented)."
    fprintf(stderr, "RGFW Error: Platform not supported by RGFW_GetWGPUSurface.\n");
    return NULL;
#endif
}