class_name DeckListRow
extends Button

var service: FrontendService
var draft: Dictionary = {}

func _ready() -> void:
	texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	for state: StringName in [&"normal", &"hover", &"pressed", &"disabled"]:
		var skin: StyleBoxTexture = ArtBank.box("c2d.v2.deck.list-entry", 0)
		if state == &"hover":
			skin.modulate_color = Color(1.1, 1.15, 1.15)
		add_theme_stylebox_override(state, skin)
	resized.connect(queue_redraw)

func _draw() -> void:
	if service == null or draft.is_empty():
		return
	var side: Dictionary = draft.get("sideCards", {})
	var cover_id: String = str(side.keys()[0]) if not side.is_empty() else ""
	var cover: CardPresentation = service.card_by_id(cover_id)
	CardPainter.paint(self, cover, Rect2(24, 8, size.y * 0.6, size.y - 16), "builder")
	var left: float = size.y * 0.6 + 48
	var title: String = CardPainter.shortened(str(draft.get("name", "未命名卡组")), size.x - left - 28, 20, Typography.HEADING)
	draw_string(Typography.HEADING, Vector2(left, size.y * 0.43), title, HORIZONTAL_ALIGNMENT_LEFT, -1, 20, CardPainter.PAPER)
	var main: int = DeckUi.total(draft.get("cards", {}))
	var side_count: int = DeckUi.total(draft.get("sideCards", {}))
	var problems: Array = draft.get("problems", [])
	var subtitle: String = "主卡组 %d / 40 · 副卡组 %d / 10 · %s" % [main, side_count, "构筑检查通过" if problems.is_empty() else "草稿 · 点击编辑"]
	if int(draft.get("format", 1)) == 1:
		subtitle = "旧版草稿 · 点击按新规则编辑"
	draw_string(Typography.BODY, Vector2(left, size.y * 0.78), CardPainter.shortened(subtitle, size.x - left - 28, 14), HORIZONTAL_ALIGNMENT_LEFT, -1, 14, Color("a8c4bc"))
