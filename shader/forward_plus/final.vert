// 普通的顶点阶段
#version 450
#extension GL_GOOGLE_include_directive : enable
#include "../shader/common.glsl"
#include "type.glsl"


layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

layout(location = 0) out VertFrag o;

layout(set = 0, binding = 0, std140) uniform _0
{
    Frame u_frame;
};
layout(set = 0, binding = 1, std140) uniform _2
{
    Scene u_scene;
};
layout(push_constant) uniform _push_constant_
{
    mat4 u_model;
};


void main()
{
    vec4 pos_world = u_model * vec4(in_pos, 1.0);
    vec4 pos_view  = u_frame.view_matrix * pos_world;

    o.pos_world    = pos_world.xyz;
    o.pos_view     = pos_view.xyz;
    o.normal_world = normalize(inverse(transpose(mat3(u_model))) * in_normal);
    o.normal_view  = normalize(inverse(transpose(mat3(u_frame.view_matrix))) * o.normal_world);
    o.uv           = in_uv;

    gl_Position = u_scene.proj_matrix * pos_view;
}
