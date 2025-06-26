#include "common.h" // Assumed to contain wgpu_init() and wgpu_base struct
#include <stdlib.h>
#include <math.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// The WGSL shader source remains the same.
const char wgslSource[] = R"(
const red = 0.5f;
struct VertexInput {
    @location(0) position: vec2f,
    @location(1) uv: vec2f
};
struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.position = vec4f(in.position.x, in.position.y, 0.0f, 1.0f);
    out.uv = in.uv;
    return out;
}
@group(0) @binding(0) var colDiffuse: texture_2d<f32>;
@group(0) @binding(1) var grsampler: sampler;

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    return textureSample(colDiffuse, grsampler, in.uv);
}
)";

typedef struct {
    wgpu_base base;
    WGPURenderPipeline pipeline;
    WGPUBindGroup bind_group;
    WGPUBuffer index_buffer;
    float t;
} Context;

void main_loop(void* user_data) {
    Context* ctx = (Context*)user_data;
    WGPUDevice device = ctx->base.device;
    WGPUQueue queue = ctx->base.queue;

    ctx->t += 0.01f;
    glfwPollEvents();
    WGPUSurfaceTexture surfaceTexture;
    wgpuSurfaceGetCurrentTexture(ctx->base.surface, &surfaceTexture);

    // The surface must be valid to draw.
    if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal && surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        if (surfaceTexture.texture) wgpuTextureRelease(surfaceTexture.texture);
        return;
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
        .view = surfaceView,
        .resolveTarget = NULL,
        .loadOp = WGPULoadOp_Clear,
        .storeOp = WGPUStoreOp_Store,
        .clearValue = {0.5, 0.2, 0, 0.5},
        .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
    };

    WGPURenderPassEncoder rpenc = wgpuCommandEncoderBeginRenderPass(cenc, &(const WGPURenderPassDescriptor){
        .colorAttachmentCount = 1,
        .colorAttachments = &colorAttachment,
    });

    const float scale = 0.5f;
    float adhocVertices[16] = {
        scale * 1.5f * sinf(ctx->t             ), scale * 1.5f * cosf(ctx->t             ), 0.0f, 0.0f,
        scale * 1.5f * sinf(ctx->t + M_PI_2    ), scale * 1.5f * cosf(ctx->t + M_PI_2    ), 0.0f, 1.0f,
        scale * 1.5f * sinf(ctx->t + 2 * M_PI_2), scale * 1.5f * cosf(ctx->t + 2 * M_PI_2), 1.0f, 1.0f,
        scale * 1.5f * sinf(ctx->t + 3 * M_PI_2), scale * 1.5f * cosf(ctx->t + 3 * M_PI_2), 1.0f, 0.0f,
    };
    WGPUBufferDescriptor adhocBufferDesc = {
        .size = sizeof(adhocVertices),
        .usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst
    };
    WGPUBuffer adhocBuffer = wgpuDeviceCreateBuffer(device, &adhocBufferDesc);
    wgpuQueueWriteBuffer(queue, adhocBuffer, 0, adhocVertices, sizeof(adhocVertices));

    // Record render commands.
    wgpuRenderPassEncoderSetPipeline(rpenc, ctx->pipeline);
    wgpuRenderPassEncoderSetBindGroup(rpenc, 0, ctx->bind_group, 0, NULL);
    wgpuRenderPassEncoderSetIndexBuffer(rpenc, ctx->index_buffer, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetVertexBuffer(rpenc, 0, adhocBuffer, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderDrawIndexed(rpenc, 6, 1, 0, 0, 0);

    wgpuRenderPassEncoderEnd(rpenc);
    WGPUCommandBuffer cbuffer = wgpuCommandEncoderFinish(cenc, NULL);

    // Submit and present.
    wgpuQueueSubmit(queue, 1, &cbuffer);
    #ifndef __EMSCRIPTEN__
    wgpuSurfacePresent(ctx->base.surface);
    #endif
    // Release per-frame resources.
    wgpuBufferRelease(adhocBuffer);
    wgpuRenderPassEncoderRelease(rpenc);
    wgpuTextureViewRelease(surfaceView);
    wgpuCommandBufferRelease(cbuffer);
    wgpuCommandEncoderRelease(cenc);
}


int main() {
    Context* ctx = (Context*)calloc(1, sizeof(Context));

    ctx->base = wgpu_init();
    if (!ctx->base.device) {
        free(ctx);
        return 1;
    }
    WGPUDevice device = ctx->base.device;
    WGPUQueue queue = ctx->base.queue;

    WGPUShaderSourceWGSL shaderCodeDesc = {
        .chain = { .sType = WGPUSType_ShaderSourceWGSL },
        .code = {
            .data = wgslSource,
            .length = WGPU_STRLEN
        }
    };
    WGPUShaderModuleDescriptor shaderDesc = { .nextInChain = &shaderCodeDesc.chain };
    WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);

    // Vertex Layout
    WGPUVertexAttribute vertexAttributes[2] = {
        {
            .shaderLocation = 0,
            .format = WGPUVertexFormat_Float32x2,
            .offset = 0,
        },
        {
            .shaderLocation = 1,
            .format = WGPUVertexFormat_Float32x2,
            .offset = 2 * sizeof(float),
            
        }
    };
    WGPUVertexBufferLayout vbLayout = {
        .arrayStride = sizeof(float) * 4,
        .attributeCount = 2,
        .attributes = vertexAttributes,
        .stepMode = WGPUVertexStepMode_Vertex
    };

    WGPUColorTargetState colorTargetState = {
        .format = WGPUTextureFormat_BGRA8Unorm,
        .writeMask = WGPUColorWriteMask_All,
    };
    WGPUFragmentState fragmentState = {
        .entryPoint = {
            .data = "fs_main",
            .length = sizeof("fs_main") - 1
        },
        .module = shaderModule,
        .targetCount = 1,
        .targets = &colorTargetState,
    };

    WGPUBindGroupLayoutEntry layoutEntries[2] = {
        {.binding = 0,
            .visibility = WGPUShaderStage_Fragment,
            .texture.sampleType = WGPUTextureSampleType_Float,
            .texture.viewDimension = WGPUTextureViewDimension_2D
        },
        {
            .binding = 1,
            .visibility = WGPUShaderStage_Fragment,
            .sampler.type = WGPUSamplerBindingType_Filtering
        }
    };
    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = { .entries = layoutEntries, .entryCount = 2 };
    WGPUBindGroupLayout bindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);

    WGPUPipelineLayoutDescriptor pldesc = {
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &bindGroupLayout
    };
    WGPUPipelineLayout pllayout = wgpuDeviceCreatePipelineLayout(device, &pldesc);

    // Render Pipeline
    WGPURenderPipelineDescriptor rpdesc = {
        .vertex = { 
            .module = shaderModule, 
            .entryPoint = {
                .data = "vs_main",
                .length = sizeof("vs_main") - 1
            },
            .bufferCount = 1,
            .buffers = &vbLayout,
        },
        .fragment = &fragmentState,
        .primitive = { 
            .topology = WGPUPrimitiveTopology_TriangleList,
            .cullMode = WGPUCullMode_None,
            .frontFace = WGPUFrontFace_CCW
        },
        .layout = pllayout,
        .multisample = { .count = 1, .mask = 0xffffffff },
    };
    ctx->pipeline = wgpuDeviceCreateRenderPipeline(device, &rpdesc);

    WGPUTextureDescriptor tdesc = {
        .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
        .dimension = WGPUTextureDimension_2D,
        .size = {10, 10, 1},
        .format = WGPUTextureFormat_RGBA8Unorm,
        .mipLevelCount = 1,
        .sampleCount = 1
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
    
    
    uint8_t* textureData = calloc(tdesc.size.width * tdesc.size.height, 4);
    for (size_t i = 0; i < tdesc.size.width * tdesc.size.height * 4; i++) {
        textureData[i] = (i * 77u) & 255;
    }
    wgpuQueueWriteTexture(
        queue,
        &(WGPUTexelCopyTextureInfo){.texture = texture},
        textureData, tdesc.size.width * tdesc.size.height * 4,
        &(WGPUTexelCopyBufferLayout){.bytesPerRow = tdesc.size.width * 4, .rowsPerImage = tdesc.size.height},
        &(WGPUExtent3D){tdesc.size.width, tdesc.size.height, 1}
    );
    free(textureData);

    WGPUSamplerDescriptor samplerDesc = {
        .addressModeU = WGPUAddressMode_Repeat,
        .addressModeV = WGPUAddressMode_Repeat,
        .addressModeW = WGPUAddressMode_Repeat,
        .magFilter = WGPUFilterMode_Nearest,
        .minFilter = WGPUFilterMode_Nearest,
        .mipmapFilter = WGPUMipmapFilterMode_Linear,
        .lodMinClamp = 0,
        .lodMaxClamp = 1,
        .compare = WGPUCompareFunction_Undefined,
        .maxAnisotropy = 1
    };
    WGPUSampler sampler = wgpuDeviceCreateSampler(device, &samplerDesc);

    WGPUBindGroupEntry bindGroupEntries[2] = {
        {.binding = 0, .textureView = textureView},
        {.binding = 1, .sampler = sampler}
    };
    WGPUBindGroupDescriptor bindGroupDesc = { .layout = bindGroupLayout, .entryCount = 2, .entries = bindGroupEntries };
    ctx->bind_group = wgpuDeviceCreateBindGroup(device, &bindGroupDesc);
    
    const uint32_t indices[] = { 0, 1, 2, 0, 2, 3 };
    WGPUBufferDescriptor iboDesc = { .size = sizeof(indices), .usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst };
    ctx->index_buffer = wgpuDeviceCreateBuffer(device, &iboDesc);
    wgpuQueueWriteBuffer(queue, ctx->index_buffer, 0, indices, sizeof(indices));
    
    wgpuShaderModuleRelease(shaderModule);
    wgpuBindGroupLayoutRelease(bindGroupLayout);
    wgpuPipelineLayoutRelease(pllayout);
    wgpuTextureRelease(texture);
    wgpuTextureViewRelease(textureView);
    wgpuSamplerRelease(sampler);

    ctx->t = 0.0f;
    #ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(main_loop, ctx, 0, 1);
    #else
    while(!glfwWindowShouldClose(ctx->base.window)){
        main_loop(ctx);
    }
    #endif
    return 0;
}