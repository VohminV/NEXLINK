#version 450

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(set = 0, binding = 0) uniform Uniforms {
    mat4 mvp;
    vec4 tint;
} ubo;

layout(set = 0, binding = 1) uniform sampler2D s_texture;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outTexCoord;

void main() {
    gl_Position = ubo.mvp * vec4(inPos, 0.0, 1.0);
    outColor = inColor * ubo.tint;
    outTexCoord = inTexCoord;
}
