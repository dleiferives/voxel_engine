#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;
layout(location = 3) in vec2 aUV;
layout(location = 4) in float aTextureLayer;
layout(location = 5) in float aAO;

uniform mat4 view;
uniform mat4 projection;

out vec3 FragPos;
out vec3 Normal;
out vec3 Color;
out vec2 TexCoord;
out float TextureLayer;
out float AO;

void main() {
    FragPos = aPos;
    Normal = aNormal;
    Color = aColor;
    TexCoord = aUV;
    TextureLayer = aTextureLayer;
    AO = aAO;
    gl_Position = projection * view * vec4(aPos, 1.0);
}
