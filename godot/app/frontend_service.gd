class_name FrontendService
extends RefCounted

var error: String = ""
var cards: Array[CardPresentation] = []
var settings: Dictionary = {}
var rules_version: String = ""
var definitions: Dictionary[String, Dictionary] = {}
var drafts: Array[Dictionary] = []
var presets: Array[Dictionary] = []
var playable: Array[Dictionary] = []
var writable: bool = false
var notice: String = ""
var editing_draft: Dictionary = {}
var saved_draft: Dictionary = {}
var match_active: bool = false
var match_mode: String = ""
var match_viewer: int = 0
var match_snapshot: Dictionary = {}
var last_replay_path: String = ""
var assets_directory: String = ""
var user_directory: String = ""
var setup_mode: String = "ai"
var network_active: bool = false
var network_state: Dictionary = {}
var _bridge: RefCounted

func initialize(assets: String, user_directory: String) -> bool:
	assets_directory = assets
	self.user_directory = user_directory
	if not ClassDB.class_exists(&"WizardBridge"):
		error = "原生模块尚未加载，请构建对应版本后重试。"
		return false
	_bridge = ClassDB.instantiate(&"WizardBridge") as RefCounted
	var response: Dictionary = _bridge.call(&"initialize", assets, user_directory)
	if not bool(response.get("ok", false)):
		error = str(response.get("error", "内容读取失败"))
		return false
	var data: Dictionary = response.get("data", {})
	rules_version = str(data.get("rulesVersion", ""))
	settings = data.get("settings", {}).duplicate(true)
	# The native defaults also serve the legacy client. Only Godot first-run data changes.
	if not FileAccess.file_exists(user_directory.path_join("settings.json")):
		var initial: Dictionary = settings.duplicate(true)
		initial["width"] = FrontendDisplay.DESIGN_SIZE.x
		initial["height"] = FrontendDisplay.DESIGN_SIZE.y
		if not apply_settings(initial):
			return false
	response = _bridge.call(&"library")
	if not bool(response.get("ok", false)):
		error = str(response.get("error", "图鉴读取失败"))
		return false
	data = response.get("data", {})
	_ingest_library(data)
	return true

func _ingest_library(data: Dictionary) -> void:
	cards.clear()
	definitions.clear()
	var rows: Array = data.get("cards", [])
	for definition: Dictionary in rows:
		cards.append(CardPresentation.from_definition(definition))
		definitions[str(definition["id"])] = definition.duplicate(true)
	drafts.assign(data.get("drafts", []))
	presets.assign(data.get("presets", []))
	playable.assign(data.get("playable", data.get("presets", [])))
	writable = bool(data.get("writable", false))
	notice = str(data.get("notice", ""))

func _request(method: StringName, arguments: Array = []) -> Dictionary:
	if _bridge == null:
		error = "卡组服务不可用"
		return {"ok": false, "error": error}
	if not _bridge.has_method(method):
		error = "请重新启动游戏，以加载新版卡组模块。"
		return {"ok": false, "error": error}
	var response: Dictionary = _bridge.callv(method, arguments)
	error = "" if bool(response.get("ok", false)) else str(response.get("error", "操作失败"))
	return response

func refresh_library() -> bool:
	var response: Dictionary = _request(&"library")
	if not bool(response.get("ok", false)):
		return false
	_ingest_library(response.get("data", {}))
	return true

func create_draft(preset: String = "") -> bool:
	var response: Dictionary = _request(&"create_construction_draft", [preset])
	if not bool(response.get("ok", false)):
		return false
	editing_draft = response.get("data", {}).duplicate(true)
	saved_draft = {}
	return true

func open_draft(id: String) -> bool:
	if _bridge == null or not _bridge.has_method(&"create_construction_draft"):
		error = "请重新启动游戏，以加载新版卡组模块。"
		return false
	for draft: Dictionary in drafts:
		if str(draft["id"]) != id:
			continue
		editing_draft = draft.duplicate(true)
		editing_draft.erase("problems")
		editing_draft.erase("matchCompatible")
		saved_draft = editing_draft.duplicate(true)
		# ``balance`` was the old six-point formation ID. The release keeps the
		# reviewed mechanics under ``hexagram`` and uses ``small-hexagram`` as the
		# only level-1 initial formation, so migrate legacy drafts in memory.
		if str(editing_draft.get("baseFormation", "")) == "balance":
			editing_draft["baseFormation"] = "small-hexagram"
		if int(editing_draft.get("format", 1)) == 1:
			editing_draft["format"] = 2
			var side: Dictionary = {}
			var main: Dictionary = editing_draft["cards"]
			for key: String in main.keys():
				var target: String = "hexagram" if key == "balance" else key
				if key == "balance" or int(definitions.get(key, {}).get("type", -1)) == 3:
					side[target] = int(side.get(target, 0)) + int(main[key])
					main.erase(key)
			var initial: String = str(editing_draft["baseFormation"])
			if definitions.has(initial) and not side.has(initial):
				side[initial] = 1
			editing_draft["sideCards"] = side
		return true
	error = "卡组已不存在，请刷新列表"
	return false

