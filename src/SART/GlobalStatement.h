#pragma once
#include "SART/MRCParser.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
//CImg
#include "SART/CImg-2.6.1/CImg.h"
#include "SART/WebStruct.h"  //һЩ�Զ���ṹ
#include <time.h>  for time counting
//Get radians from angle
#define PI 3.14159265358979323846

time_t start, time_end;//use for time calculation 
MRCParser mrcParser;

string filename;

glm::uvec3 pdim;									//for the projection size
glm::uvec3 vdim;									//for the volume size
glm::vec3 vdelta;  //for float
glm::vec2 pdelta;  //flor float
glm::vec3 dir;  // for ray dir
glm::vec3 pip;  // for point in plane

DIM  AllDim;
INTVAR intVAR;
FLOATVAR floatVAR;

float starting_angle;
float angle_step;
float source_object_distance;
float sample_rate;
float MAX;
float cost;
float sint;
float chillfactor;


bool tiled;
bool random_projection_order; //Random projection order or not

int number_extra_rows;
int number_of_tiles;
int current_tile;
int tile_size;
int volume_depth;
int M;  //sample step
int padding;
int data_term_iters; //Number of sart iterations for data term proximal operator
int current_projection;

size_t data_size;
size_t recon_size;
size_t proj_size;
size_t crop_size;
size_t final_size;


std::vector<int> projections;
std::vector<std::vector<int>> pw_projections;

//Host data  �����汻SART���������
CImgFloat h_rec_vol, h_proj_data;//h_rec_vol for cropped result in host

//GPU Var
WGPUDevice wgpuDevice;
wgpu::Device device;
wgpu::Buffer Ori_pro_buffer;  //original projection
wgpu::Buffer Cal_rec_buffer; // for gpu calculate buffer
wgpu::Buffer H_rec_buffer;// for host 
wgpu::Buffer Dim_buffer;
wgpu::Buffer ray_dir_buffer;
wgpu::Buffer flo_var_buffer;	// some const float var
wgpu::Buffer int_var_buffer;
wgpu::Buffer cor_img_buffer;
wgpu::Buffer pip_buffer;
wgpu::Buffer per_crop_buffer;
wgpu::Buffer Max;  // for saving max value in linearized and delineared