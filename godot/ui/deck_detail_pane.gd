class_name DeckDetailPane
extends Panel

var service: FrontendService
var _title: Label
var _face: CardFace
var _body: RichTextLabel
var _hint: Label

func _ready() -> void:
	add_theme_stylebox_override(&"panel", ArtBank.box("c2d.v2.deck.detail-panel", 0))
	_title = UiFactory.label("卡牌详情", 24)
	_title.clip_text = true
	add_child(_title)
	_face = CardFace.new()
	_face.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_face)
	_body = RichTextLabel.new()
	_body.bbcode_enabled = false
	_body.add_theme_font_size_override(&"normal_font_size", 16)
	add_child(_body)
	_hint = UiFactory.label("右键增减 · 拖动 · 悬停详情", 12, true)
	add_child(_hint)
	resized.connect(_reflow)
	_reflow()

func _reflow() -> void:
	if _face != null:
		var compact: bool = size.y < 650
		var inset: float = 32 if compact else 44
		var top: float = 52
		var face_height: float = minf(360, size.y * 0.42)
		DeckUi.place(_title, Rect2(inset, top, size.x - inset * 2, 32))
		DeckUi.place(_face, Rect2(inset, top + 44, size.x - inset * 2, face_height))
		var body_y: float = top + 60 + face_height
		DeckUi.place(_body, Rect2(inset, body_y, size.x - inset * 2, maxf(40, size.y - body_y - 88)))
		DeckUi.place(_hint, Rect2(inset + 12, size.y - 84, size.x - inset * 2 - 24, 28))

func inspect(id: String) -> void:
	if service == null:
		return
	var card: CardPresentation = service.card_by_id(id)
	_title.text = card.title
	_face.configure(card)
	var data: Dictionary = service.definitions.get(id, {})
	var lines: Array[String] = [card.type_label + " · " + str(data.get("rarityLabel", "")), "%s：%s" % [data.get("costLabel", "费用"), card.primary_value]]
	if int(data.get("type", -1)) == 1:
		lines.append("施法费用：%s · 位阶 %s" % [data.get("castCost", 0), data.get("rank", 0)])
	if int(data.get("type", -1)) == 3:
		lines.append("阵体 %s · 荷载上限 %s\n环位 %s · 魔素收入 %s" % [data.get("body", 0), data.get("capacity", 0), data.get("rings", 0), data.get("income", 0)])
	lines.append("标签：" + "、".join(data.get("tags", [])))
	lines.append(card.text)
	if not str(data.get("flavor", "")).is_empty():
		lines.append(str(data["flavor"]))
	_body.text = "\n\n".join(lines)
	_body.scroll_to_line(0)
