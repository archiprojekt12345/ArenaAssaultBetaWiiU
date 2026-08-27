#version 450
#extension GL_ARB_shading_language_420pack: enable
layout(location=0) in vec3 v_worldPos;
layout(location=1) in vec3 v_normal;
layout(location=2) in vec2 v_uv;
layout(location=3) in vec4 v_diffuse;
layout(location=4) in vec4 v_emissive;
layout(location=5) in vec4 v_surface;
layout(binding=0, std140) uniform ActorBlock {
    mat4 u_viewProj; vec4 u_cameraPos; vec4 u_lightDirAmbient;
    vec4 u_fogColorDensity; vec4 u_point0PosRange; vec4 u_point0ColorIntensity;
    vec4 u_point1PosRange; vec4 u_point1ColorIntensity; vec4 u_exposure;
    mat4 u_model;
};
layout(binding=0) uniform sampler2D u_atlas;
layout(location=0) out vec4 out_color;
vec3 pointLight(vec4 p,vec4 c,vec3 N){
    vec3 d=p.xyz-v_worldPos; float dist=length(d); float a=max(1.0-dist/max(p.w,0.001),0.0); a*=a;
    return c.rgb*(c.a*max(dot(N,normalize(d)),0.0)*a);
}
void main(){
    vec3 N=normalize(v_normal); vec3 L=normalize(-u_lightDirAmbient.xyz);
    vec3 V=normalize(u_cameraPos.xyz-v_worldPos); vec3 H=normalize(L+V);
    vec4 texel=texture(u_atlas,v_uv); float tm=clamp(v_surface.z,0.0,1.0);
    vec3 albedo=v_diffuse.rgb*mix(vec3(1.0),texel.rgb,tm);
    float rough=clamp(v_surface.y,0.04,1.0);
    float spec=pow(max(dot(N,H),0.0),mix(96.0,5.0,rough))*v_surface.x;
    vec3 light=vec3(clamp(u_lightDirAmbient.w,0.0,1.0))+vec3(max(dot(N,L),0.0)*0.86);
    light+=pointLight(u_point0PosRange,u_point0ColorIntensity,N);
    light+=pointLight(u_point1PosRange,u_point1ColorIntensity,N);
    vec3 color=albedo*light+vec3(spec)+v_emissive.rgb*v_surface.w;
    color*=max(u_exposure.x,0.01);
    float fog=clamp(1.0-exp(-u_fogColorDensity.a*length(u_cameraPos.xyz-v_worldPos)),0.0,0.92);
    out_color=vec4(mix(color,u_fogColorDensity.rgb,fog),v_diffuse.a*texel.a);
}
