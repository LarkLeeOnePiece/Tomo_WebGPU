#include "SART/SART.h"



//引入全局变量
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

extern DIM  AllDim;
extern INTVAR intVAR;
extern FLOATVAR floatVAR;

extern float MAX;
extern float starting_angle;
extern float angle_step;
extern float source_object_distance;
extern float sample_rate;
extern float cost;
extern float sint;
extern float chillfactor;;

extern string filename;

extern bool random_projection_order;
extern bool tiled;

extern std::vector<int> projections;
extern std::vector<std::vector<int>> pw_projections;

extern CImgFloat h_rec_vol;

//Get radians from angle
#define PI 3.14159265358979323846
float radians(float angle)
{
	return (angle * PI) / (float)180;
}

float get_angle()
{
	return starting_angle + angle_step * current_projection;
}

//传入原始GPU,函数会自动复制，并保存为图像文件


void Buffer_save(wgpu::Buffer gpubuffer, int buffer_size, int dim[3], string dataname) {
	struct mydata {
		bool IfFinish;
	};
	mydata* UserDataTest = new mydata;
	UserDataTest->IfFinish = false;

	wgpu::CommandEncoder testencoder = device.CreateCommandEncoder();
	wgpu::Buffer host_buffer = utils::CreateBuffer(device, buffer_size, wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst);//save to host 
	testencoder.CopyBufferToBuffer(gpubuffer, 0, host_buffer, 0, buffer_size);
	wgpu::CommandBuffer commands = testencoder.Finish();
	auto testqueue = device.GetQueue();
	testqueue.Submit(1, &commands);
	host_buffer.MapAsync(wgpu::MapMode::Read, 0, buffer_size, [](WGPUBufferMapAsyncStatus status, void* userdata) {
		if (status == WGPUBufferMapAsyncStatus_Success) {
			mydata* TempUserData = static_cast<mydata*>(userdata);
			TempUserData->IfFinish = true;
			printf("\nMapped!\n");
		}
		}, UserDataTest);
	while (!(UserDataTest->IfFinish)) {
		Sleep(100);
		printf("Waiting for Buffer Mappping...\r");
		device.Tick();
	}

	typedef cimg_library::CImg<float> CImgFloat;		//Convenient for saving data as .hdr
	CImgFloat temp_Img_data;
	temp_Img_data = CImgFloat(dim[0], dim[1], dim[2], 1, 0.0f);  //This is for the whole volume without padding
	temp_Img_data._data = (float*)host_buffer.GetConstMappedRange(0, buffer_size);
	string file_save_path = ".\\test\\data" + dataname + ".hdr";
	temp_Img_data.save_analyze(file_save_path.c_str());
			
	//Update filename for ali file
	//string file_save_path = ".\\test\\data_linearized_" + to_string(MAX) + ".ali";
	//Save linearized projections
	//int temppdim[3] = { pdim.x,pdim.y,pdim.z };
	//smrc(file_save_path, temp_Img_data._data, temppdim, 0);

	printf("\n** MySART: RecoVolume Test sucessfully!  File save as : %s\n", file_save_path.c_str());
	temp_Img_data._data = NULL;
	host_buffer.Unmap();
	host_buffer.Destroy();
	delete UserDataTest;
};

