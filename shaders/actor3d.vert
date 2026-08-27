#version 450
#extension GL_ARB_shading_language_420pack: enable
layout(location=0) in vec3 in_position;
layout(location=1) in vec3 in_normal;
layout(location=2) in vec2 in_uv;
layout(location=3) in vec4 in_diffuse;
layout(location=4) in vec4 in_emissive;
layout(location=5) in vec4 in_surface;
layout(binding=0, std140) uniform ActorBlock {
    mat4 u_viewProj; vec4 u_cameraPos; vec4 u_lightDirAmbient;
    vec4 u_fogColorDensity; vec4 u_point0PosRange; vec4 u_point0ColorIntensity;
    vec4 u_point1PosRange; vec4 u_point1ColorIntensity; vec4 u_exposure;
    mat4 u_model;
};
layout(location=0) out vec3 v_worldPos;
layout(location=1) out vec3 v_normal;
layout(location=2) out vec2 v_uv;
layout(location=3) out vec4 v_diffuse;
layout(location=4) out vec4 v_emissive;
layout(location=5) out vec4 v_surface;
void main(){
    vec4 world=u_model*vec4(in_position,1.0);
    gl_Position=u_viewProj*world;
    v_worldPos=world.xyz;
    v_normal=normalize(mat3(u_model)*in_normal);
    v_uv=in_uv;
    v_diffuse=in_diffuse;
    v_emissive=in_emissive;
    v_surface=in_surface;
}
