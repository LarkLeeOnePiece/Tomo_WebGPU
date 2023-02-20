#pragma once
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
//CImg
#include "SART/CImg-2.6.1/CImg.h"

//需要注意结构体的大小获取
typedef struct dim {
	int pdim_x;
	int pdim_y;
	int pdim_z;
	
	int vdim_x;
	int vdim_y;
	int vdim_z;

	float vdelta_x;
	float vdelta_y;
	float vdelta_z;

	float pdelta_x;
	float pdelta_y;
} DIM;

typedef cimg_library::CImg<float> CImgFloat;

// sample_num, current_projection, current_tile, tile_size, number_extra_row
typedef struct intVar {
	int sample_num;
	int current_projection;
	int current_tile;
	int tile_size;
	int number_extra_row;
	int padding;
}INTVAR;


// dis_vs  sample_rate   chillfactor
typedef struct floatVar {
	float dis_vs;
	float sample_rate;
	float chillfactor;
}FLOATVAR;