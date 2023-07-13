#pragma once
#include <stdio.h>  // for printf
#include<iostream>  //for string var
//��������ͷ�ļ��������󲿷�WebGPU�ĺ����Լ�����
#include <webgpu/webgpu_cpp.h>   
#include "dawn/utils/WGPUHelpers.h"
#include "Helper.h" //����ǵ���Ϊ������GPU buffer д��

//����ȫ�ֱ�����Ҫһ��ͷ�ļ�
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
//CImg
#include "SART/CImg-2.6.1/CImg.h"
#include "shaderUtil/ShaderUtil.h"
#include "SART/WebStruct.h"  //һЩ�Զ���ṹ
#include "SART/MRCParser.h"  //����MRC file
#include <random>  // for random_device
using namespace std;

float radians(float angle);
void SART();
void linearize_projection();
void Buffer_save(wgpu::Buffer gpubuffer, int buffer_size, int dim[3], string dataname,int spectrum=1);
void DeLinearize_Projections(wgpu::Buffer Volume, size_t Volume_Size, float MaxValue);

float get_angle();

void Crop_data(wgpu::Buffer Rec_Vol, size_t Rec_Vol_Size, wgpu::Buffer CR_buffer, size_t CR_Size);

void Piece_Buffer_copy(wgpu::Buffer gpubuffer, int buffer_size, string dataname);