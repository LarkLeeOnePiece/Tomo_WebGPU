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
    @group(0) @binding(0) var<storage,write> RecVolume : Matrix;
    @group(0) @binding(1) var<storage,write> MaxValue :f32;
    @group(0) @binding(2) var<storage,write> AllDim : Dim; 

    @stage(compute) @workgroup_size(8,8,8)
fn DLmain(@builtin(global_invocation_id) global_id : vec3<u32>) {
    // Guard against out-of-bounds work group sizes
    //let pdim=vec3<i32>(AllDim.pdim_x,AllDim.pdim_y,AllDim.pdim_z);
    let vdim=vec3<i32>(AllDim.vdim_x,AllDim.vdim_y,AllDim.vdim_z);
 if (global_id.x >u32(vdim.x)  || global_id.y >u32(vdim.y)||(global_id.z>u32(vdim.z))) {
    return;
    }
    
    let gx=i32(global_id.x);let gy=i32(global_id.y); let gz=i32(global_id.z);
    let VoxIndex=gx+gy*vdim.x+gz*vdim.x*vdim.y;
    RecVolume.data[VoxIndex]=MaxValue*exp(-1*RecVolume.data[VoxIndex]);

}