void linearize_projection() {
	printf("\nLinearizing projection data...\n");

	wgpu::Buffer Max_buffer = utils::CreateBuffer(device, sizeof(float), wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);
	wgpu::Buffer cal_max_per_slice = utils::CreateBuffer(device, sizeof(float) * pdim.z, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);
	wgpu::Buffer h_max_per_slice = utils::CreateBuffer(device, sizeof(float) * pdim.z, wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst);//for copy to CPU

	//1.bindGroupLayout 绑定组布局
	auto LPbindGroupLayout = utils::MakeBindGroupLayout(
		device, {
			{0, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   //
			{1, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   //
			{2, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   //
			{3, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   //

		});
	//2. 绑定组
	wgpu::BindGroup LPComputeBindGroup = utils::MakeBindGroup(
		device, LPbindGroupLayout, {
			{0, Ori_pro_buffer, 0, data_size},
			{1, Dim_buffer, 0,   sizeof(AllDim)},//需要全部数据的大小
			{2, cal_max_per_slice, 0,   sizeof(float) * pdim.z},
			{3, Max_buffer, 0,   sizeof(float)},
		});

	//3.把布局设置到管道
	wgpu::PipelineLayout LPpipelineLayout = utils::MakeBasicPipelineLayout(device, &LPbindGroupLayout);
	////创建shader
	std::string LPshaderPath("work/shader/");
	std::string LPmsFile(LPshaderPath + "GetMax.fs");
	wgpu::ShaderModule LPShaderModule = ShaderUtil::loadAndCompileShader(device, LPmsFile);

	//4.设置计算通道  目测每次只需要改变这个即可
	//setup compute pipeline
	wgpu::ComputePipelineDescriptor LPDescriptor;
	wgpu::ProgrammableStageDescriptor LPproDescriptor;
	LPproDescriptor.entryPoint = "Maxmain";
	LPproDescriptor.module = LPShaderModule;
	LPDescriptor.layout = LPpipelineLayout;
	LPDescriptor.compute = LPproDescriptor;

	//5.创建计算通道
	wgpu::ComputePipeline LPcomputepipeline = device.CreateComputePipeline(&LPDescriptor);

	//6. 创建指令编码器
	wgpu::CommandEncoder LPencoder = device.CreateCommandEncoder();

	//7.获取指令队列
	auto LPqueue = device.GetQueue();

	wgpu::ComputePassDescriptor LPpassdescriptor;
	LPpassdescriptor.label = "Maxcompute pass";
	wgpu::ComputePassEncoder LPcomputepass;
	int workgroupCountX = ceil(float(pdim.x) / 8);
	int workgroupCountY = ceil(float(pdim.y) / 8);
	int workgroupCountZ = ceil(float(pdim.z) / 8);

	//8. 设置管道指令
	LPcomputepass = LPencoder.BeginComputePass(&LPpassdescriptor);
	LPcomputepass.SetPipeline(LPcomputepipeline);
	LPcomputepass.SetBindGroup(0, LPComputeBindGroup);
	LPcomputepass.DispatchWorkgroups(1, 1, workgroupCountZ);//1行1列1纵队的工作组
	LPcomputepass.End();

	//9.获取指令集并执行
	LPencoder.CopyBufferToBuffer(cal_max_per_slice, 0, h_max_per_slice, 0, sizeof(float) * pdim.z);
	printf("\n** Linearize_Projections:Max Value Buffer copied...\n");
	wgpu::CommandBuffer commands = LPencoder.Finish();
	LPqueue.Submit(1, &commands);
	printf("Linearizing-Get Max_value....\n");

	struct mydata {
		bool IfFinish;
	};
	mydata* UserDataTest = new mydata;
	UserDataTest->IfFinish = false;

	h_max_per_slice.MapAsync(wgpu::MapMode::Read, 0, sizeof(float) * pdim.z, [](WGPUBufferMapAsyncStatus status, void* userdata) {
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

	float* LPMAX = (float*)h_max_per_slice.GetConstMappedRange(0, sizeof(float) * pdim.z);
	MAX = 0.0;
	for (int i = 0; i < pdim.z; i++) {
		//printf("|| %f  ", LPMAX[i]);
		if (LPMAX[i] > MAX) {
			MAX = LPMAX[i];
		}
	}
	printf("\n Linearizing Max=%f\n", MAX);
	int save_linear = 0;
	//printf("是否保存线性化投影结果？ 0-> no , 1-> yes ...");
	//scanf_s("%d", &save_linear);

	//完成最大值获取，下一步直接开始线性化

		////创建shader

	std::string WLPmsFile(LPshaderPath + "Lineraize.fs"); //
	wgpu::ShaderModule WLPShaderModule = ShaderUtil::loadAndCompileShader(device, WLPmsFile);//W意为完整的意思

	//4.设置计算通道  目测每次只需要改变这个即可
	//setup compute pipeline
	wgpu::ComputePipelineDescriptor WLPDescriptor;
	wgpu::ProgrammableStageDescriptor WLPproDescriptor;
	WLPproDescriptor.entryPoint = "LPmain";
	WLPproDescriptor.module = WLPShaderModule;
	WLPDescriptor.layout = LPpipelineLayout;
	WLPDescriptor.compute = WLPproDescriptor;

	//5.创建计算通道
	wgpu::ComputePipeline WLPcomputepipeline = device.CreateComputePipeline(&WLPDescriptor);

	//6. 创建指令编码器
	wgpu::CommandEncoder WLPencoder = device.CreateCommandEncoder();


	//7.获取指令队列
	auto WLPqueue = device.GetQueue();
	WLPqueue.WriteBuffer(Max_buffer, 0, &MAX, sizeof(MAX));


	wgpu::ComputePassDescriptor WLPpassdescriptor;
	WLPpassdescriptor.label = "Whole LPcompute pass";
	wgpu::ComputePassEncoder WLPcomputepass;

	//8. 设置管道指令
	WLPcomputepass = WLPencoder.BeginComputePass(&WLPpassdescriptor);
	WLPcomputepass.SetPipeline(WLPcomputepipeline);
	WLPcomputepass.SetBindGroup(0, LPComputeBindGroup);
	WLPcomputepass.DispatchWorkgroups(workgroupCountX, workgroupCountY, workgroupCountZ);
	WLPcomputepass.End();

	//9.获取指令集并执行
	wgpu::CommandBuffer wcommands = WLPencoder.Finish();
	WLPqueue.Submit(1, &wcommands);

	if (save_linear) {
		//保存线性结果
		cout << "正在保存线性化投影结果....." << endl;
		int tempdim[3] = { pdim.x,pdim.y,pdim.z };
		Buffer_save(Ori_pro_buffer, data_size, tempdim, "linearized_Projection");

	}



}


void DeLinearize_Projections(wgpu::Buffer Volume, size_t Volume_Size,float MaxValue)
{
	
	//1.bindGroupLayout 绑定组布局
	auto DLbindGroupLayout = utils::MakeBindGroupLayout(
		device, {
			{0, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   //
			{1, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   //
			{2, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   //

		});
	//2. 绑定组
	wgpu::BindGroup DLComputeBindGroup = utils::MakeBindGroup(
		device, DLbindGroupLayout, {
			{0, Volume, 0, Volume_Size},
			{1, Max, 0, sizeof(MaxValue)},
			{2, Dim_buffer, 0,   sizeof(AllDim)},
		});

	//3.把布局设置到管道
	wgpu::PipelineLayout DLpipelineLayout = utils::MakeBasicPipelineLayout(device, &DLbindGroupLayout);
	////创建shader
	std::string shaderPath("work/shader/");
	std::string DLmsFile(shaderPath + "deLinearize.fs");
	wgpu::ShaderModule DLShaderModule = ShaderUtil::loadAndCompileShader(device, DLmsFile);

	//4.设置计算通道  目测每次只需要改变这个即可
	//setup compute pipeline
	wgpu::ComputePipelineDescriptor DLDescriptor;
	wgpu::ProgrammableStageDescriptor DLproDescriptor;
	DLproDescriptor.entryPoint = "DLmain";
	DLproDescriptor.module = DLShaderModule;
	DLDescriptor.layout = DLpipelineLayout;
	DLDescriptor.compute = DLproDescriptor;

	//5.创建计算通道
	wgpu::ComputePipeline DLcomputepipeline = device.CreateComputePipeline(&DLDescriptor);

	//6. 创建指令编码器
	wgpu::CommandEncoder DLencoder = device.CreateCommandEncoder();

	//7.获取指令队列
	auto DLqueue = device.GetQueue();
	wgpu::ComputePassDescriptor DLpassdescriptor;
	DLpassdescriptor.label = "DLcompute pass";
	wgpu::ComputePassEncoder DLcomputepass;
	int workgroupCountX = ceil(float(vdim.x) / 8);//如果是变成volume的时候 这里需要改变
	int workgroupCountY = ceil(float(vdim.y) / 8);
	int workgroupCountZ = ceil(float(vdim.z) / 8);

	//8. 设置管道指令
	DLcomputepass = DLencoder.BeginComputePass(&DLpassdescriptor);
	DLcomputepass.SetPipeline(DLcomputepipeline);
	DLcomputepass.SetBindGroup(0, DLComputeBindGroup);
	DLcomputepass.DispatchWorkgroups(workgroupCountX, workgroupCountY, workgroupCountZ);
	DLcomputepass.End();

	//9.获取指令集并执行
	wgpu::CommandBuffer commands = DLencoder.Finish();
	DLqueue.Submit(1, &commands);
	printf("DeLinearizing-Whole process of delinearizing Done!\n");


	int save_delinear = 0;
	//printf("是否保存去线性化volume结果？ 0-> no , 1-> yes ...");
	//scanf_s("%d", &save_delinear);
	if (save_delinear) {
		//保存线性结果
		cout << "正在保存线性化投影结果....." << endl;
		int tempdim[3] = { vdim.x,vdim.y,vdim.z };
		Buffer_save(Volume, Volume_Size, tempdim, "Delinearized_Projection");
	}
}

void Crop_data(wgpu::Buffer Rec_Vol, size_t Rec_Vol_Size, wgpu::Buffer CR_buffer,

	size_t CR_Size)
{
	//1.bindGroupLayout 绑定组布局
	auto TempbindGroupLayout = utils::MakeBindGroupLayout(
		device, {
			{0, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   //
			{1, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   //
			{2, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   //
			{3, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   //

		});
	//2. 绑定组
	wgpu::BindGroup TempComputeBindGroup = utils::MakeBindGroup(
		device, TempbindGroupLayout, {
			{0, Rec_Vol, 0, Rec_Vol_Size},
			{1, Dim_buffer, 0,   sizeof(AllDim)},//需要全部数据的大小
			{2, int_var_buffer, 0,  sizeof(intVAR)},
			{3, CR_buffer, 0,  CR_Size},
		});
	//3.把布局设置到管道
	wgpu::PipelineLayout TemppipelineLayout = utils::MakeBasicPipelineLayout(device, &TempbindGroupLayout);
	////创建shader
	std::string shaderPath("work/shader/");
	std::string TempmsFile(shaderPath + "Cropdata.fs");
	wgpu::ShaderModule TempShaderModule = ShaderUtil::loadAndCompileShader(device, TempmsFile);

	//4.设置计算通道  目测每次只需要改变这个即可
	//setup compute pipeline
	wgpu::ComputePipelineDescriptor TempDescriptor;
	wgpu::ProgrammableStageDescriptor TempproDescriptor;
	TempproDescriptor.entryPoint = "CRmain";
	TempproDescriptor.module = TempShaderModule;
	TempDescriptor.layout = TemppipelineLayout;
	TempDescriptor.compute = TempproDescriptor;

	//5.创建计算通道
	wgpu::ComputePipeline Tempcomputepipeline = device.CreateComputePipeline(&TempDescriptor);

	//6. 创建指令编码器
	wgpu::CommandEncoder Tempencoder = device.CreateCommandEncoder();

	//7.获取指令队列
	auto Tempqueue = device.GetQueue();
	wgpu::ComputePassDescriptor Temppassdescriptor;
	Temppassdescriptor.label = "Tempcompute pass";
	wgpu::ComputePassEncoder Tempcomputepass;
	int workgroupCountX = ceil(float(pdim.x) / 8);//pdim.x
	int workgroupCountY = ceil(float(tile_size) / 8);//tile_size
	int workgroupCountZ = ceil(float(vdim.z) / 8);//vdim.z

	//8. 设置管道指令
	Tempcomputepass = Tempencoder.BeginComputePass(&Temppassdescriptor);
	Tempcomputepass.SetPipeline(Tempcomputepipeline);
	Tempcomputepass.SetBindGroup(0, TempComputeBindGroup);
	Tempcomputepass.DispatchWorkgroups(workgroupCountX, workgroupCountY, workgroupCountZ);
	Tempcomputepass.End();

	//9.获取指令集并执行
	wgpu::CommandBuffer commands = Tempencoder.Finish();
	Tempqueue.Submit(1, &commands);
}


void SART() {

	cout << "********** SART Starting... *********" << endl;
	//int tempsize = vdim.x * vdim.y * vdim.z * sizeof(float);
	float* tempzero = (float*)malloc(recon_size);
	memset(tempzero, 0, recon_size);
	time(&start);
	//Forward projection and BackProjection
	auto bindGroupLayout = utils::MakeBindGroupLayout(
		device, {
			{0, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // for projection data
			{1, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // for calculating data
			{2, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // for DIM data
			{3, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // for ray dir data
			{4, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // for float var
			{5, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // for int var
			{6, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // for cor_img 
			{7, wgpu::ShaderStage::Compute, wgpu::BufferBindingType::Storage},   // for pip 
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
		});
	//pass the Layout for the pipeline
	wgpu::PipelineLayout pipelineLayout = utils::MakeBasicPipelineLayout(device, &bindGroupLayout); //后面应该可以直接使用pipelineLayout

	////创建shader
	std::string shaderPath("work/shader/");
	std::string FPshaderFile(shaderPath + "forwardprojection.fs");
	wgpu::ShaderModule FPShaderModule = ShaderUtil::loadAndCompileShader(device, FPshaderFile);
	std::string BPshaderFile(shaderPath + "backprojection.fs");
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

	for (int tile = 0; tile < number_of_tiles; tile++) {//分块处理
		current_tile = tile;
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

				
				//std::string file3 = "REC_TILE_" + std::to_string(current_tile) + "_PRO" + std::to_string(current_projection);
				//int tempdim3[3] = { vdim.x,vdim.y,vdim.z };
				//Buffer_save(Cal_rec_buffer, recon_size, tempdim3, file3);
			}
		}
		DeLinearize_Projections(Cal_rec_buffer,recon_size, MAX);//Delinearizing 
		Crop_data(Cal_rec_buffer, recon_size, per_crop_buffer, crop_size);

		std::string file2 = "CopyTile" + std::to_string(current_tile);
		Piece_Buffer_copy(per_crop_buffer, crop_size, file2);


		queue.WriteBuffer(Cal_rec_buffer, 0, (void*)tempzero, recon_size);//Clear the calculate

	}
	time(&time_end);
	float cost = difftime(time_end, start);
	printf("SART time consumption: %f s\n", cost);
}

void Piece_Buffer_copy(wgpu::Buffer gpubuffer, int buffer_size, string dataname){
	struct mydata {
		bool IfFinish;
	};
	mydata* UserDataTest = new mydata;
	UserDataTest->IfFinish = false;

	wgpu::CommandEncoder testencoder = device.CreateCommandEncoder();
	wgpu::Buffer host_buffer = utils::CreateBuffer(device, buffer_size, wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst);//save to host 
	testencoder.CopyBufferToBuffer(gpubuffer, 0, host_buffer, 0, buffer_size);
	wgpu::CommandBuffer commands = testencoder.Finish();
	auto testqueue = device.GetQueue();
	testqueue.Submit(1, &commands);
	host_buffer.MapAsync(wgpu::MapMode::Read, 0, buffer_size, [](WGPUBufferMapAsyncStatus status, void* userdata) {
		if (status == WGPUBufferMapAsyncStatus_Success) {
			mydata* TempUserData = static_cast<mydata*>(userdata);
			TempUserData->IfFinish = true;
			printf("Mapped!\n");
		}
		}, UserDataTest);
	while (!(UserDataTest->IfFinish)) {
		Sleep(100);
		printf("\rWaiting for Buffer Mappping[%s]....  ", dataname.c_str());
		device.Tick();
	}

	typedef cimg_library::CImg<float> CImgFloat;		//Convenient for saving data as .hdr
	CImgFloat temp_Img_data;
	temp_Img_data._data = (float*)host_buffer.GetConstMappedRange(0, buffer_size);
	
	if (tiled)
	{
		//The data is copied slice by slice to be able to do this
		for (int i = 0; i < vdim.z; i++)
		{
			//Copy piece slice (ignoring fist and last rows)
			//from device to host volume in the corresponding piece.
			memcpy(h_rec_vol._data + i * pdim.x * pdim.y + current_tile * pdim.x * tile_size, temp_Img_data._data + i * pdim.x * tile_size, pdim.x* tile_size* sizeof(float));

		}
	}
	else
	{
		//Copy whole volume
		memcpy(h_rec_vol._data, temp_Img_data._data, pdim.x * pdim.y * vdim.z * sizeof(float));
	}

	temp_Img_data._data = NULL;
	host_buffer.Unmap();
	host_buffer.Destroy();
	delete UserDataTest;
}