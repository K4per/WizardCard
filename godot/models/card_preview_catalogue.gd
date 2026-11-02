class_name CardPreviewCatalogue
extends RefCounted

var cards: Array[CardPresentation] = []
var version: String = ""
var error: String = ""

static func load_catalogue() -> CardPreviewCatalogue:
	var catalogue: CardPreviewCatalogue = CardPreviewCatalogue.new()
	var document: Variant = JSON.parse_string(FileAccess.get_file_as_string("res://data/card-previews-v2.json"))
	if not document is Dictionary or (document as Dictionary).get("usage", "") != "illustrated-preview":
		catalogue.error = "2.0 卡牌图鉴读取失败"
		return catalogue
	var data: Dictionary = document
	catalogue.version = str(data.get("cardSetVersion", ""))
	var rows: Array = data.get("cards", [])
	for definition: Dictionary in rows:
		catalogue.cards.append(CardPresentation.from_preview_definition(definition))
	return catalogue
