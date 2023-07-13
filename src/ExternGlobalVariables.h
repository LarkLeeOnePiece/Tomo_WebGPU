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
//����ȫ�ֱ���
extern time_t start;
extern time_t time_end;//use for time calculation 



extern wgpu::Device device;
extern wgpu::Buffer Ori_pro_buffer;  //original projection
extern wgpu::Buffer Cal_rec_buffer; // for gpu calculate buffer
extern wgpu::Buffer Dim_buffer;
extern wgpu::Buffer ray_dir_buffer;
extern wgpu::Buffer flo_var_buffer;	// some const float var
extern wgpu::Buffer int_var_buffer;
extern wgpu::Buffer cor_img_buffer;
extern wgpu::Buffer pip_buffer;
extern wgpu::Buffer per_crop_buffer;
extern wgpu::Buffer Max;
extern wgpu::Buffer SART_y_buffer;
extern wgpu::Buffer ADMM_y_buffer;
extern wgpu::Buffer ADMM_z_buffer;
extern wgpu::Buffer LADMM_Paras_buffer;

extern glm::uvec3 pdim;									//for the projection size
extern glm::uvec3 vdim;									//for the volume size
extern glm::vec3 dir;
extern glm::vec3 pip;  // for point in plane
extern glm::vec3 vdelta;  //for float
extern glm::vec2 pdelta;  //flor float

extern size_t data_size;
extern size_t recon_size;
extern size_t proj_size;
extern size_t crop_size;
extern size_t final_size;

extern int number_extra_rows;
extern int number_of_tiles;
extern int current_tile;
extern int tile_size;
extern int M;  //sample step
extern int padding;
extern int current_tile;
extern int data_term_iters; //Number of sart iterations for data term proximal operator;
extern int current_projection;
extern int proximal_iters;


extern DIM  AllDim;
extern INTVAR intVAR;
extern FLOATVAR floatVAR;
extern LADMMVAR LADMMParameters;

extern float MAX;
extern float starting_angle;
extern float angle_step;
extern float source_object_distance;
extern float sample_rate;
extern float cost;
extern float sint;
extern float chillfactor;;
extern float tile_norm;

extern string filename;

extern bool random_projection_order;
extern bool tiled;
extern bool check_convergence;

extern std::vector<int> projections;
extern std::vector<std::vector<int>> pw_projections;

extern CImgFloat h_rec_vol, h_SART_y, h_ADMM_y;