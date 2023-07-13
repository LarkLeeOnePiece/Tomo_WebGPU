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
struct LADMM_parameters {
    mu:f32,
    ro:f32,
    lambda:f32,
    sigma:f32,
};

@group(0) @binding(0) var<storage,write> Originalprojection : array<f32>;
@group(0) @binding(1) var<storage,write> Reconstructvolume : array<f32>;
@group(0) @binding(2) var<uniform> AllDim : Dim;  
@group(0) @binding(3) var<storage,write> dir : vec3<f32>;
@group(0) @binding(4) var<uniform> floatconstVar : FloatVar;
@group(0) @binding(5) var<storage,write> intconstVar : IntVar;
@group(0) @binding(6) var<storage,write> CorrectionImg : array<f32>;
@group(0) @binding(7) var<storage,write> pip : vec3<f32>;
@group(0) @binding(8) var<uniform> LADMM_Vars: LADMM_parameters;
@group(0) @binding(9) var<storage,write> yslack: array<f32>;

fn rotate_clockwise(pos:vec3<f32>)->vec3<f32>{
	//dir.z==cos   dir.x==sin
	var rotated_pos=vec3<f32>(dir.z*pos.x+dir.x*pos.z,pos.y,dir.z*pos.z-dir.x*pos.x);
	return rotated_pos;
}
fn get_ray_origin(pos:vec3<f32>)->vec3<f32>
{
	let vdelta=vec3<f32>(AllDim.vdelta_x,AllDim.vdelta_y,AllDim.vdelta_z);
	//Rotate according to current projection angle
	//Initial position of volume rendering, from source plane
	var roPOS:vec3<f32>=rotate_clockwise(pos);

	//'Decenter' to volume coordinates
	roPOS = roPOS + vec3<f32>(vdelta.x, 0, vdelta.z);//这里理解应该是把全部的点都旋转了
	return roPOS;
}
//get_index
//input: the volume pos in volume frame
//output: the volume index
fn get_index(Worpos:vec3<f32>)->i32{
    var index:i32;
    var pos:vec3<i32>;
    let vdim=vec3<i32>(AllDim.vdim_x,AllDim.vdim_y,AllDim.vdim_z);
    pos=vec3(i32(Worpos.x),i32(Worpos.y),i32(Worpos.z));
    index=pos.x+pos.y*vdim.x+pos.z*vdim.x*vdim.y;
    return index;
}

//interpolation
//input: current ray sample position in the volume frame,the reconstruct volume data
//output: the interpolation value 
fn interpolation(pos:vec3<f32>)->f32
{
	let vdim=vec3<i32>(AllDim.vdim_x,AllDim.vdim_y,AllDim.vdim_z);
	var p11=vec3<f32>(floor(pos.x),floor(pos.y),floor(pos.z));
		//Are we at the rightmost or topmost column?
	var xlimit:bool= false;
	var ylimit:bool= false;
	var zlimit:bool = !(p11.z < f32(vdim.z - i32(1)));
	var p21:vec2<f32>;
	if(p11.x<f32(vdim.x-i32(1))){
		p21=vec2(p11.x+f32(1),p11.y);
	}else{
		xlimit = true;
		p21=vec2(p11.x,p11.y);
	}

	var p12:vec2<f32>;
	if (p12.y < f32(vdim.y - i32(1)))
	{
		p12 = vec2(p11.x, p11.y +f32(1) );
	}	
	else
	{
		ylimit = true;
		p12 = vec2(p11.x, p11.y);
	}

	var p22:vec2<f32>;
	if (!xlimit){
		p22 = vec2(p12.x + f32(1), p12.y);
	}else{
		p22 = p12;
	}

		//Get the volume data values of the cell points
	var q11:f32 = Reconstructvolume[i32(p11.x) + i32(p11.y)*vdim.x + i32(p11.z)*vdim.y*vdim.x];
	var q21:f32= Reconstructvolume[i32(p21.x) + i32(p21.y)*vdim.x + i32(p11.z)*vdim.y*vdim.x];
	var q12:f32 = Reconstructvolume[i32(p12.x) + i32(p12.y)*vdim.x +i32( p11.z)*vdim.y*vdim.x];
	var q22:f32 = Reconstructvolume[i32(p22.x) + i32(p22.y)*vdim.x + i32(p11.z)*vdim.y*vdim.x];	
		
	//Same, next z value
	var s11:f32;
	var s21:f32;
	var s12:f32;
	var s22:f32;
	if (!zlimit)
	{
		s11 = Reconstructvolume[i32(p11.x) + i32(p11.y)*vdim.x + i32(p11.z + 1)*vdim.y*vdim.x];
		s21 = Reconstructvolume[i32(p21.x) + i32(p21.y)*vdim.x + i32(p11.z + 1)*vdim.y*vdim.x];
		s12 = Reconstructvolume[i32(p12.x) + i32(p12.y)*vdim.x + i32(p11.z + 1)*vdim.y*vdim.x];
		s22 = Reconstructvolume[i32(p22.x) + i32(p22.y)*vdim.x + i32(p11.z + 1)*vdim.y*vdim.x];
	}
	else
	{
		s11 = q11;
		s21 = q21;
		s12 = q12;
		s22 = q22;
	}

	//Interpolate
	var ip1:f32;
	var ip2:f32;
	var is1:f32;
	var is2:f32;
	if (!xlimit)
	{
		ip1 = ((p21.x - pos.x) / (p21.x - p11.x)) * q11 + ((pos.x - p11.x) / (p21.x - p11.x)) * q21;
		ip2 = ((p22.x - pos.x) / (p22.x - p12.x)) * q12 + ((pos.x - p12.x) / (p22.x - p12.x)) * q22;

		is1 = ((p21.x - pos.x) / (p21.x - p11.x)) * s11 + ((pos.x - p11.x) / (p21.x - p11.x)) * s21;
		is2 = ((p22.x - pos.x) / (p22.x - p12.x)) * s12 + ((pos.x - p12.x) / (p22.x - p12.x)) * s22;
	}
	else
	{
		ip1 = q11;
		ip2 = q22;

		is1 = s11;
		is2 = s22;
	}

	var interpol1:f32;
	var interpol2:f32;
	if (!ylimit)
	{
		interpol1 = ((p12.y - pos.y) / (p12.y - p11.y)) * ip1 + ((pos.y - p11.y) / (p12.y - p11.y)) * ip2;
		interpol2 = ((p12.y - pos.y) / (p12.y - p11.y)) * is1 + ((pos.y - p11.y) / (p12.y - p11.y)) * is2;
	}
	else
	{
		interpol1 = ip1;
		interpol2 = is1;
	}

	if (!zlimit)
	{
		return (((p11.z + f32(1)) - pos.z) / ((p11.z + f32(1)) - p11.z)) * interpol1 + ((pos.z - p11.z) / ((p11.z + f32(1)) - p11.z)) * interpol2;
	}
	else
	{
		return interpol1;
	}
}
//get_length
//input: a vector
//output: the length of the vector
fn get_length(pos:vec3<f32>)->f32
{
    var length:f32;
    length=sqrt(pos.x*pos.x+pos.y*pos.y+pos.z*pos.z);
    return length;

}

