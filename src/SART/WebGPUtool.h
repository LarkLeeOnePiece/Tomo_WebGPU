#pragma once

#include "defines.h"
#include <stdio.h>
#include <webgpu/webgpu_cpp.h>
#include <webgpu/webgpu.h>
#include <GLFW/glfw3.h>

WGPUDevice _NULLABLE SARTcreateDevice(WGPUBackendType type = WGPUBackendType_Force32);