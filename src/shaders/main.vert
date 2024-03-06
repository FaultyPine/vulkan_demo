#version 450

layout(binding = 0) uniform uniform_buffer_obj {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 sun_dir_and_time;
} ubo;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inColor;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragUV;

mat3 rotateZ(float angle)
{
    float c = cos(angle);
    float s = sin(angle);
    return mat3(c, -s, 0,
                s, c,  0,
                0, 0,  1);
}

mat4 Billboard(mat4 modelViewMat) {
    // To create a "billboard" effect,
    // set upper 3x3 submatrix of model-view matrix to identity
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            modelViewMat[i][j] = 0;
        }
    }
    modelViewMat[0][0] = 1;
    modelViewMat[1][1] = 1;
    modelViewMat[2][2] = 1;
    return modelViewMat;
}

void main() 
{
    mat4 rotatedModel = ubo.model;
    mat4 modelview = ubo.view * rotatedModel;
    //modelview = Billboard(modelview);
    mat4 mvp = ubo.proj * modelview;
    gl_Position = vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
    fragUV = inUV;
}