class_name SimpleCardPainter
extends RefCounted

static func paint(surface: CanvasItem, card: CardPresentation, rect: Rect2, highlighted: bool = false) -> void:
	if card == null:
		return
	var accent: Color = {"formation": Color("80b8a3"), "analysis": Color("82b4d7"), "incantation": Color("d9b780"), "rune": Color("ab94d8"), "talisman": Color("cf989b")}.get(card.type_key, Color("9bafbc"))
	surface.draw_rect(rect, Color("1d2c39"), true)
	surface.draw_rect(rect, Color("a7c6d1") if highlighted else accent.darkened(0.30), false, 2)
	var accent_height: float = 3.0 if rect.size.y < 90 else 5.0
	surface.draw_rect(Rect2(rect.position, Vector2(rect.size.x, accent_height)), accent, true)
	if card.hidden or card.face_down:
		_label(surface, "盖伏", rect.grow(-4), maxi(10, mini(20, int(rect.size.x * 0.22))), Color("b7c6ce"))
		return
	var font_size: int = clampi(int(rect.size.x * 0.13), 9, 22)
	var cost_size: int = maxi(8, mini(20, int(rect.size.x * 0.16)))
	var cost_width: float = minf(rect.size.x * 0.44, Typography.BODY.get_string_size(card.primary_value, HORIZONTAL_ALIGNMENT_LEFT, -1, cost_size).x + 8.0)
	var cost_rect: Rect2 = Rect2(rect.end.x - cost_width - 3, rect.position.y + accent_height, cost_width, cost_size + 4)
	if card.type_key != "formation":
		surface.draw_rect(cost_rect, Color("31485d"), true)
		_label(surface, card.primary_value, cost_rect, cost_size, Color("f2cb62"))
	var title_width: float = rect.size.x - 8 - (cost_width + 3 if card.type_key != "formation" else 0)
	var title_rect: Rect2 = Rect2(rect.position.x + 4, rect.position.y + accent_height, title_width, rect.size.y - accent_height) if rect.size.y < 90 else Rect2(rect.position.x + 5, rect.position.y + cost_size + 12, rect.size.x - 10, font_size + 6)
	if rect.size.y < 90:
		font_size = mini(font_size, 16)
		while font_size > 8 and Typography.BODY.get_height(font_size) > title_rect.size.y - 2:
			font_size -= 1
	_label(surface, card.title, title_rect, font_size, Color("eff3f5"))
	if rect.size.y >= 90:
		_label(surface, card.type_label, Rect2(rect.position.x + 6, rect.end.y - 45, rect.size.x - 12, 18), maxi(10, font_size - 2), accent)
		_label(surface, "Lv.%s" % card.secondary_value, Rect2(rect.position.x + 6, rect.end.y - 24, rect.size.x - 12, 18), maxi(9, font_size - 3), Color("b7c6ce"))

static func _label(surface: CanvasItem, text: String, rect: Rect2, font_size: int, color: Color) -> void:
	font_size = Typography.fitted_size(font_size, rect.size.y)
	var displayed: String = text
	while displayed.length() > 1 and Typography.BODY.get_string_size(displayed, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x > rect.size.x:
		displayed = displayed.left(displayed.length() - 1)
	var width: float = Typography.BODY.get_string_size(displayed, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x
	surface.draw_string(Typography.BODY, rect.position + Vector2(maxf(0, (rect.size.x - width) * 0.5), (rect.size.y - Typography.BODY.get_height(font_size)) * 0.5 + Typography.BODY.get_ascent(font_size)), displayed, HORIZONTAL_ALIGNMENT_LEFT, rect.size.x, font_size, color)
