#ifndef NOISE
#define NOISE

const vec4 NC0=vec4(0.0,157.0,113.0,270.0);
const vec4 NC1=vec4(1.0,158.0,114.0,271.0);

vec4 hash4( vec4 n ) { return fract(sin(n)*1399763.5453123); }

float noise2( vec2 x )
{
    vec2 p = floor(x);
    vec2 f = fract(x);
    f = f*f*(3.0-2.0*f);
    float n = p.x + p.y*157.0;
    vec4 h = hash4(vec4(n)+vec4(NC0.xy,NC1.xy));
    vec2 s1 = mix(h.xy,h.zw,f.xx);
    return mix(s1.x,s1.y,f.y);
}

float noise3( vec3 x )
{
    vec3 p = floor(x);
    vec3 f = fract(x);
    f = f*f*(3.0-2.0*f);
    float n = p.x + dot(p.yz,vec2(157.0,113.0));
    vec4 s1 = mix(hash4(vec4(n)+NC0),hash4(vec4(n)+NC1),f.xxxx);
    return mix(mix(s1.x,s1.y,f.y),mix(s1.z,s1.w,f.y),f.z);
}

#endif //#ifndef NOISE
