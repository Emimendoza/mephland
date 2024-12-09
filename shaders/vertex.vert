#version 450
/*
layout(constant_id = 0) const float WINDOW_COUNT_INVERSE = 0;
layout(location = 0) in vec3 pos;
layout(location = 1) in vec4 color;
layout(location = 0) out vec4 fragColor;

void main() {
	gl_Position = vec4(pos.x, pos.y, pos.z * WINDOW_COUNT_INVERSE, 1.0);
	fragColor = color;
}
*/
layout(location = 0) out vec3 fragColor;

vec2 positions[3] = vec2[](
vec2(0.0, -0.5),
vec2(0.5, 0.5),
vec2(-0.5, 0.5)
);

vec3 colors[3] = vec3[](
vec3(1.0, 0.0, 0.0),
vec3(0.0, 1.0, 0.0),
vec3(0.0, 0.0, 1.0)
);

void main() {
	gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
	fragColor = colors[gl_VertexIndex];
}

