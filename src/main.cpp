#include <stdio.h>
#include <iostream>
//��������ͷ�ļ��������󲿷�WebGPU�ĺ����Լ�����
#include <webgpu/webgpu_cpp.h>   
#include "dawn/utils/WGPUHelpers.h"
#include "Helper.h" //����ǵ���Ϊ������GPU buffer д��

//������GPU�豸��������
#include "SART/WebGPUtool.h"
//shader�Ĵ�����
#include "shaderUtil/ShaderUtil.h"
#include "SART/SART.h"  //define functions related to SART 
#include <windows.h>// for Sleep()
#include "SART/MRCParser.h"  //����MRC file
#include "SART/GlobalStatement.h"   //�������Ѿ�������ȫ�ֱ����Ķ����ˣ������ط���Ҫ������Ҫʹ��extern
#include "SART/WebStruct.h"  //һЩ�Զ���ṹ


//������LADMM�ĺ���
#include"LADMM/LADMM.h"


using namespace std;

//��¼�ܽ�һ��ͨ�ü�����ߵĻ�����ܺ͸���������
void GPGPUSample(wgpu::Device device) {
	
	//1. ����Ϊ���� GPU �������ݽ�������Ҫһ���а���Ĳ��ֶ��������� GPUBindGroupLayout����������ߵĶ��壬
	//���������ǰԤ֪�������Ҫ�������Դ���ͣ�Buffer��Texture��Sampler��
	//�Ӷ��õ�����Ĵ����ٶ�
	auto bindGroupLayout = utils::MakeBindGroupLayout(
		device, {
			{0, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // for projection data
			//{bindingNum,ShaderSatge,BindingType},
		});

	//2. Create BindGroup
	wgpu::BindGroup ComputeBindGroup = utils::MakeBindGroup(
		device, bindGroupLayout, {
			{0, Ori_pro_buffer, 0, data_size},
		  //BindingInitializationHelper->{bingding��ţ�buffer��offest,size}
		});

	//3. pass the Layout for the pipeline Layout
	wgpu::PipelineLayout pipelineLayout = utils::MakeBasicPipelineLayout(device, &bindGroupLayout); //����Ӧ�ÿ���ֱ��ʹ��pipelineLayout

	//׼������pipeline����Ҫ׼��shader,�����Ϣ
	//����shader
	std::string shaderFile( "work/shader/xxxx.fs");
	wgpu::ShaderModule ShaderModule = ShaderUtil::loadAndCompileShader(device, shaderFile);
	//setup FP compute pipeline
	wgpu::ComputePipelineDescriptor comDescriptor;
	wgpu::ProgrammableStageDescriptor proDescriptor;
	proDescriptor.entryPoint = "main";
	proDescriptor.module = ShaderModule;
	comDescriptor.layout = pipelineLayout;
	comDescriptor.compute = proDescriptor;

	//4.Create compute pipeline
	wgpu::ComputePipeline computepipeline = device.CreateComputePipeline(&comDescriptor);
	//��ȡָ�����
	auto queue = device.GetQueue();
	

	//5. ����ָ�������
	wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

	wgpu::ComputePassDescriptor compassdescriptor;
	compassdescriptor.label = "xxx����ͨ��";
	wgpu::ComputePassEncoder computepass;

	computepass = encoder.BeginComputePass(&compassdescriptor);
	computepass.SetPipeline(computepipeline);
	computepass.SetBindGroup(0, ComputeBindGroup);

	int workgroupCountX = ceil((float)pdim.x / 8.0);	//����ÿ�ζ���Ҫ����
	int workgroupCountY = ceil((float)vdim.y / 8.0);   //����ÿ�ζ���Ҫ��������Ҫ��shader��Ӧ

	computepass.DispatchWorkgroups(workgroupCountX, workgroupCountY); //����shader
	computepass.End();
	wgpu::CommandBuffer commands = encoder.Finish();
	queue.Submit(1, &commands);  //�ύ��GPU��ָ�����������

}

/**
  * Main application entry point.
  */
void Initial() {
	//initial var
	cout << "************* Initial .... ***************" << endl;
	filename = ".\\data\\ts_16.ali"; 
	starting_angle= 0.0f;   angle_step = 0.0f;
	tiled = true;  number_extra_rows = 80; number_of_tiles = 25;   //for tiled data
	volume_depth = 300;
	sample_rate = 0.5;
	current_tile = 0;
	current_projection = 0;
	random_projection_order = false;
	chillfactor = 0.2;
	data_term_iters = 5;
	proximal_iters = 2;



	//To read MRC files, by Ondrej
	mrcParser.load_original(filename);
	//Get the projection sizes and print
	pdim.x = mrcParser.dimensions(0);
	pdim.y = mrcParser.dimensions(1);
	pdim.z = mrcParser.dimensions(2);

	if ((starting_angle == 0.0f) && (angle_step == 0.0f))
	{
		printf("Projections starting angle and angle step were not provided.\n");
		if (pdim.z == 41)
		{
			starting_angle = -60.0;
			angle_step = 3.0;
			projections = { 20, 21, 19, 22, 18, 23, 17, 24, 16, 25, 15, 26, 14, 27, 13, 28, 12, 29, 11, 30, 10, 31, 9, 32, 8, 33, 7, 34, 6, 35, 5, 36, 4, 37, 3, 38, 2, 39, 1, 40, 0 };
		}
	}

	
	//����ͶӰ����
	h_proj_data = CImgFloat(pdim.x, pdim.y, pdim.z, 1, 0.0f);
	h_proj_data._data = mrcParser.getData();

	//�����￪ʼ��ͼ
	if (ifcrop) {
		int fromx = 256, widthx = 512;
		int fromy = 464, heighty = 512;
		int index = 0;
		h_crop_proj_data = CImgFloat(widthx, heighty, pdim.z, 1, 0.0f);
		for (int z = 0; z < pdim.z; z++) {
			for (int y = fromy; y < fromy + heighty; y++) {
				for (int x = fromx; x < fromx + widthx; x++) {
					h_crop_proj_data._data[index] = h_proj_data._data[x + pdim.x * y + pdim.x * pdim.y * z];
					index++;
				}
			}
		}
		pdim.x = widthx; widthx; pdim.y = heighty;
		tiled = true;  number_extra_rows = 80; number_of_tiles = 10;   //for tiled data
	}

	

	//h_crop_proj_data.save_analyze("my_crop.mrc");
	

	//tiled or not
	if (tiled && (number_of_tiles != 1))
	{
		//Check that # extra rows is even
		if (number_extra_rows % 2 != 0)
		{
			printf("Number of extra rows must be even!\n");

		}

		//Check that the image/volume y-axis can be divided by selected number of pieces
		if (pdim.y % number_of_tiles == 0)
		{
			tile_size = pdim.y / number_of_tiles;
			printf("tiled, %d pieces of size %d, %d extra rows\n", number_of_tiles, tile_size, number_extra_rows);
		}
		else
		{
			printf("y-axis length is not divisible by selected number of pieces!\n");
		}
	}
	else
	{
		//Non-tiled settings
		tiled = false;
		number_of_tiles = 1;
		current_tile = 0;
		tile_size = pdim.y;
		number_extra_rows = 0;
	}

	if (tiled)
	{
		vdim.z = volume_depth;
		vdim.x = ceil(2 * vdim.z / tan(radians(30))) + abs(-1.0f * (float)pdim.x * sin(radians(30)) + (cos(radians(30)) / sin(radians(30))) * ((float)vdim.z - (float)pdim.x * cos(radians(30))));
		vdim.y = tile_size + number_extra_rows; //Additional rows above and below for interpolation
	}
	else
	{
		vdim.z = volume_depth;
		vdim.x = ceil(2 * vdim.z / tan(radians(30))) + abs(-1.0f * (float)pdim.x * sin(radians(30)) + (cos(radians(30)) / sin(radians(30))) * ((float)vdim.z - (float)pdim.x * cos(radians(30))));
		vdim.y = pdim.y; //Additional rows above and below for interpolation
	}
	padding = (vdim.x - pdim.x) / 2;

	vdelta.x = 0.5 * (vdim.x - 1); vdelta.y = 0.5 * (pdim.y - 1); vdelta.z= 0.5 * (vdim.z - 1);  //ע���м��yֵ
	pdelta.x = 0.5 * (pdim.x - 1); pdelta.y=0.5 * (pdim.y - 1);

	//Distance between origin and source plane
	//5% Bigger than volume's xz diagonal
	source_object_distance = 1.05 * (sqrt((float)(vdim.x * vdim.x) + (float)(vdim.z * vdim.z)));
	M = ceil((2.0 * (source_object_distance + vdelta.z)) / sample_rate);
	//Get the sizes in bytes
	if (ifcrop) {
		data_size = h_crop_proj_data.size() * sizeof(float); //Original projection data size
	}
	else {
		data_size = h_proj_data.size() * sizeof(float); //Original projection data size
	}
	
	recon_size = vdim.x * vdim.y * vdim.z * sizeof(float); //Reconstructed volume size
	proj_size = pdim.x * pdim.y * sizeof(float); //Size of projection image, correction image, ray length image
	crop_size = pdim.x * tile_size * vdim.z * sizeof(float);

	h_rec_vol= CImgFloat(pdim.x, pdim.y, vdim.z, 1, 0.0f);//for host save
}

int main(int /*argc*/, char* /*argv*/[])
{
	//����һ�д������Branch����
	Initial();
	init_LADMM();
	cout << "open file" << filename << endl;
	printf("%d projections of size: %d, %d loaded succesfully.\n", pdim.z, pdim.x, pdim.y);
	if (ifcrop) {
		printf("Using Crop data to reconstruct");
	}
		
	printf("Projection order:\n");
	for (int i = 0; i < pdim.z - 1; i++)
		printf("%d, ", projections[i]);
	printf("%d\n", projections[pdim.z - 1]);
	printf("Rec. vol. size: %d x %d x %d\n", vdim.x, vdim.y, vdim.z);
	printf("padding: %d\n", padding);
	printf("vdelta: %f, %f, %f\n", vdelta.x, vdelta.y, vdelta.z);
	printf("pdelta: %f, %f\n", pdelta.x, pdelta.y);
	printf("sod: %f\n", source_object_distance);
	printf("M: %d\n", M);
	printf("Data size: %f MB\n", (float)data_size / 1000000);
	printf("Recon size: %f GB\n", (float)recon_size / 1000000000);
	printf("Proj size: %f MB\n", (float)proj_size / 1000000);
	cout << "******************** Initial Done *************************"<<endl;
	cout << endl;
	cout << endl;

	//����GPU Device
	wgpuDevice = SARTcreateDevice();
	// create cpp device
	device = wgpu::Device(wgpuDevice);

	AllDim.pdim_x = pdim.x; AllDim.pdim_y = pdim.y; AllDim.pdim_z = pdim.z;
	AllDim.vdim_x = vdim.x; AllDim.vdim_y = vdim.y; AllDim.vdim_z = vdim.z;
	AllDim.vdelta_x = vdelta.x; AllDim.vdelta_y = vdelta.y; AllDim.vdelta_z = vdelta.z;
	AllDim.pdelta_x = pdelta.x; AllDim.pdelta_y = pdelta.y;
	printf("pdim=%d Byte, vdim=%d Byte, pdelta=%d Byte, vdelta=%d Byte.\n", sizeof(pdim), sizeof(vdim), sizeof(pdelta), sizeof(vdelta));//12 12 8 12 Byte
	printf("AllDim=%d Byte",sizeof(AllDim)); //44Byte

	floatVAR.dis_vs = source_object_distance;
	floatVAR.sample_rate = sample_rate;
	floatVAR.chillfactor = chillfactor;

	intVAR.sample_num = M;
	intVAR.current_projection = current_projection;//�������ʼ����
	intVAR.current_tile = current_tile;
	intVAR.tile_size = tile_size;
	intVAR.number_extra_row = number_extra_rows;
	intVAR.padding = padding;

	//�Ȳ鿴linearized���
	if (ifcrop) {
		Ori_pro_buffer = utils::CreateBufferFromData(device, h_crop_proj_data._data, data_size, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);//�����Ҫ�����Ƶ�CPU���ڴ���
	}
	else {
		Ori_pro_buffer = utils::CreateBufferFromData(device, h_proj_data._data, data_size, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);//�����Ҫ�����Ƶ�CPU���ڴ���
	}
	
	Cal_rec_buffer = utils::CreateBuffer(device, recon_size, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);//for reconstuction
	H_rec_buffer = utils::CreateBuffer(device, recon_size, wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst);//for reconstuction
	Dim_buffer = utils::CreateBufferFromData(device,(void*)&AllDim, sizeof(AllDim), wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopySrc);  // Save the  volume dim 
	ray_dir_buffer= utils::CreateBuffer(device, sizeof(dir), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);
	flo_var_buffer= utils::CreateBufferFromData(device,(void*)&floatVAR, sizeof(floatVAR), wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopySrc);
	int_var_buffer= utils::CreateBuffer(device, sizeof(intVAR), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);
	cor_img_buffer= utils::CreateBuffer(device,proj_size, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);
	pip_buffer = utils::CreateBuffer(device, sizeof(pip), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);
	per_crop_buffer= utils::CreateBuffer(device, crop_size, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);
	LADMM_Paras_buffer = utils::CreateBufferFromData(device, (void*)&LADMMParameters, sizeof(LADMMParameters), wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopySrc);

	
	linearize_projection();// ���ԵĽ���Աȿ�����Ϊ����ȷ��
	Max = utils::CreateBufferFromData(device, &MAX, sizeof(MAX), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);//�����Ҫ�����Ƶ�CPU���ڴ���,has max value after delinearizing
	LADMM();
	
	/*
	linearize_projection();// ���ԵĽ���Աȿ�����Ϊ����ȷ��
	Max = utils::CreateBufferFromData(device, &MAX, sizeof(MAX), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);//�����Ҫ�����Ƶ�CPU���ڴ���,has max value after delinearizing
	SART();
	printf("Main:  MAX=%f", MAX);
	string save_result = ".\\test\\REC_it" + std::to_string(data_term_iters) + "_SART_"+".hdr";//���+��������+�㷨
	h_rec_vol.save_analyze(save_result.c_str());
	*/

	


	return 0;
}

