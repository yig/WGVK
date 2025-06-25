#include "common.h"
#include <stdlib.h>

const char wgslSource[] = R"(
struct VertexInput {
    @location(0) position: vec2f
};
struct VertexOutput {
    @builtin(position) position: vec4f
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = vec4f(in.position.x, in.position.y, 0.0f, 1.0f);
    return out;
}
@group(0) @binding(0) var colDiffuse: texture_2d<f32>;
@group(0) @binding(1) var grsampler: sampler;

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    return textureSample(colDiffuse, grsampler, in.position.xy * 0.01f);
}
)";
int main(){
    wgpu_base base = wgpu_init();
    WGPUDevice device = base.device;
    WGPUShaderSourceWGSL shaderSourceSpirv = {
        .chain = {
            .sType = WGPUSType_ShaderSourceWGSL
        },
        .code = {
            .data = wgslSource,
            .length = sizeof(wgslSource) - 1
        }
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
            .srcFactor = WGPUBlendFactor_One,
            .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
            .operation = WGPUBlendOperation_Add,
        },
        .color = {
            .srcFactor = WGPUBlendFactor_SrcAlpha,
            .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
            .operation = WGPUBlendOperation_Add
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

    WGPUBindGroupLayoutEntry layoutEntries[2] = {
        {
            .binding = 0,
            .texture.sampleType = WGPUTextureSampleType_Float,
            .texture.multisampled = 0,
            .texture.viewDimension = WGPUTextureViewDimension_2D
        },
        {
            .binding = 1,
            .sampler.type = WGPUSamplerBindingType_Filtering,
        }
    };
    WGPUBindGroupLayoutDescriptor bindGroupLayoutDescriptor = {
        .entries = layoutEntries,
        .entryCount = 2,
    };
    WGPUBindGroupLayout bindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDescriptor);

    WGPUPipelineLayoutDescriptor pldesc = {
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &bindGroupLayout
    };
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
    WGPUTextureFormat formatrgba8 = WGPUTextureFormat_RGBA8Unorm;
    WGPUTextureDescriptor tdesc = {
        .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
        .dimension = WGPUTextureDimension_2D,
        .size = {10,10,1},
        .format = WGPUTextureFormat_RGBA8Unorm,
        .mipLevelCount = 1,
        .sampleCount = 1,
        .viewFormatCount = 1,
        .viewFormats = &formatrgba8
    };
    WGPUTexture texture = wgpuDeviceCreateTexture(device, &tdesc);
    WGPUTextureViewDescriptor viewDesc = {
        .format = WGPUTextureFormat_RGBA8Unorm,
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .aspect = WGPUTextureAspect_All,
        .usage = WGPUTextureUsage_TextureBinding
    };
    WGPUTextureView textureView = wgpuTextureCreateView(texture, &viewDesc);
    
    WGPUSamplerDescriptor samplerDesc = {
        .addressModeU = WGPUAddressMode_Repeat,
        .addressModeV = WGPUAddressMode_Repeat,
        .addressModeW = WGPUAddressMode_Repeat,
        .magFilter = WGPUFilterMode_Nearest,
        .minFilter = WGPUFilterMode_Nearest,
        .mipmapFilter = WGPUMipmapFilterMode_Linear,
        .lodMinClamp = 0,
        .lodMaxClamp = 1,
        .compare = WGPUCompareFunction_Always,
        .maxAnisotropy = 0
    };

    WGPUSampler sampler = wgpuDeviceCreateSampler(device, &samplerDesc);

    WGPUBindGroupEntry bindGroupEntries[2] = {
        [0] = {
            .binding = 0,
            .textureView = textureView,
        },
        [1] = {
            .binding = 1,
            .sampler = sampler
        }
    };
    WGPUBindGroupDescriptor bindGroupDescriptor = {
        .entries = bindGroupEntries,
        .entryCount = 2,
        .layout = bindGroupLayout
    };
    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupDescriptor);

    const float scale = 0.5f;
    const float vertices[8] = {
        -scale,-scale, 
        -scale, scale,
         scale, scale,
         scale,-scale,
    };
    const uint32_t indices[6] = {
        0,1,2,0,2,3
    };
    
    WGPUBufferDescriptor bufferDescriptor = {
        .size = sizeof(vertices),
        .usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst
    };
    WGPUBufferDescriptor indexBufferDescriptor = {
        .size = sizeof(indices),
        .usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst
    };
    WGPUBuffer vertexBuffer = wgpuDeviceCreateBuffer(device, &bufferDescriptor);
    WGPUBuffer indexBuffer = wgpuDeviceCreateBuffer(device, &indexBufferDescriptor);
    wgpuQueueWriteBuffer(base.queue, vertexBuffer, 0, vertices, sizeof(vertices));
    wgpuQueueWriteBuffer(base.queue, indexBuffer, 0, indices, sizeof(indices));
    uint8_t* textureData = calloc(tdesc.size.width * tdesc.size.height, 4);
    for(size_t i = 0;i < tdesc.size.width * tdesc.size.height * 4;i++){
        textureData[i] = (i * 77u) & 255;
    }
    WGPUTexelCopyTextureInfo writeTexture = {
        .texture = texture,
        .aspect = WGPUTextureAspect_All,
        .mipLevel = 0,
        .origin = {0, 0, 0}
    };
    WGPUTexelCopyBufferLayout writeLayout = {
        .bytesPerRow = tdesc.size.width * 4,
        .rowsPerImage = tdesc.size.height
    };
    wgpuQueueWriteTexture(base.queue, &writeTexture, textureData, tdesc.size.width * tdesc.size.height * 4 , &writeLayout, 
        &(WGPUExtent3D){
        tdesc.size.width,
        tdesc.size.height,
        1
    });
    WGPUSurfaceTexture surfaceTexture;
    int width, height;
    float t = 0;
    while(!glfwWindowShouldClose(base.window)){
        glfwPollEvents();
        t += 0.01f;
        wgpuSurfaceGetCurrentTexture(base.surface, &surfaceTexture);
        if(surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal){
            glfwGetWindowSize(base.window, &width, &height);
            wgpuSurfaceConfigure(base.surface, &(const WGPUSurfaceConfiguration){
                .alphaMode = WGPUCompositeAlphaMode_Opaque,
                .presentMode = WGPUPresentMode_Mailbox,
                .device = device,
                .format = WGPUTextureFormat_BGRA8Unorm,
                .width = width,
                .height = height
            });
            wgpuSurfaceGetCurrentTexture(base.surface, &surfaceTexture);
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

        WGPUBuffer adhocbuffer = wgpuDeviceCreateBuffer(device, &bufferDescriptor);
        
        float adhocVertices[8] = {
            scale * 1.5 * sin(t             ), scale * 1.5 * cos(t             ),
            scale * 1.5 * sin(t + M_PI_2    ), scale * 1.5 * cos(t + M_PI_2    ),
            scale * 1.5 * sin(t + 2 * M_PI_2), scale * 1.5 * cos(t + 2 * M_PI_2),
            scale * 1.5 * sin(t + 3 * M_PI_2), scale * 1.5 * cos(t + 3 * M_PI_2)
        };
        wgpuQueueWriteBuffer(base.queue, adhocbuffer, 0, adhocVertices, sizeof(adhocVertices));
        wgpuRenderPassEncoderSetPipeline(rpenc, rp);
        wgpuRenderPassEncoderSetBindGroup(rpenc, 0, bindGroup, 0, NULL);
        wgpuRenderPassEncoderSetIndexBuffer(rpenc, indexBuffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
        wgpuRenderPassEncoderSetVertexBuffer(rpenc, 0, adhocbuffer, 0, WGPU_WHOLE_SIZE);
        wgpuRenderPassEncoderDrawIndexed(rpenc, 6, 1, 0, 0, 0);
        wgpuRenderPassEncoderSetVertexBuffer(rpenc, 0, vertexBuffer, 0, WGPU_WHOLE_SIZE);
        wgpuRenderPassEncoderDrawIndexed(rpenc, 6, 1, 0, 0, 0);
        wgpuRenderPassEncoderEnd(rpenc);
        WGPUCommandBuffer cbuffer = wgpuCommandEncoderFinish(cenc, NULL);

        wgpuQueueSubmit(base.queue, 1, &cbuffer);
        wgpuCommandEncoderRelease(cenc);
        wgpuCommandBufferRelease(cbuffer);
        wgpuRenderPassEncoderRelease(rpenc);
        wgpuTextureViewRelease(surfaceView);
        wgpuBufferRelease(adhocbuffer);
        wgpuSurfacePresent(base.surface);
    }
}