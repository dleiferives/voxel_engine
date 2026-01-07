#version 460 core

in vec3 FragPos;
in vec3 Normal;
in vec3 Color;

out vec4 FragColor;

uniform vec3 viewPos;

void main() {
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    vec3 norm = normalize(Normal);

    float ambient = 0.4;
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 result = Color * (ambient + diff * 0.6);

    result += Color * 0.1;

    float dist = length(FragPos - viewPos);
    float fogFactor = clamp(exp(-dist * 0.003), 0.0, 1.0);
    vec3 fogColor = vec3(0.4, 0.6, 0.9);
    result = mix(fogColor, result, fogFactor);

    FragColor = vec4(result, 1.0);
}
