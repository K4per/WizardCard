class_name DeckCardPainter
extends Control

const FONT: Font = Typography.BODY
const TITLE_FONT: Font = Typography.HEADING
const CARD_FILL: Color = DeckTheme.CONTROL
const ART_FILL: Color = DeckTheme.BACKGROUND

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

static func artwork_rect(rect: Rect2) -> Rect2:
	var padding: float = clampf(rect.size.x * 0.065, 4.0, 9.0)
	var header: float = clampf(rect.size.y * 0.18, 20.0, 30.0)
	var footer: float = clampf(rect.size.y * 0.27, 32.0, 42.0)
	return Rect2(rect.position + Vector2(padding, header), Vector2(maxf(0.0, rect.size.x - padding * 2.0), maxf(0.0, rect.size.y - header - footer - padding)))

static func paint_artwork(surface: CanvasItem, value: CardPresentation, rect: Rect2) -> void:
	if rect.size.x <= 0.0 or rect.size.y <= 0.0:
		return
	surface.draw_rect(rect, ART_FILL)
	if value == null or value.hidden or value.art_id().is_empty() or ArtBank.metadata("illustration." + value.art_id()).is_empty():
		return
	var illustration: Texture2D = ArtBank.texture("illustration." + value.art_id())
	if illustration == null or illustration.get_width() <= 0 or illustration.get_height() <= 0:
		return
	var scale_factor: float = minf(rect.size.x / illustration.get_width(), rect.size.y / illustration.get_height())
	var dimensions: Vector2 = illustration.get_size() * scale_factor
	surface.draw_texture_rect(illustration, Rect2(rect.get_center() - dimensions * 0.5, dimensions), false)

static func paint(surface: CanvasItem, value: CardPresentation, rect: Rect2, highlighted: bool = false) -> void:
	if value == null or rect.size.x <= 0.0 or rect.size.y <= 0.0:
		return
	surface.draw_rect(rect, CARD_FILL)
	surface.draw_rect(rect, DeckTheme.ACCENT if highlighted else DeckTheme.BORDER, false, 2.0 if highlighted else 1.0)
	if value.hidden or value.face_down:
		_label(surface, "盖伏", rect.grow(-4.0), maxi(10, mini(20, int(rect.size.x * 0.22))), FONT, DeckTheme.MUTED, true)
		return
	var artwork: Rect2 = artwork_rect(rect)
	paint_artwork(surface, value, artwork)
	var padding: float = artwork.position.x - rect.position.x
	var font_size: int = clampi(roundi(rect.size.x * 0.1), 10, 16)
	var title_area: Rect2 = Rect2(rect.position + Vector2(padding, 1.0), Vector2(artwork.size.x, artwork.position.y - rect.position.y - 2.0))
	_label(surface, value.title, title_area, font_size, TITLE_FONT, DeckTheme.TEXT)
	var footer_height: float = maxf(0.0, rect.end.y - padding - artwork.end.y)
	var line_height: float = footer_height * 0.5
	var type_area: Rect2 = Rect2(Vector2(artwork.position.x, artwork.end.y), Vector2(artwork.size.x, line_height))
	var cost_area: Rect2 = Rect2(type_area.position + Vector2(0.0, line_height), type_area.size)
	_label(surface, value.type_label, type_area, maxi(9, font_size - 1), FONT, DeckTheme.MUTED)
	var costs: String = "施法 %d" % value.cast_cost if value.type_key in ["incantation", "talisman"] else "设%d / 施%d" % [value.setting_cost, value.cast_cost]
	if value.type_key == "formation":
		costs = "等级 %s" % value.secondary_value
	elif value.type_key == "rune":
		costs = "附着 %d" % value.setting_cost
	_label(surface, costs, cost_area, maxi(9, font_size - 1), FONT, DeckTheme.TEXT)

static func _label(surface: CanvasItem, text: String, rect: Rect2, font_size: int, font: Font, color: Color, centered: bool = false) -> void:
	if rect.size.x <= 0.0 or rect.size.y <= 0.0:
		return
	var displayed: String = text
	if font.get_string_size(displayed, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x > rect.size.x:
		while not displayed.is_empty() and font.get_string_size(displayed + "…", HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x > rect.size.x:
			displayed = displayed.left(displayed.length() - 1)
		displayed += "…"
	var baseline: float = (rect.size.y - font.get_height(font_size)) * 0.5 + font.get_ascent(font_size)
	var width: float = rect.size.x if centered else -1.0
	var offset: float = (rect.size.x - font.get_string_size(displayed, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x) * 0.5 if centered else 0.0
	surface.draw_string(font, rect.position + Vector2(offset, baseline), displayed, HORIZONTAL_ALIGNMENT_LEFT, width, font_size, color)
