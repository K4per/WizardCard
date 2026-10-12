class_name DeckDetailPane
extends Panel

var service: FrontendService
var _title: Label
var _face: CardFace
var _body: RichTextLabel
var _hint: Label

func _ready() -> void:
	_title = %Title
	_face = %Face
	_body = %Body
	_hint = %Hint
	add_theme_stylebox_override(&"panel", ArtBank.box("c2d.v2.deck.detail-panel", 0))

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
		lines.append("设置条件：" + str(data.get("settingConditions", "无额外条件")))
	lines.append("标签：" + "、".join(data.get("tags", [])))
	lines.append(card.text)
	if not str(data.get("flavor", "")).is_empty():
		lines.append(str(data["flavor"]))
	_body.text = "\n\n".join(lines)
	_body.scroll_to_line(0)
