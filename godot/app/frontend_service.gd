class_name FrontendService
extends RefCounted

var error: String = ""
var cards: Array[CardPresentation] = []
var settings: Dictionary = {}
var rules_version: String = ""
var _bridge: RefCounted

func initialize(assets: String, user_directory: String) -> bool:
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
	response = _bridge.call(&"library")
	if not bool(response.get("ok", false)):
		error = str(response.get("error", "图鉴读取失败"))
		return false
	data = response.get("data", {})
	cards.clear()
	var definitions: Array = data.get("cards", [])
	for definition: Dictionary in definitions:
		cards.append(CardPresentation.from_definition(definition))
	return true

func apply_settings(next: Dictionary) -> bool:
	if _bridge == null:
		error = "设置服务不可用"
		return false
	var response: Dictionary = _bridge.call(&"apply_settings", next)
	if not bool(response.get("ok", false)):
		error = str(response.get("error", "保存失败，请重试"))
		return false
	settings = next.duplicate(true)
	return true

func release() -> void:
	if _bridge != null:
		_bridge.call(&"release_session")
		_bridge = null
