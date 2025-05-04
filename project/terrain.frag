#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

///////////////////////////////////////////////////////////////////////////////
// Material
///////////////////////////////////////////////////////////////////////////////
uniform vec3 material_color = vec3(1, 1, 1);
uniform float material_metalness = 0;
uniform float material_fresnel = 0;
uniform float material_shininess = 0;
uniform vec3 material_emission = vec3(0);

// uniform int has_emission_texture = 0;
// layout(binding = 5) uniform sampler2D emissiveMap;

///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
layout(binding = 6) uniform sampler2D environmentMap;
layout(binding = 7) uniform sampler2D irradianceMap;
layout(binding = 8) uniform sampler2D reflectionMap;
uniform float environment_multiplier;

///////////////////////////////////////////////////////////////////////////////
// Light source
///////////////////////////////////////////////////////////////////////////////
uniform vec3 point_light_color = vec3(1.0, 1.0, 1.0);
uniform float point_light_intensity_multiplier = 50.0;

///////////////////////////////////////////////////////////////////////////////
// Constants
///////////////////////////////////////////////////////////////////////////////
#define PI 3.14159265359

///////////////////////////////////////////////////////////////////////////////
// Input varyings from vertex shader
///////////////////////////////////////////////////////////////////////////////
in vec2 texCoord;
in vec3 viewSpaceNormal;
in vec3 viewSpacePosition;
in float worldHeight;

///////////////////////////////////////////////////////////////////////////////
// Input uniform variables
///////////////////////////////////////////////////////////////////////////////
uniform mat4 viewInverse;
uniform vec3 viewSpaceLightPosition;
uniform vec2 texSize;

///////////////////////////////////////////////////////////////////////////////
// Output color
///////////////////////////////////////////////////////////////////////////////
layout(location = 0) out vec4 fragmentColor;

// Add a uniform for the colormap texture
layout(binding = 9) uniform sampler2D colormap;

// Add uniforms for color control
uniform float waterLevel;
uniform float sandLevel;
uniform float grassLevel;
uniform float rockLevel;

uniform vec3 waterColor = vec3(0.1, 0.2, 0.8);
uniform vec3 sandColor = vec3(0.85, 0.8, 0.5);
uniform vec3 grassColor = vec3(0.2, 0.6, 0.2);
uniform vec3 rockColor = vec3(0.6, 0.6, 0.6);
uniform vec3 snowColor = vec3(1.0, 1.0, 1.0);

// Fog parameters
uniform vec3 fogColor = vec3(0.5, 0.6, 0.7);
uniform float fogDensity = 0.001;  // Reduced density for more distant fog
uniform float fogStart = 100.0;    // Distance at which fog starts

// Function to calculate exponential fog
vec3 applyFog(vec3 color, float distance) {
    // Only apply fog beyond the start distance
    if (distance < fogStart) {
        return color;
    }
    float fogFactor = exp(-fogDensity * (distance - fogStart));
    return mix(fogColor, color, fogFactor);
}

vec3 calculateDirectIllumiunation(vec3 wo, vec3 n, vec3 base_color)
{
    vec3 direct_illum = base_color;
    float d = distance(viewSpacePosition, viewSpaceLightPosition);
    vec3 wi = normalize(viewSpaceLightPosition - viewSpacePosition);
    vec3 Li = point_light_intensity_multiplier * point_light_color * 1 / pow(d, 2);
    if (dot(wi, n) <= 0.0) return vec3(0, 0, 0);

    vec3 diffuse_term = base_color * (1.0 / PI) * dot(n, wi) * Li;

    vec3 wh = normalize(wi + wo);
    float F = material_fresnel + (1 - material_fresnel) * pow(1 - dot(wh, wi), 5);
    float D = (material_shininess + 2) / (2 * PI) * pow(max(0.001, dot(n, wh)), material_shininess);
    float G = min(1, min(2 * (dot(n, wh) * dot(n, wo) / max(0.001, dot(wo, wh))), 2 * (dot(n, wh) * dot(n, wi) / max(0.001, dot(wo, wh)))));
    float brdf = F * D * G / max(0.001, (4 * dot(n, wo) * dot(n, wi)));

    vec3 dielectic_term = brdf * dot(n, wi) * Li + (1.0 - F) * diffuse_term;
    vec3 metal_term = brdf * base_color * dot(n, wi) * Li;
    direct_illum = material_metalness * metal_term + (1.0 - material_metalness) * dielectic_term;

    return material_metalness * metal_term + (1.0 - material_metalness) * dielectic_term;
}

