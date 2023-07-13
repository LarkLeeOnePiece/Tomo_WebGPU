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
    @group(0) @binding(0) var<storage,write> Originalprojection : Matrix;
    @group(0) @binding(1) var<uniform> AllDim : Dim; 
    @group(0) @binding(2) var<storage,write> per_slice :Matrix;

    @stage(compute) @workgroup_size(1,1,8)
fn Maxmain(@builtin(global_invocation_id) global_id : vec3<u32>) {

    let pdim=vec3<i32>(AllDim.pdim_x,AllDim.pdim_y,AllDim.pdim_z);
    //let vdim=vec3<i32>(AllDim.vdim_x,AllDim.vdim_y,AllDim.vdim_z);
    let gx=i32(global_id.x);let gy=i32(global_id.y); let gz=i32(global_id.z);
    
    var max=-10000.0;
    var VoxIndex:i32;
    for(var i=0;i<pdim.x;i++){
        for(var j=0;j<pdim.y;j++){
            VoxIndex=i+j*pdim.x+gz*pdim.x*pdim.y;
                if (Originalprojection.data[VoxIndex]>max){
                        max=Originalprojection.data[VoxIndex];
                    }
        }
    }
    per_slice.data[gz]=max;
}

