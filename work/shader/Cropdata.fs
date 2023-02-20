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
struct IntVar{
    sample_num:i32,
    current_projection:i32,
    tile_num:i32,
    tile_size:i32,
    number_extra_rows:i32,
    padding:i32,
}

@group(0) @binding(0) var<storage,write> OriVol : array<f32>;
@group(0) @binding(1) var<storage,write> AllDim : Dim;  
@group(0) @binding(2) var<storage,write> intconstVar : IntVar;
@group(0) @binding(3) var<storage,write> CropResult : array<f32>;


//what I need : original data,crop_buffer, dim£¬padding, tile_size 
    @stage(compute) @workgroup_size(8,8,8)
fn CRmain(@builtin(global_invocation_id) global_id : vec3<u32>) {
    let gx=i32(global_id.x);let gy=i32(global_id.y); let gz=i32(global_id.z);
    let pdim=vec3<i32>(AllDim.pdim_x,AllDim.pdim_y,AllDim.pdim_z);
	let vdim=vec3<i32>(AllDim.vdim_x,AllDim.vdim_y,AllDim.vdim_z);
    let col=gx; let row=gy;
    let extra_row=intconstVar.number_extra_rows/2; let TL=intconstVar.tile_size; let padding=intconstVar.padding;
    if(col<=pdim.x&&row<=TL){
        //make sure all points in the projection coor
        let VoxIndex=i32(padding)+col+(row+extra_row)*vdim.x+gz*vdim.y*vdim.x;
        let PointInCR=col+row*pdim.x+gz*pdim.x*TL;
        CropResult[PointInCR]=OriVol[VoxIndex];
    }
}