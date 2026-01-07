#version 460 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aInstancePosScale;  // xyz = position, w = scale
layout(location = 3) in vec4 aInstanceColorType; // xyz = color, w = modelId

uniform mat4 view;
uniform mat4 projection;

out vec3 FragPos;
out vec3 Normal;
out vec3 Color;

void main() {
    vec3 scaledPos = aPos * aInstancePosScale.w;
    vec3 worldPos = scaledPos + aInstancePosScale.xyz;

    FragPos = worldPos;
    Normal = aNormal;
    Color = aInstanceColorType.xyz;

    gl_Position = projection * view * vec4(worldPos, 1.0);
}
