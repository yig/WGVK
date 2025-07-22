#include "common.h"
#include <stdlib.h>
#include <math.h>

#define vertexFloatCount 9

int main(){
    wgpu_base base = wgpu_init();
    printf("Initialized device: %p\n", base.device);
    WGPUBufferDescriptor vbDesc = {
        .usage = WGPUBufferUsage_Raytracing,
        .size = sizeof(float) * vertexFloatCount,
    };
    WGPUBuffer vertexBuffer = wgpuDeviceCreateBuffer(base.device, &vbDesc);
    float vertices[vertexFloatCount] = {
        0, 0, 0,
        1, 0, 0,
        0, 1, 0
    };

    wgpuQueueWriteBuffer(base.queue, vertexBuffer, 0, vertices, vertexFloatCount * sizeof(float));
    WGPURayTracingAccelerationGeometryDescriptor geometry = {
        .type = WGPURayTracingAccelerationGeometryType_Triangles,
        .index.format = WGPUIndexFormat_Undefined,
        .vertex = {
            .format = WGPUVertexFormat_Float32x3,
            .count = 3,
            .stride = sizeof(float) * vertexFloatCount,
            .offset = 0,
            .buffer = vertexBuffer
        }
    };
    

    WGPURayTracingAccelerationContainerDescriptor blasDesc = {
        .level = WGPURayTracingAccelerationContainerLevel_Bottom,
        .geometryCount = 1,
        .geometries = &geometry
    };



    WGPURayTracingAccelerationContainer blas = wgpuDeviceCreateRayTracingAccelerationContainer(base.device, &blasDesc);
    
    WGPURayTracingAccelerationInstanceTransformDescriptor identity = {0};
    identity.scale.x = 1;
    identity.scale.y = 1;
    identity.scale.z = 1;
    WGPURayTracingAccelerationInstanceDescriptor instance = {
        .instanceId = 0,
        .instanceOffset = 0,
        .transform = identity,
        .geometryContainer = blas,
    };
    
    {
        WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(base.device, NULL);
        wgpuCommandEncoderBuildRayTracingAccelerationContainer(enc, blas);
        WGPUCommandBuffer cbuf = wgpuCommandEncoderFinish(enc, NULL);
        wgpuQueueSubmit(base.queue, 1, &cbuf);
    }

    WGPURayTracingAccelerationContainerDescriptor tlasDesc = {
        .level = WGPURayTracingAccelerationContainerLevel_Top,
        .instanceCount = 1,
        .instances = &instance
    };
    
    WGPURayTracingAccelerationContainer tlas = wgpuDeviceCreateRayTracingAccelerationContainer(base.device, &tlasDesc);
    {
        WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(base.device, NULL);
        wgpuCommandEncoderBuildRayTracingAccelerationContainer(enc, tlas);
        WGPUCommandBuffer cbuf = wgpuCommandEncoderFinish(enc, NULL);
        wgpuQueueSubmit(base.queue, 1, &cbuf);
    }
}