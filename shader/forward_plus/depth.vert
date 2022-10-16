// 普通的顶点阶段
#version 450
#extension GL_GOOGLE_include_directive : enable
#include "../shader/common.glsl"
#include "type.glsl"


layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;


layout(std140, set = 0, binding = 0) uniform _0
{
    Frame u_frame;
};
layout(std140, set = 0, binding = 1) uniform _1
{
    Scene u_scene;
};
layout(push_constant) uniform _push_constant_
{
    mat4 u_model;
};


void main()
{
    gl_Position = u_scene.proj_matrix * u_frame.view_matrix * u_model * vec4(in_pos, 1.0);
}
