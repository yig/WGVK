#include <wgvk.h>
#include <external/volk.h>
#include <stdio.h>
#include <external/incbin.h>
#include <string.h>
#ifndef STRVIEW
    #define STRVIEW(X) (WGPUStringView){X, sizeof(X) - 1}
#endif
INCBIN(compute_spv, "../resources/simple_compute.spv");
void adapterCallbackFunction(
        enum WGPURequestAdapterStatus status,
        WGPUAdapter adapter,
        struct WGPUStringView label,
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

void reflectionCallback(WGPUReflectionInfoRequestStatus status, const WGPUReflectionInfo* reflectionInfo, void* userdata1, void* userdata2){
    for(uint32_t i = 0;i < reflectionInfo->globalCount;i++){

        const char* typedesc = NULL;

        if(reflectionInfo->globals[i].buffer.type != WGPUBufferBindingType_BindingNotUsed){
            typedesc = "buffer";
        }
        if(reflectionInfo->globals[i].texture.sampleType != WGPUTextureSampleType_BindingNotUsed){
            typedesc = "texture";
        }
        if(reflectionInfo->globals[i].sampler.type != WGPUSamplerBindingType_BindingNotUsed){
            typedesc = "sampler";
        }
        

        char namebuffer[256] = {0};
        
        const WGPUGlobalReflectionInfo* toBePrinted = reflectionInfo->globals + i;

        memcpy(namebuffer, toBePrinted->name.data, toBePrinted->name.length);
        printf("Name: %s, location: %u, type: %s", namebuffer, toBePrinted->binding, typedesc);
        if(reflectionInfo->globals[i].buffer.type != WGPUBufferBindingType_BindingNotUsed){
            printf(", minBindingSize = %d", (int)toBePrinted->buffer.minBindingSize);
        }
        printf("\n");
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
        NULL
        #else
        &lsel.chain
        #endif
        ,
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
    

    WGPUShaderSourceSPIRV computeSourceSpirv = {
        .chain = {
            .next = NULL,
            .sType = WGPUSType_ShaderSourceSPIRV
        },
        .code = (const uint32_t*)gcompute_spvData,
        .codeSize = gcompute_spvSize
    };
    WGPUShaderModuleDescriptor computeModuleDesc = {
        .nextInChain = &computeSourceSpirv.chain,
        .label = {
            .data   = "Compute Modul",
            .length = sizeof("Compute Modul"),
        }
    };

    WGPUShaderModule computeModule = wgpuDeviceCreateShaderModule(device, &computeModuleDesc);
    
    WGPUReflectionInfoCallbackInfo reflectionCallbackInfo = {
        .nextInChain = NULL,
        .mode = WGPUCallbackMode_WaitAnyOnly,
        .callback = reflectionCallback,
        .userdata1 = NULL,
        .userdata2 = NULL
    };

    WGPUFuture computeReflectionFuture = wgpuShaderModuleGetReflectionInfo(computeModule, reflectionCallbackInfo);

    WGPUFutureWaitInfo reflectionWaitInfo[1] = {
        {
            .future =   computeReflectionFuture,
            .completed = 0
        },
    };
    wgpuInstanceWaitAny(instance, 3, reflectionWaitInfo, 1 << 30);
    WGPUComputePipelineDescriptor cplDesc = {
        .nextInChain = NULL,
        .label = STRVIEW("Kopmute paipline"),
        .compute = {
            .constantCount = 0,
            .constants = NULL,
            .entryPoint = STRVIEW("compute_main"),
            .module = computeModule,
            .nextInChain = NULL
        }
    };
    WGPUBindGroupLayoutEntry bglEntries[1] = {
        [0] = {
            .binding = 0,
            .buffer = {
                .type = WGPUBufferBindingType_Storage,
                .hasDynamicOffset = 0,
                .minBindingSize = 4
            }
        }
    };
    WGPUBindGroupLayoutDescriptor bglDesc = {
        .entries = bglEntries,
        .entryCount = 1
    };
    WGPUBindGroupLayout layout = wgpuDeviceCreateBindGroupLayout(device, &bglDesc);
    WGPUPipelineLayout pllayout = wgpuDeviceCreatePipelineLayout(device, &(WGPUPipelineLayoutDescriptor){
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &layout,
    });

    cplDesc.layout = pllayout;
    WGPUComputePipeline cpl = wgpuDeviceCreateComputePipeline(device, &cplDesc);
    
    WGPUBuffer stbuf = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor){
        .size = 64,
        .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_MapWrite
    });
    
    WGPUBuffer readableBuffer = wgpuDeviceCreateBuffer(device, &(WGPUBufferDescriptor){
        .size = 64,
        .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead
    });

    WGPUQueue queue = wgpuDeviceGetQueue(device);
    float floatData[16] = {
        0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
    };

    WGPUBindGroupEntry entries[1] = {
        (WGPUBindGroupEntry){
            .binding = 0,
            .buffer = stbuf,
            .size = 64,
        }
    };

    wgpuQueueWriteBuffer(queue, stbuf, 0, floatData, sizeof(floatData));
    WGPUBindGroup group = wgpuDeviceCreateBindGroup(device, &(WGPUBindGroupDescriptor){
        .entries = entries,
        .entryCount = 1,
        .layout = layout
    });

    WGPUCommandEncoder cenc = wgpuDeviceCreateCommandEncoder(device, NULL);
    WGPUComputePassEncoder cpenc = wgpuCommandEncoderBeginComputePass(cenc);
    wgpuComputePassEncoderSetPipeline(cpenc, cpl);
    wgpuComputePassEncoderSetBindGroup(cpenc, 0, group, 0, NULL);
    wgpuComputePassEncoderDispatchWorkgroups(cpenc, 16, 1, 1);
    wgpuComputePassEncoderEnd(cpenc);
    wgpuComputePassEncoderRelease(cpenc);
    wgpuCommandEncoderCopyBufferToBuffer(cenc, stbuf, 0, readableBuffer, 0, 64);
    WGPUCommandBuffer cmdBuffer = wgpuCommandEncoderFinish(cenc, NULL);
    wgpuCommandEncoderRelease(cenc);
    wgpuQueueSubmit(queue, 1, &cmdBuffer);
    wgpuCommandBufferRelease(cmdBuffer);

    float* floatRead = NULL;
    wgpuBufferMap(readableBuffer, WGPUMapMode_Read, 0, 64, (void**)&floatRead);
    for(int i = 0;i < 16;i++){
        printf("output value[%d] = %f\n", i, floatRead[i]);
    }
    wgpuBufferUnmap(readableBuffer);
    wgpuBufferRelease(readableBuffer);
    wgpuBufferRelease(stbuf);
    
    wgpuShaderModuleRelease(computeModule);
    wgpuComputePipelineRelease(cpl);
    wgpuBindGroupRelease(group);
    wgpuBindGroupLayoutRelease(layout);
    wgpuPipelineLayoutRelease(pllayout);
    wgpuQueueRelease(queue);
    wgpuDeviceRelease(device);
    wgpuAdapterRelease(requestedAdapter);
    wgpuInstanceRelease(instance);

    return 0;
}