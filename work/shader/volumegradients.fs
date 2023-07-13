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
struct FloatVar{
    dis_vs:f32,
    sample_rate:f32,
    chillfactor:f32,
}
struct IntVar{
    sample_num:i32,
    current_projection:i32,
    tile_num:i32,
    tile_size:i32,
    number_extra_rows:i32,
    padding:i32,
}
@group(0) @binding(0) var<storage,write> Reconstructvolume : array<f32>;
@group(0) @binding(1) var<storage,write> VolumeGradients : array<f32>;
@group(0) @binding(2) var<uniform> AllDim : Dim;  
@group(0) @binding(3) var<uniform> floatconstVar : FloatVar;
@group(0) @binding(4) var<storage,write> intconstVar : IntVar;

fn if_ignorePadding(x:i32)->bool{
    let ignore_padding:bool=true;
    let padding=intconstVar.padding;
    let pdim=vec3<i32>(AllDim.pdim_x,AllDim.pdim_y,AllDim.pdim_z);
    	if (!ignore_padding || ((x >= padding) && (x < padding + pdim.x)))
        {
            return true;
        }
		
	return false;
}
@stage(compute) @workgroup_size(8,8,8)
fn main(@builtin(global_invocation_id) global_id : vec3<u32>){
		let pdim=vec3<i32>(AllDim.pdim_x,AllDim.pdim_y,AllDim.pdim_z);
		let vdim=vec3<i32>(AllDim.vdim_x,AllDim.vdim_y,AllDim.vdim_z);
		let pdelta=vec2<f32>(AllDim.pdelta_x,AllDim.pdelta_y);
		let vdelta=vec3<f32>(AllDim.vdelta_x,AllDim.vdelta_y,AllDim.vdelta_z);
		let gx=i32(global_id.x);let gy=i32(global_id.y);let gz=i32(global_id.z);
		let col:i32=gx; 
		let row:i32=gy;  
        let padding=intconstVar.padding;
        if (if_ignorePadding(col) && (row < vdim.y) && (col < vdim.x))
	    {
            let voxel_index:i32 = col + row*vdim.x + gz*vdim.y*vdim.x;
            //If we are in the x limit, assume the values outside the volume to be zero
            //The gradient is the negative of the voxel value. The same applies for y and z
            if (col < (vdim.x - 1))
                {VolumeGradients[voxel_index] = Reconstructvolume[voxel_index + 1] - Reconstructvolume[voxel_index];}
            else
                {VolumeGradients[voxel_index] = Reconstructvolume[voxel_index - col]  -Reconstructvolume[voxel_index];}
                //vol_grad[voxel_index] = -rec_vol[voxel_index];

            if (row < (vdim.y - 1))
                {VolumeGradients[voxel_index + vdim.z*vdim.y*vdim.x] = Reconstructvolume[voxel_index + vdim.x] - Reconstructvolume[voxel_index];}
            else
                {VolumeGradients[voxel_index + vdim.z*vdim.y*vdim.x] = Reconstructvolume[voxel_index - row*vdim.x] - Reconstructvolume[voxel_index];}
                //vol_grad[voxel_index + vdim.z*vdim.y*vdim.x] = -rec_vol[voxel_index];

            if (gz < (vdim.z - 1))
                {VolumeGradients[voxel_index + 2*vdim.z*vdim.y*vdim.x] = Reconstructvolume[voxel_index + vdim.y*vdim.x] - Reconstructvolume[voxel_index];}
            else
               { VolumeGradients[voxel_index + 2*vdim.z*vdim.y*vdim.x] = Reconstructvolume[voxel_index - gz*vdim.y*vdim.x] - Reconstructvolume[voxel_index];}
                //vol_grad[voxel_index + 2 * vdim.z*vdim.y*vdim.x] = -rec_vol[voxel_index];
        }


}