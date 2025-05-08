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

layout(binding = 9) uniform sampler2D colormap;

uniform float waterLevel;
uniform float sandLevel;
uniform float grassLevel;
uniform float rockLevel;
uniform float slopeThreshold = 0.7;

layout(binding = 10) uniform sampler2D waterTexture;
layout(binding = 11) uniform sampler2D sandTexture;
layout(binding = 12) uniform sampler2D grassTexture;
layout(binding = 13) uniform sampler2D rockTexture;
layout(binding = 14) uniform sampler2D snowTexture;

uniform float textureScale = 10.0;


float random(vec2 st) {
    return fract(sin(dot(st.xy, vec2(12.9898,78.233))) * 43758.5453123);
}

float noise(vec2 st) {
    vec2 i = floor(st);
    vec2 f = fract(st);
    
    float a = random(i);
    float b = random(i + vec2(1.0, 0.0));
    float c = random(i + vec2(0.0, 1.0));
    float d = random(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a)* u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
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

vec3 getTriplanarMapping(vec3 normal, vec3 position, sampler2D tex, float scale) {
    vec3 blendWeights = abs(normal);
    blendWeights = blendWeights / (blendWeights.x + blendWeights.y + blendWeights.z);
    
    // Add noise to position
    vec3 noiseOffset = vec3(
        noise(position.yz * 0.1),
        noise(position.xz * 0.1),
        noise(position.xy * 0.1)
    ) * 0.4; // Adjust noise strength here
    
    vec3 scaledPosition = (position + noiseOffset) / scale;
    
    vec3 colorX = mix(
        texture(tex, scaledPosition.yz).rgb,
        texture(tex, scaledPosition.yz * 2.0).rgb,
        0.7
    );
    vec3 colorY = mix(
        texture(tex, scaledPosition.xz).rgb,
        texture(tex, scaledPosition.xz * 2.0).rgb,
        0.7
    );
    vec3 colorZ = mix(
        texture(tex, scaledPosition.xy).rgb,
        texture(tex, scaledPosition.xy * 2.0).rgb,
        0.7
    );
    
    return colorX * blendWeights.x + colorY * blendWeights.y + colorZ * blendWeights.z;
}

vec3 getTerrainColor(float height) {
    
    float slope = 1.0 - abs(dot(normalize(viewSpaceNormal), vec3(0.0, 1.0, 0.0)));
    
    vec3 worldPos = vec3(viewInverse * vec4(viewSpacePosition, 1.0));
    
    vec3 waterTex = getTriplanarMapping(normalize(viewSpaceNormal), worldPos, waterTexture, textureScale);
    vec3 sandTex = getTriplanarMapping(normalize(viewSpaceNormal), worldPos, sandTexture, textureScale);
    vec3 grassTex = getTriplanarMapping(normalize(viewSpaceNormal), worldPos, grassTexture, textureScale);
    vec3 rockTex = getTriplanarMapping(normalize(viewSpaceNormal), worldPos, rockTexture, textureScale);
    vec3 snowTex = getTriplanarMapping(normalize(viewSpaceNormal), worldPos, snowTexture, textureScale);
    
    float smoothing = 0.4;
    float waterBlend = smoothstep(waterLevel - smoothing, waterLevel + smoothing, height);
    float sandBlend = smoothstep(sandLevel - smoothing, sandLevel + smoothing, height);
    float grassBlend = smoothstep(grassLevel - smoothing, grassLevel + smoothing, height);
    float rockBlend = smoothstep(rockLevel - smoothing, rockLevel + smoothing, height);
    
    float slopeBlend = smoothstep(slopeThreshold - 0.1, slopeThreshold + 0.1, slope);
    
    vec3 color = waterTex;
    
    color = mix(color, sandTex, waterBlend);
    color = mix(color, grassTex, sandBlend);
    color = mix(color, rockTex, grassBlend);
    color = mix(color, snowTex, rockBlend);
    
    color = mix(color, rockTex, slopeBlend);
    
    return color;
}

void main()
{
    vec3 wo = -normalize(viewSpacePosition);
    vec3 n = normalize(viewSpaceNormal);

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

    float distance = length(viewSpacePosition);

    fragmentColor = vec4(shading, 1.0);
    return;
}
