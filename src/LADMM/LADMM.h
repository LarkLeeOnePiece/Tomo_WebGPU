#pragma once
#include "SART/WebStruct.h"  //一些自定义结构
//下面两个头文件包含绝大部分WebGPU的函数以及变量
#include <webgpu/webgpu_cpp.h>   
#include "dawn/utils/WGPUHelpers.h"
#include "Helper.h" //这个是单独为创建空GPU buffer 写的
/*
HEAD file for LADMM algorithm
include this file to use the library for LADMM
All variables just be declared not defined all variables are defined in the source files
*/

//according to Julio's paper, we need parameters like μ，ρ ,L2 norm of K 
extern float mu;//μ used to control proximal of date term for fidelity
extern float ro;//ρ used to control proximal of regularization term
extern float L2_Norm_K;// L2 norm of matrix K
extern float lambda;   //How to define this variable
extern float sigma;   //How to define this variable

void init_LADMM();//to initiate some parameters for LADMM

void LADMM(int data_term=0, int regularization_term=0);  //in fact, I only use SART and ATV

void VolumeGradient(wgpu::Buffer Volume, wgpu::Buffer z_Gradient);

float VolumeNorm(wgpu::Buffer Volume);

void update_x_ladmm();

void Proximal_SART();

void update_z_ladmm_tv();

void update_y_ladmm();