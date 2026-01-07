#version 460 core

in vec3 FragPos;
in vec3 Normal;
in vec3 Color;
in vec2 TexCoord;
in float TextureLayer;
in float AO;  // Now contains baked light level (0.0 - 1.0)

out vec4 FragColor;

uniform vec3 viewPos;
uniform sampler2DArray textureArray;

void main() {
    vec4 texColor;
    if (TextureLayer < 0.0) {
        // Micro blocks use vertex color only
        texColor = vec4(Color, 1.0);
    } else {
        // Sample from texture array
        texColor = texture(textureArray, vec3(TexCoord, TextureLayer));
        // Multiply by vertex color for tinting/shading
        texColor.rgb *= Color;
    }

    // Discard fully transparent pixels
    if (texColor.a < 0.1) {
        discard;
    }

    // Apply baked light level (AO now contains propagated light)
    // The light level already includes directional shading from meshing
    vec3 result = texColor.rgb * AO;

    // Fog
    float dist = length(FragPos - viewPos);
    float fogFactor = clamp(exp(-dist * 0.003), 0.0, 1.0);
    vec3 fogColor = vec3(0.4, 0.6, 0.9);
    result = mix(fogColor, result, fogFactor);

    FragColor = vec4(result, texColor.a);
}
