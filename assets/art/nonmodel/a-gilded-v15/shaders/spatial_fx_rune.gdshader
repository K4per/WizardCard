shader_type spatial;
render_mode unshaded, cull_disabled, depth_draw_never;

uniform sampler2D effect_texture : source_color, filter_nearest, repeat_disable;
uniform float opacity : hint_range(0.0, 1.0) = 1.0;
uniform float phase = 0.0;
uniform bool looping = false;

void fragment() {
	vec4 ink = texture(effect_texture, UV);
	float pulse = looping ? 0.38 + 0.12 * sin(phase) : 1.0;
	ALBEDO = ink.rgb;
	ALPHA = ink.a * opacity * pulse;
}
