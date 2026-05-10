#version 460

layout(location = 0) in vec3 v_WorldPos;
layout(location = 1) in vec3 v_Normal;

layout(location = 0) out vec4 out_Color;

layout(std140, binding = 0) uniform Frame {
	mat4 viewMat;
	mat4 projMat;
    float time;
} u_Frame;

layout(binding = 0) uniform sampler2D tex_Sky;
layout(binding = 1) uniform sampler2D tex_Opaque;
layout(binding = 2) uniform sampler2D tex_Depth;

#include "raymarch.glsl"

const float PI = 3.14159265359;

vec2 sample_equirect(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= vec2(1.0 / (2.0 * PI), 1.0 / PI);
    uv += 0.5;
    return uv;
}

vec3 sample_sky(vec3 dir) {
    return texture(tex_Sky, sample_equirect(normalize(dir))).rgb;
}

vec3 calculateSSR(vec3 viewPos, vec3 viewNormal, vec3 viewIncident, out bool hit, out float fade) {
    vec3 reflectDir = normalize(reflect(viewIncident, viewNormal));
    RayMarchResult rm = rayMarch(
        tex_Depth,
        u_Frame.projMat,
        u_Frame.time,
        viewPos,
        reflectDir,
        200.0, // rayLength
        0.05,  // bias
        32,    // stepCount
        8      // refineStepCount
    );
    
    hit = rm.hasHit;
    if (rm.hasHit) {
        vec2 screenSize = vec2(textureSize(tex_Opaque, 0));
        vec2 uv     = gl_FragCoord.xy / screenSize;
        vec2 dist   = abs(rm.coord * 2.0 - 1.0);
        uv          = abs(uv * 2.0 - 1.0);
        dist.x      = smoothstep(1.0 - 1.5 * (1.0 - uv.x), 1.0, dist.x);
        float bound = max(dist.y, dist.x);
        fade  = smoothstep(0.0, 0.2, 1.0 - bound);

        return texture(tex_Opaque, rm.coord).rgb;
    }
    
    fade = 0.0;
    return vec3(0.0);
}

vec3 calculateRefraction(vec3 viewPos, vec3 viewNormal, vec3 viewIncident, out bool hit, out vec2 refractUV) {
    // Snell's law refraction - air to water (n1=1.0, n2=1.33)
    float eta = 1.0 / 1.33;
    vec3 refractDir = refract(viewIncident, viewNormal, eta);
    
    // If total internal reflection occurs, fall back to reflection direction
    if (length(refractDir) < 0.001) {
        refractDir = reflect(viewIncident, viewNormal);
    }
    
    RayMarchResult rm = rayMarch(
        tex_Depth,
        u_Frame.projMat,
        u_Frame.time,
        viewPos,
        refractDir,
        200.0, // rayLength
        0.05,  // bias
        32,    // stepCount
        8      // refineStepCount
    );
    
    hit = rm.hasHit;
    refractUV = rm.coord;
    
    if (rm.hasHit) {
        return texture(tex_Opaque, rm.coord).rgb;
    }
    
    // If ray doesn't hit anything, sample sky in refracted direction
    vec3 worldRefractDir = transpose(mat3(u_Frame.viewMat)) * refractDir;
    return sample_sky(worldRefractDir);
}

