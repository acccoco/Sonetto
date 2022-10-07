#version 450


layout (location = 0) in vec4 inPos;
layout (location = 1) in vec4 inVel;

layout (location = 0) out float outGradientPos;
layout (location = 1) out vec3 outWorldPos;

layout (binding = 2) uniform UBO
{
    mat4 projection;
    mat4 modelview;
    vec2 screendim;
} ubo;

out gl_PerVertex
{
    vec4 gl_Position;
    float gl_PointSize;
};

void main()
{
    // 点的原始尺寸
    const float spriteSize = 0.005 * inPos.w;
    outWorldPos = inPos.xyz;

    // 把位置拉远一点
    vec3 new_pos = inPos.xyz * 3.0;

    vec4 view_pos = ubo.modelview * vec4(new_pos.x, new_pos.y, new_pos.z, 1.0);

    // 离得越远，点越小
    vec4 projectedCorner = ubo.projection * vec4(0.5 * spriteSize, 0.5 * spriteSize, view_pos.z, view_pos.w);
    gl_PointSize = clamp(ubo.screendim.x * projectedCorner.x / projectedCorner.w, 0, 128.0) * 10.f;

    gl_Position = ubo.projection * view_pos;

    outGradientPos = inVel.w;
}