#version 450

layout(binding = 0) uniform uniform_buffer_obj {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 sun_dir_and_time;
} ubo;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

mat3 rotateZ(float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    return mat3(c, -s, 0,
                s, c,  0,
                0, 0,  1);
}

void main() 
{
    mat4 rotatedModel = ubo.model * mat4(rotateZ(ubo.sun_dir_and_time.w));
    gl_Position = ubo.proj * ubo.view * rotatedModel * vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
}