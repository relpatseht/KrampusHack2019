#ifndef STATIC_SCENE
#define STATIC_SCENE

#include "/hg_sdf.glsl"
#include "/noise.glsl"
#include "/scene_defines.glsl"

vec2 GroundHeight(in const vec3 pos)
{
	const float hNoise = noise(pos.xzx*20);
    const float height = cos(pos.x*0.25 + 0.3)*sin(pos.z*0.25) + 0.02*hNoise; // very regular patern + a little bit of noise

    return vec2(height, hNoise);
}

vec2 StaticScene_Tree(in vec3 pos, float typeFrac)
{
	const float woodMtl = 0.0;
	const float treeMtl = 4.0;
	float ground = GroundHeight(pos).x;

	pos.y -= ground;
	float stump = fCylinder(pos, 0.5, 1.0);
	float tree1 = fCone(pos - vec3(0, 0.5, 0), 2, 3);
	float tree2 = fCone(pos - vec3(0, 2.0, 0), 1.5, 2.5);
	float tree3 = fCone(pos - vec3(0, 3.5, 0), 1, 1.5);
	float tree = min(tree1, min(tree2, tree3));

	if(stump < tree)
		return vec2(stump, woodMtl);

	return vec2(tree, treeMtl + typeFrac);
}

vec2 StaticScene_Ground(in vec3 pos)
{
	vec2 h = GroundHeight(pos);
    return vec2(pos.y - h.x + 7.9, 1.0 + h.y*0.99);
}

vec2 StaticScene(in const vec3 pos)
{
	return StaticScene_Ground(pos);
}

vec3 Background_StarryGradient(in const float time, in const vec2 fragCoord, in const vec2 resolution)
{
	const float starSize = 30.0;
	const vec2 starPos = floor(1.0 / starSize * fragCoord);
	const float starValue = noise(vec3(starPos, 0.0));
	const float starProb = 0.98;
	const vec3 starColor = vec3(noise(vec3(fragCoord, 0.0)), noise(vec3(fragCoord - vec2(40, 40), 0.0)), noise(vec3(fragCoord + vec2(40, 40), 0.0)));
	vec3 color;

	// stars mostly stolen from https://www.shadertoy.com/view/lsfGWH
	if(starValue > starProb)
	{
		const vec2 starCenter = starSize * starPos + vec2(starSize*0.5);
		const float t = 0.9 + 0.2 * sin(time + (starValue - starProb) / (1.0 - starProb) * 45.0);
		float brightness = 1.0 - distance(fragCoord, starCenter) / (0.5 * starSize);

		brightness *= t / (abs(fragCoord.y - starCenter.y)) * t / (abs(fragCoord.x - starCenter.x));
		color = brightness*starColor;
	}
	else
	{
		const float starBrightness = noise(vec3(fragCoord, 0.0));

		if(starBrightness > 0.92)
			color = 2.0*starColor;
	}

	const float vertGrad = (1.0 - fragCoord.y / resolution.y) * 0.25;
	color += vertGrad * vec3(0.4, 0.8, 1.0);

	return color;
}

vec3 Background_Snow(in const float time, in const vec2 fragCoord, in const vec2 resolution)
{
	// stolen from https://www.shadertoy.com/view/Mdt3Df
    const float random = fract(sin(dot(fragCoord.xy,vec2(12.9898,78.233)))* 43758.5453);
	float snow = 0.0;

	for(int k=0;k<6;k++){
        for(int i=0;i<12;i++){
            float cellSize = 2.0 + (float(i)*3.0);
			float downSpeed = 0.3+(sin(time*0.4+float(k+i*20))+1.0)*0.00008;
            vec2 uv = (fragCoord.xy / resolution.x)+vec2(0.01*sin((time+float(k*6185))*0.6+float(i))*(5.0/float(i)),downSpeed*(time+float(k*1352))*(1.0/float(i)));
            vec2 uvStep = (ceil((uv)*cellSize-vec2(0.5,0.5))/cellSize);
            float x = fract(sin(dot(uvStep.xy,vec2(12.9898+float(k)*12.0,78.233+float(k)*315.156)))* 43758.5453+float(k)*12.0)-0.5;
            float y = fract(sin(dot(uvStep.xy,vec2(62.2364+float(k)*23.0,94.674+float(k)*95.0)))* 62159.8432+float(k)*12.0)-0.5;

            float randomMagnitude1 = sin(time*2.5)*0.7/cellSize;
            float randomMagnitude2 = cos(time*2.5)*0.7/cellSize;

            float d = 5.0*distance((uvStep.xy + vec2(x*sin(y),y)*randomMagnitude1 + vec2(y,x)*randomMagnitude2),uv.xy);

            float omiVal = fract(sin(dot(uvStep.xy,vec2(32.4691,94.615)))* 31572.1684);
            if(omiVal<0.08){
                float newd = (x+1.0)*0.4*clamp(1.9-d*(15.0+(x*6.3))*(cellSize/1.4),0.0,1.0);
                snow += newd;
            }
        }
    }

    return snow * vec3(0.95, 0.7, 1.0);
}

#endif //#ifndef STATIC_SCENE
