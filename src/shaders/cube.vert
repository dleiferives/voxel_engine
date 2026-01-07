#version 460 core
// cube.vert
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aInstancePos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 FragPos;

void main() {
    vec3 worldPos = aPos + aInstancePos;
    FragPos = worldPos;
    gl_Position = projection * view * model * vec4(worldPos, 1.0);
}
