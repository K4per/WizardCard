class_name DeckCardPainter
extends Control

static var FONT: Font = Typography.BODY
static var TITLE_FONT: Font = Typography.HEADING
const CARD_FILL: Color = DeckTheme.CONTROL
const ART_FILL: Color = DeckTheme.BACKGROUND
const BORDER: Color = DeckTheme.BORDER
const TEXT: Color = DeckTheme.TEXT
const MUTED: Color = DeckTheme.MUTED

var card: CardPresentation
@export var illustration_only: bool = false

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	texture_filter = CanvasItem.TEXTURE_FILTER_LINEAR
	resized.connect(queue_redraw)

func configure(value: CardPresentation) -> void:
	card = value
	queue_redraw()

func _draw() -> void:
	var rect: Rect2 = Rect2(Vector2.ZERO, size)
	if illustration_only:
		paint_artwork(self, card, rect)
	else:
		paint(self, card, rect)

static func artwork_rect(bounds: Rect2) -> Rect2:
	var rect: Rect2 = CardPainter.fitted_rect(bounds)
	var padding: float = minf(6.0, rect.size.x * 0.045)
	var header: float = minf(29.0, rect.size.y * 0.23)
	var footer: float = minf(43.0, rect.size.y * 0.22)
	return Rect2(rect.position + Vector2(padding, header), Vector2(rect.size.x - padding * 2, rect.size.y - header - footer - padding))

static func paint_artwork(surface: CanvasItem, value: CardPresentation, rect: Rect2) -> void:
	if rect.size.x <= 0.0 or rect.size.y <= 0.0:
		return
	surface.draw_rect(rect, ART_FILL)
	if value == null or value.hidden or value.face_down or value.art_id().is_empty():
		return
	var illustration: Texture2D = ArtBank.texture("illustration." + value.art_id())
	if illustration == null or illustration.get_width() <= 0 or illustration.get_height() <= 0:
		return
	var scale_factor: float = minf(rect.size.x / illustration.get_width(), rect.size.y / illustration.get_height())
	var dimensions: Vector2 = illustration.get_size() * scale_factor
	surface.draw_texture_rect(illustration, Rect2(rect.get_center() - dimensions * 0.5, dimensions), false)

static func paint_covered(surface: CanvasItem, rect: Rect2) -> void:
	surface.draw_rect(rect, CARD_FILL)
	surface.draw_rect(rect.grow(-0.5), BORDER, false, 1)
	_label(surface, GameText.label("ui.card.face_down"), rect.grow(-1), 14, FONT, MUTED, true)

static func paint(surface: CanvasItem, value: CardPresentation, bounds: Rect2, highlighted: bool = false, focus_color: Color = DeckTheme.ACCENT) -> void:
	var rect: Rect2 = CardPainter.fitted_rect(bounds)
	if value == null or not rect.has_area():
		return
	if value.hidden or value.face_down:
		paint_covered(surface, rect)
	else:
		surface.draw_rect(rect, CARD_FILL)
		var art: Rect2 = artwork_rect(rect)
		paint_artwork(surface, value, art)
		var title: Rect2 = Rect2(art.position.x, rect.position.y, art.size.x, art.position.y - rect.position.y)
		_label(surface, value.title, title, clampi(roundi(rect.size.x * 0.13), 10, 17), TITLE_FONT, TEXT)
		var status: String = value.status.get_slice("\n", 0).get_slice(" · ", 0).replace(GameText.label("ui.card.analysis_prefix"), GameText.label("ui.card.short_remaining"))
		var token: String = value.token_label()
		if status.is_empty():
			status = token
		elif not token.is_empty() and art.size.y >= 20:
			var tag: Rect2 = Rect2(art.position.x, art.end.y - 18, art.size.x, 18)
			surface.draw_rect(tag, Color(0.02, 0.07, 0.10, 0.86))
			_label(surface, token, tag, 12, FONT, TEXT, true)
		var cost: String = value.primary_value
		if value.type_key == "analysis" and not value.dynamic_cost and rect.size.x >= 85:
			cost = GameText.label("ui.card.short_spell_cost", {"setting": value.setting_cost, "casting": value.cast_cost})
		elif value.type_key == "formation":
			cost = GameText.label("ui.card.level_prefix", {"level": value.secondary_value})
		elif value.type_key in ["incantation", "talisman"]:
			cost = GameText.label("ui.card.cast_prefix", {"cost": cost})
		elif value.type_key == "rune":
			cost = GameText.label("ui.card.attach_prefix", {"cost": cost})
		var footer: Rect2 = Rect2(art.position.x, art.end.y, art.size.x, rect.end.y - art.end.y - 1)
		if footer.size.y >= 27:
			var first: Rect2 = Rect2(footer.position, Vector2(footer.size.x, footer.size.y * 0.5))
			_label(surface, value.type_label if status.is_empty() else status, first, 13, FONT, MUTED)
			_label(surface, cost, Rect2(first.position + Vector2(0, first.size.y), first.size), 13, FONT, TEXT)
		else:
			_label(surface, cost if status.is_empty() else status, footer, 13, FONT, TEXT)
	surface.draw_rect(rect.grow(-0.5), focus_color if highlighted else BORDER, false, 2.0 if highlighted else 1.0)

static func _label(surface: CanvasItem, text: String, rect: Rect2, font_size: int, font: Font, color: Color, centered: bool = false) -> void:
	if rect.size.x <= 0.0 or rect.size.y <= 0.0:
		return
	font_size = Typography.fitted_size(font_size, rect.size.y, font)
	var displayed: String = text
	if font.get_string_size(displayed, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x > rect.size.x:
		while not displayed.is_empty() and font.get_string_size(displayed + "…", HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x > rect.size.x:
			displayed = displayed.left(displayed.length() - 1)
		displayed += "…"
	var baseline: float = (rect.size.y - font.get_height(font_size)) * 0.5 + font.get_ascent(font_size)
	var width: float = rect.size.x if centered else -1.0
	var offset: float = (rect.size.x - font.get_string_size(displayed, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x) * 0.5 if centered else 0.0
	surface.draw_string(font, rect.position + Vector2(offset, baseline), displayed, HORIZONTAL_ALIGNMENT_LEFT, width, font_size, color)
