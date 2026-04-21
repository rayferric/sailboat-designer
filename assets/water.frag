#version 460

layout(location = 0) in vec3 v_WorldPos;
layout(location = 1) in vec3 v_Normal;

layout(location = 0) out vec4 out_Color;

layout(std140, binding = 0) uniform Frame {
	mat4 viewMat;
	mat4 projMat;
    float time;
} u_Frame;

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
    
    // Sky Reflection Box
    vec3 reflectionDir = reflect(-viewDir, normal);
    vec3 skyHorizonColors = vec3(0.5, 0.65, 0.85);
    vec3 skyZenithColors = vec3(0.1, 0.25, 0.6);
    float lookUpFactor = clamp(reflectionDir.y, 0.0, 1.0);
    vec3 skyReflection = mix(skyHorizonColors, skyZenithColors, lookUpFactor);
    
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
    
    // Soft distant fade
    float dist = length(camPos - v_WorldPos);
    float fade = clamp((dist - 15.0) / 60.0, 0.0, 1.0);
    vec3 fogColor = vec3(0.08, 0.08, 0.1);
    finalColor = mix(finalColor, fogColor, fade);
    
    // View-angle dependent alpha for underwater visibility
    // Looking straight down = more transparent, grazing angle = more opaque
    float viewAngleAlpha = pow(1.0 - cosTheta, 0.8); // Adjust power for falloff curve
    float alpha = mix(0.3, 0.95, viewAngleAlpha); // Min 30% at steep angles, 95% at grazing
    
    out_Color = vec4(finalColor, alpha);
}