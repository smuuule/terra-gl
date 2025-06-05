#version 330 core

in vec2 texCoords;
in vec4 clipSpace;
in vec3 worldPosition;

out vec4 fragColor;

uniform sampler2D reflectionTexture;
uniform sampler2D refractionTexture;
uniform sampler2D displacementTexture;
uniform sampler2D normalDisplacementTexture;
uniform float displacementStrength;
uniform float waterTiling;
uniform float timeOffset;
uniform vec3 cameraPosition;

///////////////////////////////////////////////////////////////////////////////
// Sunlight source
///////////////////////////////////////////////////////////////////////////////
uniform vec3 sunDirection;
uniform vec3 sunColor = vec3(1.0, 1.0, 0.8);
uniform float sunIntensity;

///////////////////////////////////////////////////////////////////////////////
// Moonlight source
///////////////////////////////////////////////////////////////////////////////
uniform vec3 moonColor = vec3(0.5f, 0.5f, 1.0f);

const float PI = 3.14159265359;

vec3 calculateSunIllumination(vec3 n, vec3 base_color, vec3 sun_direction, vec3 sun_color, float sun_intensity) {
    vec3 wi = -sun_direction;
    vec3 Li = sun_intensity * sun_color;
    if (dot(wi, n) <= 0.0) return vec3(0, 0, 0);

    vec3 sun_illum = base_color * (1.0 / PI) * dot(n, wi) * Li;
    return sun_illum;
}

void main() {
    vec2 ndc = (clipSpace.xy / clipSpace.w) / 2.0 + 0.5;
    
    vec2 reflectionTexCoord = vec2(ndc.x, 1.0 - ndc.y);
    vec2 refractionTexCoord = ndc;

    vec2 scaledTexCoords = texCoords * 0.01 * waterTiling;

    vec2 distTexCoords = texture(displacementTexture, vec2(scaledTexCoords.x + timeOffset, scaledTexCoords.y)).rg * 0.1;
	distTexCoords = scaledTexCoords + vec2(distTexCoords.x, distTexCoords.y + timeOffset);
	vec2 dist = (texture(displacementTexture, distTexCoords).rg * 2.0 - 1.0) * displacementStrength;

    reflectionTexCoord += dist;
    refractionTexCoord += dist;
    
    vec4 reflectionColor = texture(reflectionTexture, reflectionTexCoord);
    vec4 refractionColor = texture(refractionTexture, refractionTexCoord);
    
    vec3 viewDir = normalize(cameraPosition - worldPosition);
    float fresnelFactor = pow(1.0 - dot(viewDir, vec3(0.0, 1.0, 0.0)), 0.5);

    vec4 normalColor = texture(normalDisplacementTexture, distTexCoords);
    vec3 normal = normalize(vec3(normalColor.r * 2.0 - 1.0, normalColor.b, normalColor.g * 2.0 - 1.0));
    
    vec4 waterColor = mix(refractionColor, reflectionColor, fresnelFactor);
    
    // Sunlight illumination
    vec3 sunlight_illumination_term = calculateSunIllumination(normal, waterColor.rgb, sunDirection, sunColor, sunIntensity);

    // Moonlight illumination
    vec3 moonlight_illumination_term = calculateSunIllumination(normal, waterColor.rgb, -sunDirection, moonColor, 1.0);
    
    fragColor = vec4(waterColor.rgb + sunlight_illumination_term + moonlight_illumination_term, waterColor.a);
}
