class_name CardPainter
extends RefCounted

const FONT: Font = Typography.BODY
const PAPER: Color = Color("f6eed8")
const INK: Color = Color("08131f")

static func shortened(text: String, width: float, font_size: int, font: Font = FONT) -> String:
	if font.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x <= width:
		return text
	var result: String = text
	while not result.is_empty() and font.get_string_size(result + "…", HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x > width:
		result = result.left(result.length() - 1)
	return result + "…"

static func frame_id(card: CardPresentation) -> String:
	return "c2d.card.back.ai" if card.hidden or card.face_down else "c2d.card.detail." + card.type_key + ".ai"

static func fitted_rect(bounds: Rect2) -> Rect2:
	var width: float = minf(bounds.size.x, bounds.size.y * 0.75)
	var dimensions: Vector2 = Vector2(width, width / 0.75)
	return Rect2(bounds.get_center() - dimensions * 0.5, dimensions)

static func scaled_rect(bounds: Rect2, scale_factor: float) -> Rect2:
	var fitted: Rect2 = fitted_rect(bounds)
	var dimensions: Vector2 = fitted.size * maxf(1.0, scale_factor)
	return Rect2(bounds.get_center() - dimensions * 0.5, dimensions)

static func _region(layout: Dictionary, key: String, rect: Rect2, factor: Vector2) -> Rect2:
	var pixels: Array = layout[key]
	return Rect2(rect.position + Vector2(float(pixels[0]), float(pixels[1])) * factor, Vector2(float(pixels[2]), float(pixels[3])) * factor)

static func _label(surface: CanvasItem, text: String, bounds: Rect2, font_size: int, centered: bool = false, font: Font = FONT) -> void:
	font_size = Typography.fitted_size(font_size, bounds.size.y, font)
	var displayed: String = shortened(text, bounds.size.x, font_size, font)
	var width: float = font.get_string_size(displayed, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x
	var offset: float = (bounds.size.x - width) * 0.5 if centered else 0.0
	var baseline: float = (bounds.size.y - font.get_height(font_size)) * 0.5 + font.get_ascent(font_size)
	surface.draw_string(font, bounds.position + Vector2(offset, baseline), displayed, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, INK)

static func _effect(surface: CanvasItem, text: String, bounds: Rect2, font_size: int) -> void:
	font_size = Typography.fitted_size(font_size, bounds.size.y)
	var line_height: float = FONT.get_height(font_size) + 1
	var max_lines: int = maxi(1, int(bounds.size.y / line_height))
	var lines: Array[String] = []
	var line: String = ""
	for character: String in text:
		if character == "\n" or FONT.get_string_size(line + character, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x > bounds.size.x:
			lines.append(line)
			line = ""
			if lines.size() == max_lines:
				lines[max_lines - 1] = shortened(lines[max_lines - 1] + "…", bounds.size.x, font_size)
				break
		if character != "\n":
			line += character
	if lines.size() < max_lines and not line.is_empty():
		lines.append(line)
	for index: int in range(lines.size()):
		surface.draw_string(FONT, bounds.position + Vector2(0, FONT.get_ascent(font_size) + index * line_height), lines[index], HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, INK)

static func paint(surface: CanvasItem, card: CardPresentation, bounds: Rect2, view: String = "field", highlighted: bool = false) -> void:
	if card == null:
		return
	var rect: Rect2 = fitted_rect(bounds)
	var id: String = frame_id(card)
	if not card.hidden and not card.face_down:
		var meta: Dictionary = ArtBank.metadata(id)
		var source_size: Array = meta["size"]
		var factor: Vector2 = rect.size / Vector2(float(source_size[0]), float(source_size[1]))
		var layout: Dictionary = meta["runtimeLayout"]
		var art_rect: Rect2 = _region(layout, "art", rect, factor)
		# Preserve the original transparent silhouette and every ornament.
		surface.draw_rect(art_rect, INK)
		var illustration: Texture2D = null
		var illustration_id: String = card.art_id()
		# Projections and smoke fixtures may omit illustrationId or use a card
		# definition that has no published art. Leave the art region empty instead
		# of asking ArtBank to log an error on every redraw.
		if not illustration_id.is_empty() and not ArtBank.metadata("illustration." + illustration_id).is_empty():
			illustration = ArtBank.texture("illustration." + illustration_id)
		if illustration != null:
			var scale_factor: float = minf(art_rect.size.x / illustration.get_width(), art_rect.size.y / illustration.get_height())
			var dimensions: Vector2 = illustration.get_size() * scale_factor
			surface.draw_texture_rect(illustration, Rect2(art_rect.get_center() - dimensions * 0.5, dimensions), false)
		surface.draw_texture_rect(ArtBank.texture(id), rect, false)
		var font_size: int = clampi(roundi(rect.size.x * 0.065), 7, 22)
		if rect.size.x >= 75:
			_label(surface, card.title, _region(layout, "name", rect, factor), font_size, false, Typography.HEADING)
		if not card.rarity.is_empty():
			var rarity_rect: Rect2 = _region(layout, "rarity", rect, factor)
			var gem: Texture2D = ArtBank.texture("c2d.rarity." + card.rarity)
			var gem_size: float = rarity_rect.size.y
			surface.draw_texture_rect(gem, Rect2(rarity_rect.get_center() - Vector2.ONE * gem_size * 0.5, Vector2.ONE * gem_size), false)
		var values: Array[String] = ["", card.secondary_value]
		var anchors: Array = layout["values"]
		var value_size: int = clampi(roundi(rect.size.x * 0.085), 7, 28)
		for index: int in range(2):
			var anchor: Array = anchors[index]
			var center: Vector2 = rect.position + Vector2(float(anchor[0]), float(anchor[1])) * factor
			_label(surface, values[index], Rect2(center - Vector2(rect.size.x * 0.05, value_size), Vector2(rect.size.x * 0.1, value_size * 2)), value_size, true, Typography.HEADING)
		if card.type_key != "formation":
			var cost_width: float = maxf(rect.size.x * 0.24, FONT.get_string_size(card.primary_value, HORIZONTAL_ALIGNMENT_LEFT, -1, value_size).x + 8)
			var cost_rect: Rect2 = Rect2(rect.end.x - cost_width - rect.size.x * 0.035, rect.position.y + rect.size.y * 0.07, cost_width, value_size * 1.4)
			surface.draw_rect(cost_rect, PAPER, true)
			_label(surface, card.primary_value, cost_rect, value_size, true, Typography.HEADING)
		# Tiny casting cards keep the full frame and values. Larger views place
		# actual rules text inside the paper panel; details retain full text.
		if rect.size.x >= 100:
			var compact_type: String = {"analysis": "解析", "incantation": "言灵", "rune": "符文", "talisman": "咒符", "formation": "阵法"}.get(card.type_key, "")
			if card.type_label.begins_with("行动"):
				compact_type = "行动"
			var type_rect: Rect2 = _region(layout, "type", rect, factor)
			var type_size: int = maxi(5, mini(font_size - 1, floori(type_rect.size.x * 0.49)))
			_label(surface, compact_type, type_rect, type_size, true)
			if card.speed > 0:
				_label(surface, "%d速" % card.speed, _region(layout, "speed", rect, factor), font_size, true)
			var effect: String = card.text
			if not card.status.is_empty():
				effect = card.status + "\n" + effect
			_effect(surface, effect, _region(layout, "effect", rect, factor), maxi(8, font_size - 1))
			if view == "detail":
				_label(surface, card.number if not card.number.is_empty() else card.definition_id, _region(layout, "identifier", rect, factor), maxi(7, font_size - 3), true)
	else:
		surface.draw_texture_rect(ArtBank.texture(id), rect, false)
	if highlighted:
		surface.draw_rect(rect.grow(3), Color("64cfe6"), false, 2)

static func paint_art(surface: CanvasItem, card: CardPresentation, bounds: Rect2, highlighted: bool = false) -> void:
	# Hidden projections never resolve an illustration ID.
	var image: Texture2D = ArtBank.texture("c2d.card.back.ai") if card.hidden or card.face_down else null
	if image == null and not card.art_id().is_empty() and not ArtBank.metadata("illustration." + card.art_id()).is_empty():
		image = ArtBank.texture("illustration." + card.art_id())
	var inner: Rect2 = bounds.grow(-bounds.size.x * 0.085)
	surface.draw_rect(inner, INK)
	if image != null:
		var factor: float = maxf(inner.size.x / image.get_width(), inner.size.y / image.get_height())
		var crop: Vector2 = inner.size / factor
		surface.draw_texture_rect_region(image, inner, Rect2((image.get_size() - crop) * 0.5, crop))
	surface.draw_texture_rect(BattleArt.texture("spell_art_selected" if highlighted else "spell_art_normal"), bounds, false)