@stage(compute) @workgroup_size(8,8)
fn FPmain(@builtin(global_invocation_id) global_id : vec3<u32>) {
	    let pdim=vec3<i32>(AllDim.pdim_x,AllDim.pdim_y,AllDim.pdim_z);
		let vdim=vec3<i32>(AllDim.vdim_x,AllDim.vdim_y,AllDim.vdim_z);
		let pdelta=vec2<f32>(AllDim.pdelta_x,AllDim.pdelta_y);
		let vdelta=vec3<f32>(AllDim.vdelta_x,AllDim.vdelta_y,AllDim.vdelta_z);
		let gx=i32(global_id.x);let gy=i32(global_id.y);
		let current_tile=intconstVar.tile_num;
		let tile_size=intconstVar.tile_size;
		let number_extra_rows=intconstVar.number_extra_rows;
		let col:i32=gx; 
		let row:i32=current_tile*tile_size - (number_extra_rows/2) +gy;  
		let M:i32=intconstVar.sample_num;
		let sample_rate:f32=floatconstVar.sample_rate;
        let current_projection=intconstVar.current_projection;
        let mu:f32=LADMM_Vars.mu; let ro:f32=LADMM_Vars.ro;let lambda:f32=LADMM_Vars.lambda;

		if ((row >= 0) && (row < pdim.y) && (gy < vdim.y) && (col < pdim.x))
		{
			//current_tile is 0 for non pw
			var pixel_index:i32 = col + row*pdim.x;
			//Ray origin in world coordinates (from i, j pixel in source plane)
			var Oringin_pos =vec3<f32>(f32(col) - pdelta.x,f32(gy), -floatconstVar.dis_vs - vdelta.z);
			var pos:vec3<f32> = get_ray_origin(Oringin_pos);
			var  entry_pos = vec3<f32>(0, 0, 0);
			var  exit_pos = vec3<f32>(0, 0, 0);
			var  ray_sum:f32;
			var inteVAR:f32;

			for (var m:i32 = 0; m <= M; m++)
			{
				//If the current position is 'inside the volume'
				if ((pos.x <= f32(vdim.x - 1)) && (pos.x >= 0.0) &&
					//Y never changes and is always inside
					(pos.z <= f32(vdim.z - 1)) && (pos.z >= 0.0))
				{
					//Store first sampled point as entry point
					//TODO: Compute entry and exit points outside, once and exactly!
					if ((entry_pos.x == 0) && (entry_pos.y == 0) && (entry_pos.z == 0))
					{
						entry_pos = pos;
						entry_pos.x = entry_pos.x + 0.000000001; //To prevent funny things if the entry point was 0, 0, 0
					}

					//Update ray sum
					inteVAR=interpolation(pos);
					ray_sum += sample_rate*inteVAR;

					//Update last sampled position
					exit_pos = pos;
				}
				pos = pos + sample_rate*dir;
			}

            var ray_length:f32=f32(1.0)+lambda*get_length(exit_pos-entry_pos);

            //the index in the correction projection is equal to the projection
            //Correction = (lambda*(original - FP) - y)/ray_length, will be used updating X
            let indexInPro=pixel_index+current_projection*pdim.x*pdim.y;// the correspond point in projection data
            if(ray_length>f32(1)&&Originalprojection[indexInPro]>f32(0)){     
                CorrectionImg[pixel_index]=(lambda*(Originalprojection[indexInPro]-ray_sum)-yslack[pixel_index])/ray_length;
				//CorrectionImg[pixel_index]= inteVAR   ;
				//CorrectionImg[pixel_index]=Originalprojection[indexInPro];
				//CorrectionImg[pixel_index]=ray_length;
            }  
            else{//the projection is 0 means nothing to correction
                CorrectionImg[pixel_index]=f32(0);
            }
		}

}