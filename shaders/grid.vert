#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
  mat4 mvp;
} ubo;

layout(location = 0) out vec2 uv_out;

vec3 positions[4] = vec3[](
    vec3(10.0, 0.0, 10.0),
    vec3(10.0, 0.0, -10.0),
    vec3(-10.0, 0.0, -10.0),
    vec3(-10.0, 0.0, 10.0)
);

const uint indexes[6] = uint[6](0, 1, 2, 0, 2, 3);

vec2 uvs[4] = vec2[](
  vec2(1.0, 1.0),
  vec2(1.0, 0.0),
  vec2(0.0, 0.0),
  vec2(1.0, 0.0)
);

void main() {
    gl_Position = ubo.mvp * vec4(positions[indexes[gl_VertexIndex]], 1.0);
    uv_out = uvs[indexes[gl_VertexIndex]];
}
