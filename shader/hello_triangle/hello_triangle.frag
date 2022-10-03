#version 450

layout (location = 0) in vec3 fragColor;

layout (location = 0) out vec4 outColor;

layout (std140, set = 0, binding = 0) uniform UBO {
    vec3 color1;
    vec3 color2;
    vec3 color3;
    float single;
    float[3] colors;
};

void main() {
    outColor = vec4(vec3(colors[0], colors[1], colors[2]), 1.0);
}