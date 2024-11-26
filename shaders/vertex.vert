#version 450

layout(constant_id = 0) const float WINDOW_COUNT_INVERSE = 0;
layout(location = 0) in vec3 pos;
layout(location = 1) in vec4 color;
layout(location = 0) out vec4 fragColor;

void main() {
	gl_Position = vec4(pos.x, pos.y, pos.z * WINDOW_COUNT_INVERSE, 1.0);
	fragColor = color;
}
