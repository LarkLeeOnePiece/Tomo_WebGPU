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

struct Matrix {
      data: array<f32>,
    }
    @group(0) @binding(0) var<storage,write> reconVolume : Matrix;
    @group(0) @binding(1) var<uniform> AllDim : Dim; 
    @group(0) @binding(2) var<storage,write> per_slice : array<f32>;

    @stage(compute) @workgroup_size(1,1,8)
fn main(@builtin(global_invocation_id) global_id : vec3<u32>) {

    let vdim=vec3<i32>(AllDim.vdim_x,AllDim.vdim_y,AllDim.vdim_z);
    //let vdim=vec3<i32>(AllDim.vdim_x,AllDim.vdim_y,AllDim.vdim_z);
    let gx=i32(global_id.x);let gy=i32(global_id.y); let gz=i32(global_id.z);
    var VoxIndex:i32;
    var slice_sum:f32=0.0;
    for(var i=0;i<vdim.x;i++){
        for(var j=0;j<vdim.y;j++){
            VoxIndex=i+j*vdim.x+gz*vdim.x*vdim.y;
            slice_sum+=reconVolume.data[VoxIndex]*reconVolume.data[VoxIndex];
        }
    }
    per_slice[gz]=slice_sum;
}