void main() {
    mat3 R = mat3(u_Frame.viewMat);
    vec3 T_vec = vec3(u_Frame.viewMat[3]);
    vec3 camPos = -transpose(R) * T_vec;

    vec3 viewDir = normalize(camPos - v_WorldPos);
    vec3 normal = normalize(v_Normal);

    vec3 lightDir = normalize(vec3(0.6, 0.8, -0.4));

    // Get water depth using depth buffer
    vec2 screenSize = vec2(textureSize(tex_Depth, 0));
    vec2 screenUV = gl_FragCoord.xy / screenSize;
    
    // Get depth of opaque geometry behind water
    float opaqueDepth = texture(tex_Depth, screenUV).r;
    float opaqueLinearDepth = linearizeDepth(opaqueDepth, u_Frame.projMat);
    
    // Get depth of water surface
    vec4 waterViewPos = u_Frame.viewMat * vec4(v_WorldPos, 1.0);
    float waterLinearDepth = -waterViewPos.z; // view space z is negative
    
    // Calculate water thickness - how much water light travels through
    float waterThickness = max(0.0, opaqueLinearDepth - waterLinearDepth);
    
    // Beer's law absorption coefficients for RGB channels
    // Higher values = more absorption of that color
    vec3 absorptionCoeff = vec3(0.45, 0.1, 0.05) * 20.0; // red absorbed most, blue least
    
    // Calculate transmission using Beer's law: T = exp(-absorption * distance)
    vec3 transmission = exp(-absorptionCoeff * waterThickness);
    
    // Water absorption color - what color the water adds as it gets deeper
    vec3 waterAbsorptionColor = vec3(0.02, 0.12, 0.25);

    // Fresnel Schlick approximation
    float F0 = 0.02;
    float cosTheta = max(dot(normal, viewDir), 0.0);
    float fresnel = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
    fresnel = min(fresnel * 2.0, 1.0);

    vec4 viewPosHom = u_Frame.viewMat * vec4(v_WorldPos, 1.0);
    vec3 viewPos = viewPosHom.xyz / viewPosHom.w;
    vec3 viewNormal = normalize(mat3(u_Frame.viewMat) * normal);
    vec3 viewIncident = normalize(viewPos);

    // Reflection (SSR with sky fallback)
    bool ssrHit;
    float ssrFade;
    vec3 reflectionColor = calculateSSR(viewPos, viewNormal, viewIncident, ssrHit, ssrFade);
    
    vec3 reflectionDir = reflect(-viewDir, normal);
    vec3 skyReflection = sample_sky(reflectionDir);

    if (ssrHit) {
        reflectionColor = mix(skyReflection, reflectionColor, ssrFade);
    } else {
        reflectionColor = skyReflection;
    }

    // // Fake refraction by perturbing screen UVs
    // float waveDistortion = sin(u_Frame.time * 4.0 + v_WorldPos.x + v_WorldPos.z) * 0.005;
    // vec2 distortion = viewNormal.xy * 0.05 + waveDistortion;
    // vec2 refractUV = clamp(screenUV + distortion, 0.0, 1.0);
    vec3 refractedColor = texture(tex_Opaque, screenUV).rgb;

    // Apply Beer's law absorption to the refracted background
    vec3 transmittedBackground = refractedColor * transmission;
    
    // Add water's intrinsic color based on depth
    vec3 waterColor = waterAbsorptionColor * (1.0 - transmission);
    vec3 underWaterColor = transmittedBackground + waterColor;

    // Specular Highlight
    vec3 sunColor = vec3(1.0, 0.9, 0.8);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 300.0);
    vec3 specular = sunColor * spec * 5.0;

    // Mix water color with reflection by fresnel
    vec3 finalColor = mix(underWaterColor, reflectionColor, fresnel) + specular;

    // Distance fade to sky
    float dist = length(camPos - v_WorldPos);
    float fade = clamp((dist - 30.0) / 150.0, 0.0, 1.0);
    vec3 skyDir = normalize(v_WorldPos - camPos);
    vec3 skyAtPixel = sample_sky(skyDir);
    finalColor = mix(finalColor, skyAtPixel, fade);

    // Output with proper alpha blending
    // Alpha represents how much the water surface itself contributes vs pure transparency
    float alpha = 1.0 - exp(-0.1 * waterThickness); // water becomes more opaque with depth
    alpha = max(alpha, fresnel * 0.8); // always some reflection visibility
    
    out_Color = vec4(finalColor, alpha);
}
