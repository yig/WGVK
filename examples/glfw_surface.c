#include <GLFW/glfw3.h>
#ifdef __EMSCRIPTEN__
    #include <webgpu/webgpu.h>
#else
    #include <wgvk.h>
#endif
#include <external/incbin.h>
#include <stdio.h>
//INCBIN(simple_shader, "../resources/simple_shader.wgsl");
INCBIN(simple_shaderSpirv, "../resources/simple_shader.spv");

#ifndef STRVIEW
#define STRVIEW(X) (WGPUStringView){X, sizeof(X) - 1}
#endif
#ifdef __EMSCRIPTEN__
#  define GLFW_EXPOSE_NATIVE_EMSCRIPTEN
#  ifndef GLFW_PLATFORM_EMSCRIPTEN // not defined in older versions of emscripten
#    define GLFW_PLATFORM_EMSCRIPTEN 0
#  endif
#else // __EMSCRIPTEN__
#  if SUPPORT_XLIB_SURFACE == 1
#    define GLFW_EXPOSE_NATIVE_X11
#  endif
#  if SUPPORT_WAYLAND_SURFACE == 1
#    define GLFW_EXPOSE_NATIVE_WAYLAND
#  endif
#  ifdef _GLFW_COCOA
#    define GLFW_EXPOSE_NATIVE_COCOA
#  endif
#  ifdef _WIN32
#    define GLFW_EXPOSE_NATIVE_WIN32
#  endif
#endif // __EMSCRIPTEN__

#ifdef GLFW_EXPOSE_NATIVE_COCOA
#  include <Foundation/Foundation.h>
#  include <QuartzCore/CAMetalLayer.h>
#endif

#ifndef __EMSCRIPTEN__
#  include <GLFW/glfw3native.h>
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
void keyfunc(GLFWwindow* window, int key, int scancode, int action, int mods){
    if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS){
        return glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
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
    glfwInit();
    
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "GLFW Window", NULL, NULL);
    glfwSetKeyCallback(window, keyfunc);
    #ifdef _WIN32
    WGPUSurfaceSourceWindowsHWND surfaceChainObj = {
        .chain = {
            .sType = WGPUSType_SurfaceSourceWindowsHWND,
            .next = NULL
        },
        .hwnd = glfwGetWin32Window(window),
        .hinstance = GetModuleHandle(NULL)
    };
    WGPUChainedStruct* surfaceChain = &surfaceChainObj;
    #else
    WGPUSurfaceSourceXlibWindow surfaceChainX11;
    Display* x11_display = glfwGetX11Display();
    Window x11_window = glfwGetX11Window(window);
    surfaceChainX11.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
    surfaceChainX11.chain.next = NULL;
    surfaceChainX11.display = x11_display;
    surfaceChainX11.window = x11_window;

    struct wl_display* native_display = glfwGetWaylandDisplay();
    struct wl_surface* native_surface = glfwGetWaylandWindow(window);
    WGPUSurfaceSourceWaylandSurface surfaceChainWayland;
    surfaceChainWayland.chain.sType = WGPUSType_SurfaceSourceWaylandSurface;
    surfaceChainWayland.chain.next = NULL;
    surfaceChainWayland.display = native_display;
    surfaceChainWayland.surface = native_surface;
    WGPUChainedStruct* surfaceChain = NULL;
    if(x11_window == 0){
        surfaceChain = (WGPUChainedStruct*)&surfaceChainWayland;
    }
    else{
        surfaceChain = (WGPUChainedStruct*)&surfaceChainX11;
    }
    #endif
    WGPUSurfaceDescriptor surfaceDescriptor;
    surfaceDescriptor.nextInChain = surfaceChain;
    surfaceDescriptor.label = (WGPUStringView){ NULL, WGPU_STRLEN };
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    WGPUSurfaceCapabilities caps = {0};
    WGPUPresentMode desiredPresentMode = WGPUPresentMode_Immediate;
    WGPUSurface surface = wgpuInstanceCreateSurface(instance, &surfaceDescriptor);

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
    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();
        wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);
        if(surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal){
            glfwGetWindowSize(window, &width, &height);
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
        wgpuRenderPassEncoderDraw(rpenc, 3, 1, 0, 0);
        //wgpuRenderPassEncoderExecuteBundles(rpenc, 1, &renderBundle);
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