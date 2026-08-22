#version 460

#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 fragUVs;
layout(location = 1) in vec4 fragColor;
layout(location = 0) out vec4 color;

layout(push_constant) uniform PushConstants
{
    mat4 uProjection;
    uint textureIndex;
} push;

layout(set = 0, binding = 0) uniform sampler2D textures[];

void main()
{
   vec4 textureColor = texture(textures[push.textureIndex], fragUVs);
   color = textureColor * fragColor;
}