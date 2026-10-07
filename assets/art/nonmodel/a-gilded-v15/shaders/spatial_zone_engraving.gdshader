shader_type spatial;
render_mode cull_disabled, depth_draw_never;

uniform sampler2D engraving : source_color, filter_linear_mipmap, repeat_disable;
uniform float opacity : hint_range(0.0, 1.0) = 0.32;

void fragment() {
	vec4 ink = texture(engraving, UV);
	ALBEDO = ink.rgb;
	ROUGHNESS = 0.9;
	ALPHA = ink.a * opacity;
}
