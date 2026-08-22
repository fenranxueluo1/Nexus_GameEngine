#version 460

layout (location = 0) in vec2 aPosition;
layout (location = 1) in vec2 aTexCoords;
layout (location = 2) in vec4 aColor;

layout(push_constant) uniform PushConstants
{
    mat4 uProjection;
    uint textureIndex;
} push;

layout (location = 0) out vec2 fragUVs;
layout (location = 1) out vec4 fragColor;

void main()
{
   gl_Position = push.uProjection * vec4(aPosition, 0.0, 1.0);
   fragUVs = aTexCoords;
   fragColor = aColor;
}