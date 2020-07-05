
#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 outColor;


void main(void) {
    float divisions = 10.0;
    float thickness = 0.005;
    float delta = 0.05 / 2.0;

    float x = fract(uv.x / (1.0 / divisions));
    float xdelta = fwidth(x) * 2.5;
    x = smoothstep(x - xdelta, x + xdelta, thickness);

    float y = fract(uv.y / (1.0 / divisions));
    float ydelta = fwidth(y) * 2.5;
    y = smoothstep(y - ydelta, y + ydelta, thickness);

    float c = clamp(x + y, 0.1, 1.0);

    outColor = vec4(c, c, c, 1.0);
}
