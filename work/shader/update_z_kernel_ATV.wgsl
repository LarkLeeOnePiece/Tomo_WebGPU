struct Dim {
    pdim_x:i32,
	pdim_y:i32,
	pdim_z:i32,
	
	vdim_x:i32,
	vdim_y:i32,
	vdim_z:i32,

	vdelta_x:f32,
	vdelta_y:f32,
	vdelta_z:f32,

	pdelta_x:f32,
	pdelta_y:f32
}

struct LADMM_parameters {
    mu:f32,
    ro:f32,
    lambda:f32,
    sigma:f32,
};

    @group(0) @binding(0) var<storage,write> x : array<f32>;
    @group(0) @binding(1) var<storage,write>  y : array<f32>; 
    @group(0) @binding(2) var<storage,write> z : array<f32>;
    @group(0) @binding(3) var<uniform> AllDim : Dim; 
    @group(0) @binding(4) var<uniform> LADMM_Vars: LADMM_parameters;

    @stage(compute) @workgroup_size(8,8,8)
//Proximal operator of g(u) is soft thresholding function:
//The max and product are element-wise operations: z = prox_g(kx + y) = sign(kx + y)*max(0, abs(kx + y) - sigma)    
fn main(@builtin(global_invocation_id) global_id : vec3<u32>) {
    let vdim=vec3<i32>(AllDim.vdim_x,AllDim.vdim_y,AllDim.vdim_z);
    let gx=i32(global_id.x);let gy=i32(global_id.y); let gz=i32(global_id.z);
    var col=gx; var row=gy;
    let mu:f32=LADMM_Vars.mu; let ro:f32=LADMM_Vars.ro;
    let lambda:f32=LADMM_Vars.lambda; let sigma:f32=LADMM_Vars.sigma;
    //Check that the voxel is inside the volume
	//(check if it's not in an extra row/column)
	if ((row < vdim.y) && (col < vdim.x))
	{
        var voxel_index:i32=col + row*vdim.x + gz*vdim.y*vdim.x;;
        //Get x, y, z gradients of current voxel, it means the gradients of previous,up-down,forward-backward 3 dimensions gradients
		var grad = vec3<f32>(0.0, 0.0, 0.0);

        //First the gradients of X
        //the previous gradients
        if (col < (vdim.x - i32(1))){
            grad.x = x[voxel_index + i32(1)] - x[voxel_index];
        }else{
            grad.x = x[voxel_index] - x[voxel_index-i32(1)]; //the first voxel- the last , the invert ->   -x[voxel_index];
        }
        z[voxel_index] = sign(grad.x + y[voxel_index])*max(f32(0), abs(grad.x + y[voxel_index]) - sigma);

        //the up dowm gradients
        if(row < (vdim.y - i32(1))){
            grad.y = x[voxel_index + vdim.x] - x[voxel_index];
        }else{
            grad.y = x[voxel_index] - x[voxel_index - vdim.x]; //grad.y = -x[voxel_index];
        }
        //Second element
		z[voxel_index + vdim.z*vdim.y*vdim.x] = sign(grad.y + y[voxel_index + vdim.z*vdim.y*vdim.x])*max(f32(0), abs(grad.y + y[voxel_index + vdim.z*vdim.y*vdim.x]) - sigma);

        //the forward and backward gradients
        if (gz < (vdim.z - i32(1))){
            grad.z = x[voxel_index + vdim.y*vdim.x] - x[voxel_index];
        }else{
            grad.z = x[voxel_index] - x[voxel_index-vdim.y*vdim.x];//grad.z = -x[voxel_index];
        }

        //Third element
		z[voxel_index + 2 * vdim.z*vdim.y*vdim.x] = sign(grad.z + y[voxel_index + 2 * vdim.z*vdim.y*vdim.x])*max(f32(0), abs(grad.z + y[voxel_index + 2 * vdim.z*vdim.y*vdim.x]) - sigma);
    }
}