vec3 calculateIndirectIllumination(vec3 wo, vec3 n, vec3 base_color)
{
    vec3 indirect_illum = vec3(0.f);

    vec3 nws = vec3(viewInverse * vec4(n, 0.0));

    float theta = acos(max(-1.0f, min(1.0f, nws.y)));
    float phi = atan(nws.z, nws.x);
    if (phi < 0.0f)
    {
        phi = phi + 2.0f * PI;
    }
    vec2 lookup = vec2(phi / (2.0f * PI), 1.0f - theta / PI);
    vec3 irradiance = environment_multiplier * texture(irradianceMap, lookup).rgb;

    vec3 diffuse_term = base_color * (1.0f / PI) * irradiance;

    float roughness = sqrt(sqrt(2.0f / (material_shininess + 2.0f)));
    vec3 wi = normalize(reflect(-wo, n));
    vec3 wr = normalize(vec3(viewInverse * vec4(wi, 0.0f)));
    theta = acos(max(-1.0f, min(1.0f, wr.y)));
    phi = atan(wr.z, wr.x);
    if (phi < 0.0f)
    {
        phi = phi + 2.0f * PI;
    }
    lookup = vec2(phi / (2.0f * PI), 1.0f - theta / PI);
    vec3 Li = environment_multiplier * textureLod(reflectionMap, lookup, roughness * 7.0f).rgb;
    vec3 wh = normalize(wi + wo);
    float F = material_fresnel + (1.0f - material_fresnel) * pow(1 - dot(wh, wo), 5.0f);
    vec3 dielectric_term = F * Li + (1.0f - F) * diffuse_term;
    vec3 metal_term = F * base_color * Li;
    indirect_illum = material_metalness * metal_term + (1.0f - material_metalness) * dielectric_term;

    return indirect_illum;
}

// Add a function to smoothly interpolate between colors
vec3 getTerrainColor(float height) {
    // Add some smoothing to avoid sharp transitions
    float smoothing = 0.4;
    
    // Calculate blend factors using world height
    float waterBlend = smoothstep(waterLevel - smoothing, waterLevel + smoothing, height);
    float sandBlend = smoothstep(sandLevel - smoothing, sandLevel + smoothing, height);
    float grassBlend = smoothstep(grassLevel - smoothing, grassLevel + smoothing, height);
    float rockBlend = smoothstep(rockLevel - smoothing, rockLevel + smoothing, height);
    
    // Mix colors based on height
    vec3 color = waterColor;
    color = mix(color, sandColor, waterBlend);
    color = mix(color, grassColor, sandBlend);
    color = mix(color, rockColor, grassBlend);
    color = mix(color, snowColor, rockBlend);
    
    return color;
}

void main()
{
    vec3 wo = -normalize(viewSpacePosition);
    vec3 n = normalize(viewSpaceNormal);

    // Use worldHeight instead of viewSpacePosition.y
    vec3 terrainColor = getTerrainColor(worldHeight);

    // Direct illumination
    vec3 direct_illumination_term = calculateDirectIllumiunation(wo, n, terrainColor);

    // Indirect illumination
    vec3 indirect_illumination_term = calculateIndirectIllumination(wo, n, terrainColor);

    ///////////////////////////////////////////////////////////////////////////
    // Add emissive term. If emissive texture exists, sample this term.
    ///////////////////////////////////////////////////////////////////////////
    // vec3 emission_term = vec3(0.0);
    // if (has_emission_texture == 1)
    // {
    //     emission_term = texture(emissiveMap, texCoord).rgb;
    // }
    vec3 shading = direct_illumination_term + indirect_illumination_term; // + emission_term;

    // Calculate distance from camera for fog
    float distance = length(viewSpacePosition);
    
    // Apply fog to the final color
    vec3 finalColor = applyFog(shading, distance);

    fragmentColor = vec4(finalColor, 1.0);
    return;
}
