#include"LADMM/LADMM.h"
#include "dawn/webgpu_cpp.h"
#include "SART/SART.h"
#include <iostream>

using namespace std;

float mu;//μ used to control proximal of date term for fidelity
float ro;//ρ used to control proximal of regularization term
float L2_Norm_K;// L2 norm of matrix K
float lambda;
float sigma;

//需要使用全局变量要使用extern语句
#include "ExternGlobalVariables.h"
void init_LADMM() {
	L2_Norm_K = 11.99f;//set a default value for it
	ro = 50;
	mu= 0.99f/(ro * L2_Norm_K);
	lambda = sqrt(2 * 1000);
	sigma = 0.01;
	LADMMParameters.Var_mu = mu;
	LADMMParameters.Var_ro = ro;
	LADMMParameters.lambda = lambda;
	LADMMParameters.sigma = sigma;
}
/*
input: a Volume needed for caculating its gradients, a buffer to save its gradients
output: in fact, it produces the gradients
*/
void VolumeGradient(wgpu::Buffer Volume, wgpu::Buffer z_Gradient) {
	//1. 首先为了与 GPU 进行数据交换，需要一个叫绑定组的布局对象（类型是 GPUBindGroupLayout）来扩充管线的定义，
	//这里可以提前预知这个布局要处理的资源类型（Buffer、Texture、Sampler）
	//从而得到更快的处理速度
	auto bindGroupLayout = utils::MakeBindGroupLayout(
		device, {
			{0, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // for volume data
			{1, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // for volume gradients data
			{2, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform},   // for dim data
			{3, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform},   // for float data
			{4, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // for int data
			//{bindingNum,ShaderSatge,BindingType},
		});

	//2. Create BindGroup
	wgpu::BindGroup ComputeBindGroup = utils::MakeBindGroup(
		device, bindGroupLayout, {
			{0, Cal_rec_buffer, 0, recon_size},
			{1, ADMM_z_buffer, 0, recon_size*3},
			{2, Dim_buffer, 0,   sizeof(AllDim)},
			{3, flo_var_buffer, 0,  sizeof(floatVAR)},
			{4, int_var_buffer, 0,  sizeof(intVAR)},
			//BindingInitializationHelper->{bingding序号，buffer，offest,size}
		});

	//3. pass the Layout for the pipeline Layout
	wgpu::PipelineLayout pipelineLayout = utils::MakeBasicPipelineLayout(device, &bindGroupLayout); //后面应该可以直接使用pipelineLayout

	//准备创建pipeline，需要准备shader,入口信息
	//创建shader
	std::string shaderFile("work/shader/volumegradients.fs");
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
	auto queue = device.GetQueue();

	//5. 创建指令编码器
	wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

	wgpu::CommandEncoder WLPencoder = device.CreateCommandEncoder();
	wgpu::ComputePassDescriptor compassdescriptor;
	compassdescriptor.label = "volume gradient pass";
	wgpu::ComputePassEncoder computepass;

	computepass = encoder.BeginComputePass(&compassdescriptor);
	computepass.SetPipeline(computepipeline);
	computepass.SetBindGroup(0, ComputeBindGroup);

	int workgroupCountX = ceil((float)vdim.x / 8.0);	//这里每次都需要调整
	int workgroupCountY = ceil((float)vdim.y / 8.0);   //这里每次都需要调整，需要跟shader对应
	int workgroupCountZ = ceil((float)vdim.y / 8.0);   //这里每次都需要调整，需要跟shader对应

	computepass.DispatchWorkgroups(workgroupCountX, workgroupCountY, workgroupCountZ); //运行shader
	computepass.End();
	wgpu::CommandBuffer commands = encoder.Finish();
	queue.Submit(1, &commands);  //提交到GPU的指令队列中运行

	return;
}
float VolumeNorm(wgpu::Buffer Volume) {
	printf("[ *LADMM_CPP* :VolumeNorm starting...] \n");
	wgpu::Buffer cal_norm_per_slice = utils::CreateBuffer(device, sizeof(float) * pdim.z, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);
	wgpu::Buffer h_norm_per_slice = utils::CreateBuffer(device, sizeof(float) * pdim.z, wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst);//for copy to CPU

	//1.bindGroupLayout 绑定组布局
	auto bindGroupLayout = utils::MakeBindGroupLayout(
		device, {
			{0, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // Volume data
			{1, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform},   // Dim
			{2, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // cal_max_per_slice

		});
	//2. 绑定组
	wgpu::BindGroup ComputeBindGroup = utils::MakeBindGroup(
		device, bindGroupLayout, {
			{0, Cal_rec_buffer, 0, recon_size},
			{1, Dim_buffer, 0,   sizeof(AllDim)},//需要全部数据的大小
			{2, cal_norm_per_slice, 0,   sizeof(float) * pdim.z},
		});

	//3.把布局设置到管道
	wgpu::PipelineLayout pipelineLayout = utils::MakeBasicPipelineLayout(device, &bindGroupLayout);
	////创建shader
	std::string shaderFile("work/shader/VolumeNorm.wgsl");
	wgpu::ShaderModule ShaderModule = ShaderUtil::loadAndCompileShader(device, shaderFile);

	//4.设置计算通道  目测每次只需要改变这个即可
	//setup compute pipeline
	wgpu::ComputePipelineDescriptor comDescriptor;
	wgpu::ProgrammableStageDescriptor proDescriptor;
	proDescriptor.entryPoint = "main";
	proDescriptor.module = ShaderModule;
	comDescriptor.layout = pipelineLayout;
	comDescriptor.compute = proDescriptor;

	//5.创建计算通道
	wgpu::ComputePipeline computepipeline = device.CreateComputePipeline(&comDescriptor);

	//6. 创建指令编码器
	wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

	//7.获取指令队列
	auto queue = device.GetQueue();

	wgpu::ComputePassDescriptor passdescriptor;
	passdescriptor.label = "Normcompute pass";
	wgpu::ComputePassEncoder computepass;
	int workgroupCountX = ceil(float(pdim.x) / 8);
	int workgroupCountY = ceil(float(pdim.y) / 8);
	int workgroupCountZ = ceil(float(pdim.z) / 8);

	//8. 设置管道指令
	computepass = encoder.BeginComputePass(&passdescriptor);
	computepass.SetPipeline(computepipeline);
	computepass.SetBindGroup(0, ComputeBindGroup);
	computepass.DispatchWorkgroups(1, 1, workgroupCountZ);//1行1列1纵队的工作组
	computepass.End();

	//9.获取指令集并执行
	encoder.CopyBufferToBuffer(cal_norm_per_slice, 0, h_norm_per_slice, 0, sizeof(float) * pdim.z);
	printf("[ *LADMM_CPP* :Norm Value Buffer copied...] \n");
	wgpu::CommandBuffer commands = encoder.Finish();
	queue.Submit(1, &commands);

	struct mydata {
		bool IfFinish;
	};
	mydata* UserDataTest = new mydata;
	UserDataTest->IfFinish = false;

	h_norm_per_slice.MapAsync(wgpu::MapMode::Read, 0, sizeof(float) * pdim.z, [](WGPUBufferMapAsyncStatus status, void* userdata) {
		if (status == WGPUBufferMapAsyncStatus_Success) {
			mydata* TempUserData = static_cast<mydata*>(userdata);
			TempUserData->IfFinish = true;
			printf("Mapped!\n");

		}
		}, UserDataTest);
	while (!(UserDataTest->IfFinish)) {
		Sleep(100);
		printf("Waiting for Buffer Mappping...\n");
		device.Tick();
	}

	float* NormSlice = (float*)h_norm_per_slice.GetConstMappedRange(0, sizeof(float) * pdim.z);
	float sum = 0.0;
	for (int i = 0; i < pdim.z; i++) {
		sum += NormSlice[i];
	}
	printf("[ *LADMM_CPP* :Volume Norm= %f ] \n", sum);

	return sum;

}
//One thread/voxel
//x = x - mu*ro*trans(K)*(K*x - z + y)
//Check TRex algorithm 4 for details
void update_x_ladmm() {
	printf("[ *LADMM_CPP* :update_x_ladmm starting...] \n");

	//1.bindGroupLayout 绑定组布局
	auto bindGroupLayout = utils::MakeBindGroupLayout(
		device, {
			{0, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // Volume data->x
			{1, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // volume_gradients ->z
			{2, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // TV ,y = y + Kx - z  ->y
			{3, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform},	 //DIM
			{4, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform},	 //mu,ro for LADMM
		});
	//2. 绑定组
	wgpu::BindGroup ComputeBindGroup = utils::MakeBindGroup(
		device, bindGroupLayout, {
			{0, Cal_rec_buffer, 0, recon_size},
			{1, ADMM_z_buffer, 0,   recon_size*3},
			{2, ADMM_y_buffer, 0,   recon_size * 3},
			{3, Dim_buffer, 0,   sizeof(AllDim)},//需要全部数据的大小
			{4, LADMM_Paras_buffer, 0,  sizeof(LADMMParameters)},
		});

	//3.把布局设置到管道
	wgpu::PipelineLayout pipelineLayout = utils::MakeBasicPipelineLayout(device, &bindGroupLayout);
	////创建shader
	std::string shaderFile("work/shader/update_x_kernel.wgsl");
	wgpu::ShaderModule ShaderModule = ShaderUtil::loadAndCompileShader(device, shaderFile);

	//4.设置计算通道  目测每次只需要改变这个即可
	//setup compute pipeline
	wgpu::ComputePipelineDescriptor comDescriptor;
	wgpu::ProgrammableStageDescriptor proDescriptor;
	proDescriptor.entryPoint = "main";
	proDescriptor.module = ShaderModule;
	comDescriptor.layout = pipelineLayout;
	comDescriptor.compute = proDescriptor;

	//5.创建计算通道
	wgpu::ComputePipeline computepipeline = device.CreateComputePipeline(&comDescriptor);

	//6. 创建指令编码器
	wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

	//7.获取指令队列
	auto queue = device.GetQueue();

	wgpu::ComputePassDescriptor passdescriptor;
	passdescriptor.label = "update_x_kernel pass";
	wgpu::ComputePassEncoder computepass;
	int workgroupCountX = ceil(float(pdim.x) / 8);
	int workgroupCountY = ceil(float(pdim.y) / 8);
	int workgroupCountZ = ceil(float(pdim.z) / 8);

	//8. 设置管道指令
	computepass = encoder.BeginComputePass(&passdescriptor);
	computepass.SetPipeline(computepipeline);
	computepass.SetBindGroup(0, ComputeBindGroup);
	computepass.DispatchWorkgroups(workgroupCountX, workgroupCountY, workgroupCountZ);//
	computepass.End();

	//9.获取指令集并执行
	wgpu::CommandBuffer commands = encoder.Finish();
	queue.Submit(1, &commands);

	return;
}
void Proximal_SART() {
	printf("[ *LADMM_CPP* :**********Proximal SART Starting... *********] \n");
	
	/*
	float* tempzero = (float*)malloc(recon_size);
	memset(tempzero, 0, recon_size);
	queue.WriteBuffer(Cal_rec_buffer, 0, (void*)tempzero, recon_size);//Clear the calculate
	*/

	time(&start);
	//Forward projection and BackProjection
	auto bindGroupLayout = utils::MakeBindGroupLayout(
		device, {
			{0, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // for projection data
			{1, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // for calculating data
			{2, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform},   // for DIM data
			{3, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // for ray dir data
			{4, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform},   // for float var
			{5, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // for int var
			{6, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // for cor_img 
			{7, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // for pip 
			{8, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform},   // for LADMM parameters 
			{9, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // for y_slack
		});
	//Create BindGroup
	wgpu::BindGroup ComputeBindGroup = utils::MakeBindGroup(
		device, bindGroupLayout, {
			{0, Ori_pro_buffer, 0, data_size},
			{1, Cal_rec_buffer, 0, recon_size},
			{2, Dim_buffer, 0,   sizeof(AllDim)},
			{3, ray_dir_buffer, 0,  sizeof(dir)},
			{4, flo_var_buffer, 0,  sizeof(floatVAR)},
			{5, int_var_buffer, 0,  sizeof(intVAR)},
			{6, cor_img_buffer, 0,  proj_size},
			{7, pip_buffer, 0,  sizeof(pip)},
			{8, LADMM_Paras_buffer, 0,  sizeof(LADMMParameters)},
			{9, SART_y_buffer, 0,  proj_size},
		});
	//pass the Layout for the pipeline
	wgpu::PipelineLayout pipelineLayout = utils::MakeBasicPipelineLayout(device, &bindGroupLayout); //后面应该可以直接使用pipelineLayout

	////创建shader
	std::string shaderPath("work/shader/");
	std::string FPshaderFile(shaderPath + "prox_forwardprojection.wgsl");
	wgpu::ShaderModule FPShaderModule = ShaderUtil::loadAndCompileShader(device, FPshaderFile);
	std::string BPshaderFile(shaderPath + "prox_backprojection.wgsl");
	wgpu::ShaderModule BPShaderModule = ShaderUtil::loadAndCompileShader(device, BPshaderFile);

	//setup FP compute pipeline
	wgpu::ComputePipelineDescriptor FPcomDescriptor;
	wgpu::ProgrammableStageDescriptor FPproDescriptor;
	FPproDescriptor.entryPoint = "FPmain";
	FPproDescriptor.module = FPShaderModule;
	FPcomDescriptor.layout = pipelineLayout;
	FPcomDescriptor.compute = FPproDescriptor;
	//Create compute pipeline
	wgpu::ComputePipeline FPcomputepipeline = device.CreateComputePipeline(&FPcomDescriptor);

	//setup BP compute pipeline
	wgpu::ComputePipelineDescriptor BPcomDescriptor;
	wgpu::ProgrammableStageDescriptor BPproDescriptor;
	BPproDescriptor.entryPoint = "BPmain";
	BPproDescriptor.module = BPShaderModule;
	BPcomDescriptor.layout = pipelineLayout;
	BPcomDescriptor.compute = BPproDescriptor;
	//Create compute pipeline
	wgpu::ComputePipeline BPcomputepipeline = device.CreateComputePipeline(&BPcomDescriptor);

	auto queue = device.GetQueue();
	//配置FPencoder
	wgpu::CommandEncoder fpencoder;
	wgpu::ComputePassDescriptor fpcompassdescriptor;
	fpcompassdescriptor.label = "FPcompute pass";
	wgpu::ComputePassEncoder FPcomputepass;
	int fpworkgroupCountX = ceil((float)pdim.x / 8.0);	//这里是pdimx
	int fpworkgroupCountY = ceil((float)vdim.y / 8.0);   //这里要求是vdim.y来进行 因为存在extra_rows
	//配置BPencoder
	wgpu::CommandEncoder bpencoder;
	wgpu::ComputePassDescriptor bpcompassdescriptor;
	bpcompassdescriptor.label = "BPcompute pass";
	wgpu::ComputePassEncoder BPcomputepass;
	int bpworkgroupCountX = ceil((float)vdim.x / 8.0);
	int bpworkgroupCountY = ceil((float)vdim.y / 8.0);
	int bpworkgroupCountZ = ceil((float)vdim.z / 8.0);
	for (int data_iters = 0; data_iters < data_term_iters; data_iters++)
	{
		//Shuffle projection order
		if ((current_tile == 0))
		{
			if (random_projection_order)
			{
				std::random_device rd;
				std::mt19937 g(rd());
				std::shuffle(projections.begin(), projections.end(), g);
			}
			pw_projections.push_back(projections);
		}
		for (unsigned int i = 0; i < pdim.z; i++)
		{
			current_projection = pw_projections[data_iters][i];

			//Angle required in radians for cos and sin functions
			float angle = get_angle();
			angle = radians(angle);

			//Update cosine and sine for point rotations
			cost = cos(angle);
			sint = sin(angle);

			dir.x = sint; dir.y = 0; dir.z = cost;
			//printf("dir=(%f,%f,%f), current_projection=%d \n", dir.x, dir.y, dir.z, current_projection);

			pip.x = sint * (vdelta.z + source_object_distance); pip.y = 0; pip.z = cost * (vdelta.z + source_object_distance);
			//printf("pip=(%f,%f,%f)\n", pip.x, pip.y, pip.z);

			//Perform forward projection
			//Create command encoder
			fpencoder = device.CreateCommandEncoder();
			FPcomputepass = fpencoder.BeginComputePass(&fpcompassdescriptor);
			FPcomputepass.SetPipeline(FPcomputepipeline);
			FPcomputepass.SetBindGroup(0, ComputeBindGroup);
			intVAR.sample_num = M;
			intVAR.current_projection = current_projection;
			intVAR.current_tile = current_tile;
			intVAR.tile_size = tile_size;
			intVAR.number_extra_row = number_extra_rows;

			queue.WriteBuffer(int_var_buffer, 0, (void*)&intVAR, sizeof(intVAR));//update the projection angel
			queue.WriteBuffer(ray_dir_buffer, 0, (void*)&dir, sizeof(dir));//update the projection angel
			queue.WriteBuffer(pip_buffer, 0, (void*)&pip, sizeof(pip));//update the projection angel
			//都成功测试可以完整保留数据到shader中

			FPcomputepass.DispatchWorkgroups(fpworkgroupCountX, fpworkgroupCountY); //运行shader
			FPcomputepass.End();
			wgpu::CommandBuffer commands = fpencoder.Finish();
			queue.Submit(1, &commands);

			//std::string file = "Corimg_TILE" + std::to_string(current_tile) + "_PRO" + std::to_string(current_projection);
			//int tempdim[3] = { pdim.x,pdim.y,1 };
			//Buffer_save(cor_img_buffer, proj_size, tempdim, file);

			//下面是BP
			//Create command encoder
			bpencoder = device.CreateCommandEncoder();
			BPcomputepass = bpencoder.BeginComputePass(&bpcompassdescriptor);
			BPcomputepass.SetPipeline(BPcomputepipeline);
			BPcomputepass.SetBindGroup(0, ComputeBindGroup);
			BPcomputepass.DispatchWorkgroups(bpworkgroupCountX, bpworkgroupCountY, bpworkgroupCountZ); //运行shader
			BPcomputepass.End();
			wgpu::CommandBuffer bpcommands = bpencoder.Finish();
			queue.Submit(1, &bpcommands);


		}
	}

	time(&time_end);
	float cost = difftime(time_end, start);
	printf("[ *LADMM_CPP* :Proximal SART time consumption: %f s] \n", cost);
}
void update_z_ladmm_tv() {
	printf("[ *LADMM_CPP* :update_z_ladmm_tv starting...] \n");

	//1.bindGroupLayout 绑定组布局
	auto bindGroupLayout = utils::MakeBindGroupLayout(
		device, {
			{0, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // Volume data->x
			{1, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // TV ,y = y + Kx - z  ->y
			{2, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // volume_gradients ->z
			{3, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform},	 //DIM
			{4, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform},	 //mu,ro,lambda,sigma for LADMM
		});
	//2. 绑定组
	wgpu::BindGroup ComputeBindGroup = utils::MakeBindGroup(
		device, bindGroupLayout, {
			{0, Cal_rec_buffer, 0, recon_size},//x
			{1, ADMM_y_buffer, 0,   recon_size * 3},//y
			{2, ADMM_z_buffer, 0,   recon_size * 3},//z
			{3, Dim_buffer, 0,   sizeof(AllDim)},//需要全部数据的大小
			{4, LADMM_Paras_buffer, 0,  sizeof(LADMMParameters)},
		});

	//3.把布局设置到管道
	wgpu::PipelineLayout pipelineLayout = utils::MakeBasicPipelineLayout(device, &bindGroupLayout);
	////创建shader
	std::string shaderFile("work/shader/update_z_kernel_ATV.wgsl");
	wgpu::ShaderModule ShaderModule = ShaderUtil::loadAndCompileShader(device, shaderFile);

	//4.设置计算通道  目测每次只需要改变这个即可
	//setup compute pipeline
	wgpu::ComputePipelineDescriptor comDescriptor;
	wgpu::ProgrammableStageDescriptor proDescriptor;
	proDescriptor.entryPoint = "main";
	proDescriptor.module = ShaderModule;
	comDescriptor.layout = pipelineLayout;
	comDescriptor.compute = proDescriptor;

	//5.创建计算通道
	wgpu::ComputePipeline computepipeline = device.CreateComputePipeline(&comDescriptor);

	//6. 创建指令编码器
	wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

	//7.获取指令队列
	auto queue = device.GetQueue();

	wgpu::ComputePassDescriptor passdescriptor;
	passdescriptor.label = "update_z_kernel_ATV pass";
	wgpu::ComputePassEncoder computepass;
	int workgroupCountX = ceil(float(pdim.x) / 8);
	int workgroupCountY = ceil(float(pdim.y) / 8);
	int workgroupCountZ = ceil(float(pdim.z) / 8);

	//8. 设置管道指令
	computepass = encoder.BeginComputePass(&passdescriptor);
	computepass.SetPipeline(computepipeline);
	computepass.SetBindGroup(0, ComputeBindGroup);
	computepass.DispatchWorkgroups(workgroupCountX, workgroupCountY, workgroupCountZ);//
	computepass.End();

	//9.获取指令集并执行
	wgpu::CommandBuffer commands = encoder.Finish();
	queue.Submit(1, &commands);

	return;

}

//y = y + Kx - z
void update_y_ladmm() {
	printf("[ *LADMM_CPP* :update_y_ladmm starting...] \n");

	//1.bindGroupLayout 绑定组布局
	auto bindGroupLayout = utils::MakeBindGroupLayout(
		device, {
			{0, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // Volume data->x
			{1, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // TV ,y = y + Kx - z  ->y
			{2, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // volume_gradients ->z
			{3, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform},	 //DIM
			{4, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Uniform},	 //mu,ro,lambda,sigma for LADMM
		});
	//2. 绑定组
	wgpu::BindGroup ComputeBindGroup = utils::MakeBindGroup(
		device, bindGroupLayout, {
			{0, Cal_rec_buffer, 0, recon_size},//x
			{1, ADMM_y_buffer, 0,   recon_size * 3},//y
			{2, ADMM_z_buffer, 0,   recon_size * 3},//z
			{3, Dim_buffer, 0,   sizeof(AllDim)},//需要全部数据的大小
			{4, LADMM_Paras_buffer, 0,  sizeof(LADMMParameters)},
		});

	//3.把布局设置到管道
	wgpu::PipelineLayout pipelineLayout = utils::MakeBasicPipelineLayout(device, &bindGroupLayout);
	////创建shader
	std::string shaderFile("work/shader/update_y_kernel.wgsl");
	wgpu::ShaderModule ShaderModule = ShaderUtil::loadAndCompileShader(device, shaderFile);

	//4.设置计算通道  目测每次只需要改变这个即可
	//setup compute pipeline
	wgpu::ComputePipelineDescriptor comDescriptor;
	wgpu::ProgrammableStageDescriptor proDescriptor;
	proDescriptor.entryPoint = "main";
	proDescriptor.module = ShaderModule;
	comDescriptor.layout = pipelineLayout;
	comDescriptor.compute = proDescriptor;

	//5.创建计算通道
	wgpu::ComputePipeline computepipeline = device.CreateComputePipeline(&comDescriptor);

	//6. 创建指令编码器
	wgpu::CommandEncoder encoder = device.CreateCommandEncoder();

	//7.获取指令队列
	auto queue = device.GetQueue();

	wgpu::ComputePassDescriptor passdescriptor;
	passdescriptor.label = "update_y_kernel pass";
	wgpu::ComputePassEncoder computepass;
	int workgroupCountX = ceil(float(pdim.x) / 8);
	int workgroupCountY = ceil(float(pdim.y) / 8);
	int workgroupCountZ = ceil(float(pdim.z) / 8);

	//8. 设置管道指令
	computepass = encoder.BeginComputePass(&passdescriptor);
	computepass.SetPipeline(computepipeline);
	computepass.SetBindGroup(0, ComputeBindGroup);
	computepass.DispatchWorkgroups(workgroupCountX, workgroupCountY, workgroupCountZ);//
	computepass.End();

	//9.获取指令集并执行
	wgpu::CommandBuffer commands = encoder.Finish();
	queue.Submit(1, &commands);

	return;

}
void LADMM(int data_term, int regularization_term) {
	//mu > 0
	if (!((mu > 0) && (mu * ro * L2_Norm_K <= 1)))   //这里是LADMM的收敛性条件 
	{
		printf("[ *LADMM_CPP* :Error in mu/rho LADMM parameters!] \n");
		printf("[ *LADMM_CPP* : L2_Norm_K=%f  , ro=%.2f, mu=%.10f  ]\n", L2_Norm_K, ro, mu);
		return;
	}
	printf("[ *LADMM_CPP* : L2_Norm_K=%f  , ro=%.2f, mu=%.10f, lambda=%f.  ]\n", L2_Norm_K, ro, mu,0.5* lambda*lambda);
	printf("[ *LADMM_CPP* :  ***Starting LADMM reconstruction. *** ]\n");

	float* tempzero = (float*)malloc(recon_size);
	memset(tempzero, 0, recon_size);
	
	//下面开始分tile重建
	//number_of_tiles = 1 for non tiled
	for (int j = 0; j < number_of_tiles; j++)
	{
		//Update current piece
		auto queue = device.GetQueue();

		current_tile = j;
		h_SART_y = CImgFloat(pdim.x, pdim.y, 1, 1, 0.0f);   //这个是在迭代SART算法使用的中间变量，  初始化为0
		wgpu::SupportedLimits nowLimits;
		device.GetLimits(&nowLimits);
		SART_y_buffer= utils::CreateBufferFromData(device, h_SART_y._data, proj_size, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);  // Save the  volume dim 

		h_ADMM_y= CImgFloat(vdim.x, vdim.y, vdim.z, 3, 0.0f);//注意这里面的三通道 
		ADMM_y_buffer= utils::CreateBufferFromData(device, h_ADMM_y._data, 3*recon_size, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);  // y = y + Kx - z
		ADMM_z_buffer= utils::CreateBuffer(device, 3 * recon_size, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);// 都是3*recon_size
		queue.WriteBuffer(Cal_rec_buffer, 0, (void*)tempzero, recon_size);//初始化X

		//Initialize z = K*x = volume's gradient
		VolumeGradient(Cal_rec_buffer, ADMM_z_buffer);
		//printf("*LADMM_CPP* :VolumeGradient Finish.\n");
		//std::string testfilename = "LADMM_REC_TILE_Volume_Gradients" + std::to_string(current_tile);
		//int tempdim3[3] = { vdim.x,vdim.y,vdim.z };
		//Buffer_save(ADMM_z_buffer, recon_size*3, tempdim3, testfilename,3);

		for (int i = 0; i < proximal_iters; i++) {
			time(&start);
			//Check convergence every 5 iterations,只在第一个分块中
			if (check_convergence && (current_tile == 0) && ((i + 1) % 5 == 0))
			{
				float current_tile_norm = VolumeNorm(Cal_rec_buffer);
				//If converged
				if (abs(tile_norm - current_tile_norm) < tile_norm / 1000)
				{
					//Set new iteration number and go to next tile
					printf("[ *LADMM_CPP* :First tile converged.] \n");
					printf("[ *LADMM_CPP* :Norm: %f, difference: %f] \n", current_tile_norm, abs(tile_norm - current_tile_norm));
					proximal_iters = i;
					break;//这个时候找到了一个最快的收敛次数，并且推出了循环，之后的分块在运行过程中以新的proximal 迭代次数为标准
				}
				else
				{
					printf("[Norm of current tile: %f, difference: %f ]\n", current_tile_norm, abs(tile_norm - current_tile_norm));
					tile_norm = current_tile_norm;
				}
			}
			printf("[ *LADMM_CPP* :Tile %d, iteration %d/%d.] \n" ,current_tile, i + 1, proximal_iters);

			//Update x before applying the P.O.
			//x = x + mu*ro *trans(K)*(z - y -K*x)
			update_x_ladmm();
			printf("[ *LADMM_CPP* :update_x_ladmm Finish.] \n");

			//Then, use SART to update x and y_slack
			Proximal_SART();

			//Update z with proximal operator TODO, CHECK TREX 5, The regularization term
			update_z_ladmm_tv();

			//update y following line 5 algorithm 4 TRex paper
			//y = y + Kx - z
			update_y_ladmm();

			
		}
		time(&time_end);
		float cost = difftime(time_end, start);
		printf("SART time consumption: %f s\n", cost);
		//int tempdim[3] = { vdim.x,vdim.y,vdim.z };
		//string save_result = "proximal_iter" + std::to_string(proximal_iters) + "_" + std::to_string(current_tile)+ "_";//类别+迭代次数+算法
		//Buffer_save(Cal_rec_buffer, recon_size, tempdim, save_result);
		//Test successfully

		//Delinearize
		DeLinearize_Projections(Cal_rec_buffer, recon_size, MAX);//Delinearizing 

		//crop data
		Crop_data(Cal_rec_buffer, recon_size, per_crop_buffer, crop_size);

		std::string file2 = "CopyTile" + std::to_string(current_tile);
		Piece_Buffer_copy(per_crop_buffer, crop_size, file2);
	}
	//命名格式算法-proximal迭代次数-SART迭代次数
	string save_result = ".\\test\\REC_LADMM_proxit_" + std::to_string(proximal_iters) + "_"+ std::to_string(data_term_iters) + ".hdr";//类别+迭代次数+算法
	h_rec_vol.save_analyze(save_result.c_str());
}