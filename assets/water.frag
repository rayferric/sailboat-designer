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

const float PI = 3.14159265359;

vec2 sample_equirect(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= vec2(1.0 / (2.0 * PI), 1.0 / PI);
    uv += 0.5;
    return uv;
}

vec3 sample_sky(vec3 dir) {
    vec3 hdr = texture(tex_Sky, sample_equirect(normalize(dir))).rgb;
    vec3 mapped = hdr / (hdr + 1.0);
    return pow(mapped, vec3(1.0 / 2.2));
}

void main() {
    mat3 R = mat3(u_Frame.viewMat);
    vec3 T_vec = vec3(u_Frame.viewMat[3]);
    vec3 camPos = -transpose(R) * T_vec;

    vec3 viewDir = normalize(camPos - v_WorldPos);
    vec3 normal = normalize(v_Normal);

    vec3 lightDir = normalize(vec3(0.6, 0.8, -0.4));

    // Depth / angle dependent water colors
    vec3 deepWaterColor = vec3(0.01, 0.08, 0.16);
    vec3 shallowWaterColor = vec3(0.04, 0.35, 0.40);

    // Fresnel Schlick approximation
    float F0 = 0.02;
    float cosTheta = max(dot(normal, viewDir), 0.0);
    float fresnel = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);

    // Sky reflection sampled from HDR equirect
    vec3 reflectionDir = reflect(-viewDir, normal);
    vec3 skyReflection = sample_sky(reflectionDir);

    // Diffuse / Ambient base color
    float facingFactor = clamp(viewDir.y + 0.3, 0.0, 1.0);
    vec3 baseWaterColor = mix(shallowWaterColor, deepWaterColor, facingFactor);

    vec3 ambient = baseWaterColor * 0.4;
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = baseWaterColor * diff * 0.6;

    // Specular Highlight
    vec3 sunColor = vec3(1.0, 0.9, 0.8);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 300.0);
    vec3 specular = sunColor * spec * 5.0;

    // Mix water color with sky reflection by fresnel
    vec3 finalColor = mix(ambient + diffuse, skyReflection, fresnel) + specular;

    // Fade water into the exact sky color visible at this pixel's view
    // direction. Sampling the same direction the camera looks at the water
    // surface guarantees the fade target matches whatever the sky pass drew
    // behind it — no visible horizon seam.
    float dist = length(camPos - v_WorldPos);
    float fade = clamp((dist - 30.0) / 150.0, 0.0, 1.0);
    vec3 skyDir = normalize(v_WorldPos - camPos);
    vec3 skyAtPixel = sample_sky(skyDir);
    finalColor = mix(finalColor, skyAtPixel, fade);

    // View-angle dependent alpha for underwater visibility
    float viewAngleAlpha = pow(1.0 - cosTheta, 0.8);
    float alpha = mix(0.3, 0.95, viewAngleAlpha);
    // Become fully opaque where water has dissolved into sky
    alpha = mix(alpha, 1.0, fade);

    out_Color = vec4(finalColor, alpha);
}
