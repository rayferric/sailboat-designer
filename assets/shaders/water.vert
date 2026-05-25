#version 460

layout(location = 0) out vec3 v_WorldPos;
layout(location = 1) out vec3 v_Normal;

layout(std140, binding = 0) uniform Frame {
	mat4 viewMat;
	mat4 projMat;
	vec4 timeAndYaw;
	vec4 sunDir;
	vec4 sunColor;
	mat4 lightVP;
} u_Frame;

const int GRID_SIZE = 800;
const float QUAD_SIZE = 0.5;

const int NUM_WAVES = 9;
const float BASE_FREQ = 0.8;
const float BASE_AMP = 0.019;
const float BASE_SPEED = 0.26;
const float WAVE_STEEPNESS = 0.1;

void main() {
    int quad_id = gl_VertexID / 6;
    int vertex_in_quad = gl_VertexID % 6;
    
    int quad_x = quad_id % GRID_SIZE;
    int quad_z = quad_id / GRID_SIZE;
    
    vec2 offset = vec2(0.0);
    if (vertex_in_quad == 0) offset = vec2(0.0, 0.0);
    else if (vertex_in_quad == 1) offset = vec2(1.0, 0.0);
    else if (vertex_in_quad == 2) offset = vec2(0.0, 1.0);
    else if (vertex_in_quad == 3) offset = vec2(1.0, 0.0);
    else if (vertex_in_quad == 4) offset = vec2(1.0, 1.0);
    else if (vertex_in_quad == 5) offset = vec2(0.0, 1.0);
    
    vec2 grid_pos = vec2(quad_x, quad_z) + offset;
    
    mat3 R = mat3(u_Frame.viewMat);
    vec3 T_vec = vec3(u_Frame.viewMat[3]);
    vec3 cam_pos = -transpose(R) * T_vec;
    
    vec2 cam_xz = cam_pos.xz;
    vec2 snapped_cam_xz = floor(cam_xz / QUAD_SIZE) * QUAD_SIZE;
    
    float half_grid = (GRID_SIZE * QUAD_SIZE) / 2.0;
    vec2 world_xz = snapped_cam_xz + grid_pos * QUAD_SIZE - vec2(half_grid);
    
    vec3 pos = vec3(world_xz.x, 0.0, world_xz.y);
    vec3 normal = vec3(0.0, 1.0, 0.0);
    
    float freq = BASE_FREQ;
    float amp = BASE_AMP;
    float speed = BASE_SPEED;
    float angle = 0.0;
    float angle_step = -2.0 * 3.14159 / float(NUM_WAVES);
    float steepness_per_wave = WAVE_STEEPNESS / float(NUM_WAVES);
    
    for (int i = 0; i < NUM_WAVES; ++i) {
        vec2 dir = normalize(vec2(cos(angle), sin(angle)));
        
        float k = 2.0 * 3.14159 * freq;
        float c = sqrt(9.8 / k);
        float dt = k * dot(dir, world_xz) - c * (u_Frame.timeAndYaw.x * 10.0) * speed;
        float a = steepness_per_wave / k;
        
        float wa = k * a;
        float S = sin(dt);
        float C = cos(dt);
        
        pos.x += dir.x * (a * C);
        pos.z += dir.y * (a * C);
        pos.y += a * S;
        
        normal.x -= dir.x * wa * C;
        normal.y -= wa * S;
        normal.z -= dir.y * wa * C;
        
        // Update parameters for next wave
        if (i < NUM_WAVES / 2) {
            freq *= 0.66;
            amp *= 1.9;
            speed *= 0.33;
        } else {
            freq *= 1.33;
            amp *= 1.43;
            speed *= 0.99;
        }
        
        angle += angle_step;
    }
    
    normal = normalize(normal);

    v_WorldPos = pos;
    v_Normal = normal;
    
    gl_Position = u_Frame.projMat * u_Frame.viewMat * vec4(pos, 1.0);
}