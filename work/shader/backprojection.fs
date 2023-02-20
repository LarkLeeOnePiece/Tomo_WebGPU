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
}
@group(0) @binding(0) var<storage,write> Originalprojection : array<f32>;
@group(0) @binding(1) var<storage,write> Reconstructvolume : array<f32>;
@group(0) @binding(2) var<storage,write> AllDim : Dim;  
@group(0) @binding(3) var<storage,write> dir : vec3<f32>;
@group(0) @binding(4) var<storage,write> floatconstVar : FloatVar;
@group(0) @binding(5) var<storage,write> intconstVar : IntVar;
@group(0) @binding(6) var<storage,write> CorrectionImg : array<f32>;
@group(0) @binding(7) var<storage,write> pip : vec3<f32>;

//input point
//output rotated point
fn rotate_anticlockwise(pos:vec3<f32>)->vec3<f32>
{
	var temp_pos=vec3<f32>(dir.z*pos.x-dir.x*pos.z,pos.y,dir.z*pos.z+dir.x*pos.x);
	return temp_pos;
}

//get_index
//input: the projection/correction frame pos
//output: the projection/correction index
fn get_ProCOr_index(corpos:vec2<f32>)->i32{
    let pdim=vec3<i32>(AllDim.pdim_x,AllDim.pdim_y,AllDim.pdim_z);
    var index:i32;
    var pos:vec2<i32>;
    pos=vec2(i32(corpos.x),i32(corpos.y));
    index=pos.x+pos.y*pdim.x;
    return index;
}
//input: the point in the correction_img
//output: the value of the correction_img
//this function use the average as the correction value
fn interpol2D(pos:vec2<f32>)->f32
{
	let pdim=vec3<i32>(AllDim.pdim_x,AllDim.pdim_y,AllDim.pdim_z);
	var p11 = vec2<f32>(floor(pos.x), floor(pos.y));
		//Are we at the rightmost or topmost column?
	var xlimit:bool = false;
	var ylimit:bool = false;

	var p21:vec2<f32>;
	if (p11.x <f32(pdim.x - i32(1)))
	{
		p21 = vec2<f32>(p11.x + f32(1), p11.y);
	}else
	{
		xlimit = true;
		p21 = p11;
	}
	var p12:vec2<f32>;
	if (p11.y < f32(pdim.y - i32(1))){
		p12 = vec2(p11.x, p11.y + f32(1));
	}else
	{
		ylimit = true;
		p12 = p11;
	}

	var p22:vec2<f32>;
	if (!xlimit){
		p22 = vec2(p12.x + f32(1), p12.y);
	}else{
		p22 = p12;
	}
	
	//Get the volume data values of the cell points
	var q11:f32 = CorrectionImg[i32(p11.x) + i32(p11.y) * pdim.x];
	var q21:f32= CorrectionImg[i32(p21.x) + i32(p21.y) * pdim.x];
	var q12:f32= CorrectionImg[i32(p12.x) + i32(p12.y) * pdim.x];
	var q22:f32 = CorrectionImg[i32(p22.x) + i32(p22.y) * pdim.x];
	//return q11;
	//Interpolate
	var ip1:f32;
	var ip2:f32;
	if (!xlimit)
	{

		ip1 = ((p21.x - pos.x) / (p21.x - p11.x)) * q11 + ((pos.x - p11.x) / (p21.x - p11.x)) * q21;
		ip2 = ((p22.x - pos.x) / (p22.x - p12.x)) * q12 + ((pos.x - p12.x) / (p22.x - p12.x)) * q22;
	}else
	{
		ip1 = q11;
		ip2 = q22;
	}

	if (!ylimit)
	{
		return ((p12.y - pos.y) / (p12.y - p11.y)) * ip1 + ((pos.y - p11.y) / (p12.y - p11.y)) * ip2;
	}else
	{
		return ip1;
	}


}

@stage(compute) @workgroup_size(8,8,8)
fn BPmain(@builtin(global_invocation_id) global_id : vec3<u32>){
		let pdim=vec3<i32>(AllDim.pdim_x,AllDim.pdim_y,AllDim.pdim_z);
		let vdim=vec3<i32>(AllDim.vdim_x,AllDim.vdim_y,AllDim.vdim_z);
		let pdelta=vec2<f32>(AllDim.pdelta_x,AllDim.pdelta_y);
		let vdelta=vec3<f32>(AllDim.vdelta_x,AllDim.vdelta_y,AllDim.vdelta_z);
		let gx=i32(global_id.x);let gy=i32(global_id.y);let gz=i32(global_id.z);
		let current_tile=intconstVar.tile_num;
		let tile_size=intconstVar.tile_size;
		let number_extra_rows=intconstVar.number_extra_rows;
		let col:i32=gx; 
		let row:i32=current_tile*tile_size -(number_extra_rows/2) +gy;  
		let M:i32=intconstVar.sample_num;
		let sample_rate:f32=floatconstVar.sample_rate;
        let current_projection:i32=intconstVar.current_projection;
		let chill_factor:f32=floatconstVar.chillfactor;
		if ((col < vdim.x) && (row >= 0) && (row < pdim.y) && (gy < vdim.y))
		{
            var voxel_index:i32=col+gy*vdim.x+gz*vdim.x*vdim.y;
			var voxel_pos=vec3<f32>(f32(col)-vdelta.x,f32(row)-vdelta.y,f32(gz)-vdelta.z);
			var t=(pip.x-voxel_pos.x)*dir.x+(pip.z-voxel_pos.z)*dir.z;
			var pvox=vec3<f32>(voxel_pos.x + t * dir.x, voxel_pos.y, voxel_pos.z + t * dir.z);
			pvox=rotate_anticlockwise(pvox);
			if ((pvox.x <= pdelta.x) && (pvox.x >= -pdelta.x) &&
			(pvox.y <= pdelta.y) && (pvox.y >= -pdelta.y))
			{
				//Reconstructvolume[voxel_index] += chill_factor*interpol2D(vec2<f32>(pvox.x + pdelta.x, pvox.y + pdelta.y));
				//Reconstructvolume[voxel_index]= vec2<f32>(pvox.x + pdelta.x, pvox.y + pdelta.y).x;
				//Reconstructvolume[voxel_index]=interpol2D(vec2<f32>(pvox.x + pdelta.x, pvox.y + pdelta.y));//it can get correction_img data 
				//Reconstructvolume[voxel_index]=chill_factor;//chill_factor is ok 
				Reconstructvolume[voxel_index]+=chill_factor*interpol2D(vec2<f32>(pvox.x + pdelta.x, pvox.y + pdelta.y));
				//Positivity constraint
				if ((Reconstructvolume[voxel_index] < 0.0)){
                    Reconstructvolume[voxel_index] = 0.0;
                }
					
			}

		}


}