
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;

void main(void) {
  float divisions = 50.0;
  float thickness = 0.04;
  float delta = 0.05 / 2.0;

  float x = fract(uv.x * divisions);
  x = min(x, 1.0 - x);

  float xdelta = fwidth(x);
  x = smoothstep(x - xdelta, x + xdelta, thickness);

  float y = fract(uv.y * divisions);
  y = min(y, 1.0 - y);

  float ydelta = fwidth(y);
  y = smoothstep(y - ydelta, y + ydelta, thickness);

  float c =clamp(x + y, 0.0, 1.0);

  if (c < 0.3) discard;
  outColor = vec4(0.8, 0.8, 0.8, 1.0);
}
