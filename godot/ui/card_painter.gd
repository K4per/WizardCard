class_name CardPainter
extends RefCounted

const FONT: Font = preload("res://art/fonts/NotoSansCJKsc-Regular.otf")
const PAPER: Color = Color("f6eed8")
const INK: Color = Color("08131f")

static func shortened(text: String, width: float, font_size: int) -> String:
	if FONT.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x <= width:
		return text
	var result: String = text
	while not result.is_empty() and FONT.get_string_size(result + "…", HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x > width:
		result = result.left(result.length() - 1)
	return result + "…"

static func cover(surface: CanvasItem, texture: Texture2D, rect: Rect2) -> void:
	if texture == null:
		return
	var texture_size: Vector2 = texture.get_size()
	var scale_factor: float = maxf(rect.size.x / texture_size.x, rect.size.y / texture_size.y)
	var crop_size: Vector2 = rect.size / scale_factor
	var crop: Rect2 = Rect2((texture_size - crop_size) * 0.5, crop_size)
	surface.draw_texture_rect_region(texture, rect, crop)

static func paint(surface: CanvasItem, card: CardPresentation, rect: Rect2, view: String = "field", highlighted: bool = false) -> void:
	if card == null:
		return
	if card.hidden:
		surface.draw_texture_rect(ArtBank.texture("c2d.card.back.compact"), rect, false)
		if highlighted:
			surface.draw_rect(rect.grow(3), Color("64cfe6"), false, 2)
		return
	var id: String = "c2d.card." + view + "." + card.type_key
	var meta: Dictionary = ArtBank.metadata(id)
	var source_size: Array = meta.get("size", [128, 176])
	var art: Array = meta.get("artRect", [8, 29, 112, 72])
	var factor: Vector2 = rect.size / Vector2(float(source_size[0]), float(source_size[1]))
	var art_rect: Rect2 = Rect2(rect.position + Vector2(float(art[0]), float(art[1])) * factor, Vector2(float(art[2]), float(art[3])) * factor)
	surface.draw_rect(rect, INK)
	if not card.definition_id.is_empty():
		cover(surface, ArtBank.texture("illustration." + card.definition_id), art_rect)
	surface.draw_texture_rect(ArtBank.texture(id), rect, false)
	var font_size: int = 12 if rect.size.x < 110 else (14 if rect.size.x < 150 else 18)
	var name: Array = meta.get("nameRect", [10, 8, 78, 17])
	var name_position: Vector2 = rect.position + Vector2(float(name[0]), float(name[1])) * factor
	surface.draw_string(FONT, name_position + Vector2(0, font_size), shortened(card.title, float(name[2]) * factor.x, font_size), HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, INK)
	var status: String = card.status if not card.status.is_empty() else card.type_label
	var type_position: Vector2 = Vector2(art_rect.position.x, art_rect.end.y + 13)
	var type_width: float = rect.size.x - 36
	var compact_type: String = {"analysis": "解析", "incantation": "言灵", "rune": "符文", "talisman": "咒符", "formation": "阵法"}.get(card.type_key, "")
	if card.type_label.begins_with("行动"):
		compact_type = "行动"
	surface.draw_string(FONT, type_position, shortened(compact_type + " " + str(card.speed), type_width, 12), HORIZONTAL_ALIGNMENT_LEFT, -1, 12, INK)
	var rarity_anchor: Array = meta.get("rarityAnchor", [108, 109])
	var rarity_position: Vector2 = rect.position + Vector2(float(rarity_anchor[0]), float(rarity_anchor[1])) * factor
	surface.draw_texture_rect(ArtBank.texture("c2d.rarity." + card.rarity), Rect2(rarity_position - Vector2(8, 8), Vector2(16, 16)), false)
	var effect: Array = meta.get("effectRect", [12, 121, 104, 35])
	var effect_position: Vector2 = rect.position + Vector2(float(effect[0]), float(effect[1])) * factor
	surface.draw_string(FONT, effect_position + Vector2(0, font_size), shortened(status, float(effect[2]) * factor.x, font_size), HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, INK)
	var anchors: Array = meta.get("valueAnchors", [[104, 24], [24, 158]])
	var values: Array[String] = [card.primary_value, card.secondary_value]
	for index: int in range(2):
		var anchor: Array = anchors[index]
		var position: Vector2 = rect.position + Vector2(float(anchor[0]), float(anchor[1])) * factor
		var value_width: float = FONT.get_string_size(values[index], HORIZONTAL_ALIGNMENT_LEFT, -1, 14).x
		surface.draw_string(FONT, position + Vector2(-value_width * 0.5, 5), values[index], HORIZONTAL_ALIGNMENT_LEFT, -1, 14, INK)
	if highlighted:
		surface.draw_rect(rect.grow(3), Color("64cfe6"), false, 2)
