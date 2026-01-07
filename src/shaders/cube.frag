#version 460 core
// cube.frag

in vec3 FragPos;
out vec4 FragColor;

void main() {
    // Simple coloring based on position
    vec3 color = normalize(abs(FragPos)) * 0.5 + 0.5;
    FragColor = vec4(color, 1.0);
}
