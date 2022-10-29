#version 450
#extension GL_GOOGLE_include_directive : enable
#include "../shader/light.glsl"


layout(location = 1) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

TEXTURE_BINDING(1, 0)


void main()
{
    Material material = u_mat;
    parse_material(material, fragTexCoord, diffuse_texture, ambient_texture, specular_texture);
    outColor = vec4(material.diffuse_color.rgb, 1.0);
}