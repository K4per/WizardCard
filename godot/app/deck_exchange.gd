class_name DeckExchange
extends RefCounted

const FORMAT: String = "wizardcard-deck"
const VERSION: int = 1
const MAX_BYTES: int = 1048576

static func encode(draft: Dictionary) -> Dictionary:
	var document: Dictionary = {"format": FORMAT, "version": VERSION, "name": str(draft.get("name", "")), "main": [], "side": []}
	for mapping: Array in [["main", "cards"], ["side", "sideCards"]]:
		var cards: Dictionary = draft.get(mapping[1], {})
		var ids: Array = cards.keys()
		ids.sort()
		for id: String in ids:
			document[mapping[0]].append({"id": id, "count": int(cards[id])})
	return document

static func decode(document: Variant) -> Dictionary:
	if not document is Dictionary:
		return {"error": "牌组文件根节点必须为对象"}
	if document.get("format") != FORMAT or document.get("version") != VERSION:
		return {"error": "牌组文件格式或版本不兼容（需要 wizardcard-deck / 1）"}
	for key: Variant in document.keys():
		if not key in ["format", "version", "name", "main", "side"]:
			return {"error": "未知牌组字段：" + str(key)}
	if not document.get("name") is String or str(document["name"]).strip_edges().is_empty() or str(document["name"]).length() > 80:
		return {"error": "name：需要 1–80 字符的牌组名称"}
	var draft: Dictionary = {"format": 3, "name": document["name"], "cards": {}, "sideCards": {}}
	for mapping: Array in [["main", "cards"], ["side", "sideCards"]]:
		var section: String = mapping[0]
		if not document.get(section) is Array or document[section].size() > 1000:
			return {"error": section + "：需要卡牌条目数组"}
		for index: int in range(document[section].size()):
			var row: Variant = document[section][index]
			var location: String = "%s[%d]" % [section, index]
			if not row is Dictionary or row.size() != 2 or not row.get("id") is String or str(row["id"]).is_empty():
				return {"error": location + "：需要 id 与 count"}
			var count: Variant = row.get("count")
			if not (count is int or count is float) or not is_finite(float(count)) or float(count) != floorf(float(count)) or count < 1 or count > 3:
				return {"error": location + " / " + str(row["id"]) + "：数量必须为 1–3 的整数"}
			if draft[mapping[1]].has(row["id"]):
				return {"error": location + "：重复卡牌条目 " + str(row["id"])}
			draft[mapping[1]][row["id"]] = int(count)
	return {"draft": draft}

static func read_document(path: String) -> Dictionary:
	var file: FileAccess = FileAccess.open(path, FileAccess.READ)
	if file == null:
		return {"error": "无法读取牌组文件：" + error_string(FileAccess.get_open_error())}
	if file.get_length() > MAX_BYTES:
		return {"error": "牌组文件超过 1 MiB 上限"}
	var parser: JSON = JSON.new()
	if parser.parse(file.get_as_text()) != OK:
		return {"error": "JSON 第 %d 行：%s" % [parser.get_error_line(), parser.get_error_message()]}
	return decode(parser.data)
