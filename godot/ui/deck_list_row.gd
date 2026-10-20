class_name DeckListRow
extends Button

var service: FrontendService
var draft: Dictionary = {}

func _ready() -> void:
	texture_filter = CanvasItem.TEXTURE_FILTER_LINEAR
	custom_minimum_size.y = 68
	resized.connect(queue_redraw)

func _draw() -> void:
	if service == null or draft.is_empty():
		return
	var thumbnail: Rect2 = Rect2(12, 8, 56, maxf(0.0, size.y - 16))
	var side: Dictionary = draft.get("sideCards", {})
	var main_cards: Dictionary = draft.get("cards", {})
	var cover_id: String = str(side.keys()[0]) if not side.is_empty() else str(main_cards.keys()[0]) if not main_cards.is_empty() else ""
	draw_rect(thumbnail, DeckTheme.BACKGROUND)
	if not cover_id.is_empty() and service.definitions.has(cover_id):
		var cover: CardPresentation = service.card_by_id(cover_id)
		var image: Texture2D = ArtBank.texture("illustration." + cover.art_id())
		if image != null:
			var image_scale: float = minf(thumbnail.size.x / image.get_width(), thumbnail.size.y / image.get_height())
			var extent: Vector2 = image.get_size() * image_scale
			draw_texture_rect(image, Rect2(thumbnail.get_center() - extent * 0.5, extent), false)
	var left: float = thumbnail.end.x + 16
	var available_width: float = maxf(0.0, size.x - left - 16)
	var title_size: int = Typography.scaled_size(18)
	var subtitle_size: int = Typography.scaled_size(14)
	var title: String = _fit_text(str(draft.get("name", "未命名卡组")), available_width, title_size)
	draw_string(Typography.BODY, Vector2(left, 6 + Typography.BODY.get_ascent(title_size)), title, HORIZONTAL_ALIGNMENT_LEFT, -1, title_size, DeckTheme.TEXT)
	var main_count: int = DeckUi.total(main_cards)
	var side_count: int = DeckUi.total(side)
	var problems: Array = draft.get("problems", [])
	var subtitle: String = "主卡组 %d / 40    副卡组 %d / 10    %s" % [main_count, side_count, "可用于对局" if problems.is_empty() else "草稿 · 点击编辑"]
	if int(draft.get("format", 1)) == 1:
		subtitle = "旧版草稿 · 点击按新规则编辑"
	var subtitle_y: float = 8 + Typography.BODY.get_height(title_size) + Typography.BODY.get_ascent(subtitle_size)
	draw_string(Typography.BODY, Vector2(left, subtitle_y), _fit_text(subtitle, available_width, subtitle_size), HORIZONTAL_ALIGNMENT_LEFT, -1, subtitle_size, DeckTheme.MUTED)

func _fit_text(value: String, width: float, font_size: int) -> String:
	if Typography.BODY.get_string_size(value, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x <= width:
		return value
	var shortened: String = value
	while not shortened.is_empty() and Typography.BODY.get_string_size(shortened + "…", HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x > width:
		shortened = shortened.left(shortened.length() - 1)
	return shortened + "…"
