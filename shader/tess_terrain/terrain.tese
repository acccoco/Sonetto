#version 450

// pipeline 设置的背面剔除，且将背面设置为顺时针，因此这里生成的三角形是逆时针（正面）
layout (quads, fractional_odd_spacing, ccw) in;

layout (std140, set = 0, binding = 0) uniform UBO
{
    mat4 model;
    mat4 view;
    mat4 projection;
} ubo;

layout (set = 0, binding = 1) uniform sampler2D u_height_map;

layout (location = 0) in vec2 in_uv[];
layout (location = 0) out float out_height;

void main()
{
    float abstract_u = gl_TessCoord.x;
    float abstract_v = gl_TessCoord.y;


    // 双线性插值，得到顶点的 pos uv
    //     ---------> x, u
    //     | 0    1
    //     |
    //     | 2    3
    //     v z, v
    vec2 uv = mix(
        mix(in_uv[0], in_uv[1], abstract_u),
        mix(in_uv[2], in_uv[3], abstract_u),
        abstract_v);

    out_height = texture(u_height_map, uv).y * 64.0 - 16.0;


    vec4 pos = mix(
        mix(gl_in[0].gl_Position, gl_in[1].gl_Position, abstract_u),
        mix(gl_in[2].gl_Position, gl_in[3].gl_Position, abstract_u),
        abstract_v
    );


    // 计算 patch 的法线方向，直接是指向 y 方向的，和实际的地形无关
    vec3 normal = normalize(cross((gl_in[2].gl_Position - gl_in[0].gl_Position).xyz,
                                  (gl_in[1].gl_Position - gl_in[0].gl_Position).xyz));

    pos.xyz += normal * out_height;


    gl_Position = ubo.projection * ubo.view * ubo.model * pos;
}