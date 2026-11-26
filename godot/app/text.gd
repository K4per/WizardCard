extends Node

var locale: String:
	get: return GameText.shared().locale
	set(value): GameText.shared().locale = value

var card_names: Dictionary:
	get: return GameText.shared().card_names
	set(value): GameText.shared().card_names = value

var diagnostic: Callable:
	get: return GameText.shared().diagnostic
	set(value): GameText.shared().diagnostic = value

var last_error: String:
	get: return GameText.shared().last_error

func _ready() -> void:
	GameText.shared()
	get_tree().node_added.connect(_bind_scene_text)

func _exit_tree() -> void:
	GameText.release_catalog()

func _bind_scene_text(node: Node) -> void:
	# Resolve scene properties before the child and its parent run _ready.
	if node.has_meta(&"text_keys"):
		var keys: Dictionary = node.get_meta(&"text_keys")
		for property: String in keys:
			node.set(property, t(keys[property]))

func load_catalog(directory: String) -> bool:
	return GameText.shared().load_catalog(directory)

func has_key(key: String) -> bool:
	return GameText.shared().has_key(key)

func t(key: String, params: Dictionary = {}) -> String:
	return GameText.label(key, params)

func render(reference: Dictionary) -> String:
	return GameText.shared().render(reference)
