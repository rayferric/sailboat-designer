#version 460

in vec2 v_Uv;
out vec4 out_Color;

layout(binding = 0) uniform sampler2D tex_HDR;

vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 color = texture(tex_HDR, v_Uv).rgb;
    
    // ACES tonemap + gamma
    color = ACESFilm(color);
    color = pow(color, vec3(1.0 / 2.2));

    out_Color = vec4(color, 1.0);
}