func draft_dirty() -> bool:
	return saved_draft.is_empty() or editing_draft != saved_draft

func draft_problems() -> Array[String]:
	var response: Dictionary = _request(&"validate_draft", [editing_draft])
	var result: Array[String] = []
	if bool(response.get("ok", false)):
		result.assign(response.get("data", {}).get("problems", []))
	else:
		result.append(error)
	return result

func save_draft() -> bool:
	if _bridge == null or not _bridge.has_method(&"create_construction_draft"):
		error = "请重新启动游戏，以加载新版卡组模块。"
		return false
	var response: Dictionary = _request(&"save_draft", [editing_draft])
	if not bool(response.get("ok", false)):
		return false
	saved_draft = editing_draft.duplicate(true)
	_ingest_library(response.get("data", {}))
	return true

func erase_draft(id: String) -> bool:
	var response: Dictionary = _request(&"erase_draft", [id])
	if not bool(response.get("ok", false)):
		return false
	_ingest_library(response.get("data", {}))
	return true

func query_cards(query: Dictionary) -> Array[String]:
	var response: Dictionary = _request(&"query_cards", [query])
	var result: Array[String] = []
	if bool(response.get("ok", false)):
		result.assign(response.get("data", {}).get("ids", []))
	return result

func card_by_id(id: String) -> CardPresentation:
	if definitions.has(id):
		return CardPresentation.from_definition(definitions[id])
	var missing: CardPresentation = CardPresentation.new()
	missing.instance_id = id
	missing.title = "失效卡牌"
	missing.text = "当前卡池已无此卡：" + id + "。右键移除后可修复卡组。"
	return missing

func named_copies(id: String, section: String = "cards") -> int:
	var name: String = str(definitions.get(id, {}).get("name", id))
	var count: int = 0
	var membership: Dictionary = editing_draft.get(section, {})
	for key: String in membership.keys():
		if str(definitions.get(key, {}).get("name", key)) == name:
			count += int(membership[key])
	return count

func apply_settings(next: Dictionary) -> bool:
	if _bridge == null:
		error = "设置服务不可用"
		return false
	var response: Dictionary = _bridge.call(&"apply_settings", next)
	if not bool(response.get("ok", false)):
		error = str(response.get("error", "保存失败，请重试"))
		return false
	settings = next.duplicate(true)
	error = ""
	return true

func start_match(options: Dictionary) -> bool:
	var response: Dictionary = _request(&"start_match", [options])
	if not bool(response.get("ok", false)):
		return false
	match_mode = str(options.get("mode", "hotseat"))
	match_viewer = 0
	match_snapshot.clear()
	match_active = true
	last_replay_path = ""
	return true

func snapshot(viewer: int = -1) -> Dictionary:
	var response: Dictionary = _request(&"snapshot", [match_viewer if viewer < 0 else viewer])
	if not bool(response.get("ok", false)):
		return {}
	match_snapshot = (response.get("data", {}) as Dictionary).duplicate(true)
	return match_snapshot

func match_request(method: StringName, arguments: Array = []) -> Dictionary:
	return _request(method, arguments)

func save_replay() -> bool:
	var response: Dictionary = _request(&"save_replay")
	if not bool(response.get("ok", false)):
		return false
	last_replay_path = str((response.get("data", {}) as Dictionary).get("path", ""))
	return true

func leave_match() -> bool:
	if not match_active:
		return true
	var response: Dictionary = _request(&"leave_match", [match_viewer])
	if not bool(response.get("ok", false)):
		return false
	match_active = false
	match_snapshot.clear()
	return true

func network_available() -> bool:
	return _bridge != null and _bridge.has_method(&"network_request") and _bridge.has_method(&"poll_network")

func network_request(request: Dictionary) -> Dictionary:
	var response: Dictionary = _request(&"network_request", [request])
	if bool(response.get("ok", false)):
		if str(request.get("operation", "")) in ["host", "join", "reconnect"]:
			network_active = true
		elif str(request.get("operation", "")) == "leave":
			network_active = false
			match_active = false
			match_snapshot.clear()
	return response

func poll_network() -> Dictionary:
	var response: Dictionary = _request(&"poll_network")
	if not bool(response.get("ok", false)):
		if str(response.get("error", "")) == "当前没有联机房间":
			network_active = false
			match_active = false
			match_snapshot.clear()
			network_state = {"exists": false, "notice": "房间已关闭"}
			return network_state
		return {}
	network_state = response.get("data", {})
	network_active = bool(network_state.get("exists", network_state.has("room")))
	if network_active and bool(network_state.get("started", false)):
		match_active = true
		match_mode = "network"
		match_viewer = int(network_state.get("viewer", 0))
	elif not network_active and match_mode == "network":
		match_active = false
		match_snapshot.clear()
	return network_state

func release() -> void:
	if _bridge != null:
		_bridge.call(&"release_session")
		_bridge = null
