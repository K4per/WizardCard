class_name DeckDetailPane
extends PanelContainer

var service: FrontendService
var card: CardPresentation
var _title: Label
var _face: DeckCardPainter
var _body: RichTextLabel
var _hint: Label

func _ready() -> void:
	_title = %Title
	_face = %Face
	_body = %Body
	_hint = %Hint
	theme = DeckTheme.create()
	_hint.add_theme_color_override(&"font_color", DeckTheme.MUTED)

func inspect(id: String) -> void:
	if service == null:
		return
	card = service.card_by_id(id)
	if card == null:
		return
	_title.text = card.title
	_face.configure(card)
	var data: Dictionary = service.definitions.get(id, {})
	var rarity: String = str(data.get("rarityLabel", card.rarity_text()))
	var lines: Array[String] = [card.type_label + (" · " + rarity if not rarity.is_empty() else "")]
	if card.type_key == "formation":
		lines.append(card.cost_detail())
	lines.append(card.detail_text())
	var tags: Array = data.get("tags", [])
	var tag_names: PackedStringArray = []
	for value: Variant in tags:
		tag_names.append(str(value))
	if not tag_names.is_empty():
		lines.append("标签：" + "、".join(tag_names))
	if not str(data.get("flavor", "")).is_empty():
		lines.append(str(data["flavor"]))
	_body.text = "\n\n".join(lines)
	_body.scroll_to_line(0)
