@tool
class_name GameText
extends RefCounted

var locale: String = "zh_CN"
var card_names: Dictionary = {}
var diagnostic: Callable
var last_error: String = ""
var directory: String = ""
var _default_locale: String
var _fallbacks: Dictionary
var _messages: Dictionary
var _token := RegEx.create_from_string("\\{\\{|\\}\\}|\\{([a-zA-Z_][a-zA-Z0-9_]*)(?::([a-zA-Z0-9_.-]+))?\\}")
var _key := RegEx.create_from_string("^[a-z0-9_]+(?:\\.[a-z0-9_]+){2,}$")
var _locale_name := RegEx.create_from_string("^[a-zA-Z0-9_-]+$")
var _file_name := RegEx.create_from_string("^[a-z0-9_]+\\.json$")
var _json_string := RegEx.create_from_string('"[^"\\\\]*(?:\\\\.[^"\\\\]*)*"')

# Models, @tool previews and --script test entry points can render before autoloads enter the tree.
static var _shared: GameText

static func shared() -> GameText:
	if _shared == null:
		_shared = GameText.new()
		_shared._load_default()
	return _shared

static func release_catalog() -> void:
	_shared = null

static func label(key: String, params: Dictionary = {}) -> String:
	return shared().t(key, params)

# New snapshots carry a term key beside the legacy rendered caption.
static func term(fields: Dictionary, name: String, fallback: String = "") -> String:
	var key: String = str(fields.get(name + "Key", ""))
	return label(key) if not key.is_empty() else str(fields.get(name, fallback))

static func field(fields: Dictionary, name: String, fallback: String = "") -> String:
	var ref: Variant = fields.get(name + "Ref")
	return shared().render(ref) if ref is Dictionary and not str(ref.get("key", "")).is_empty() else str(fields.get(name, fallback))

func _load_default() -> void:
	var packaged := OS.get_executable_path().get_base_dir().path_join("assets/text")
	var path := packaged if FileAccess.file_exists(packaged.path_join("manifest.json")) else "res://data/text"
	if not load_catalog(path):
		push_error(last_error)

func _fail(message: String) -> bool:
	last_error = message
	return false

func _read(path: String) -> Variant:
	var file := FileAccess.open(path, FileAccess.READ)
	if file == null:
		_fail("Cannot open " + path)
		return null
	var source := file.get_as_text()
	var parser := JSON.new()
	if parser.parse(source) != OK or not parser.data is Dictionary:
		_fail("Invalid JSON object in " + path)
		return null
	# Godot's JSON parser keeps the last duplicate. Compare decoded keys within each object.
	var seen: Array[Dictionary] = []
	var cursor := 0
	for item: RegExMatch in _json_string.search_all(source):
		for ch: String in source.substr(cursor, item.get_start() - cursor):
			if ch == "{":
				seen.append({})
			elif ch == "}":
				seen.pop_back()
		cursor = item.get_end()
		var suffix := cursor
		while suffix < source.length() and source.unicode_at(suffix) in [9, 10, 13, 32]:
			suffix += 1
		if suffix < source.length() and source[suffix] == ":":
			var name: String = JSON.parse_string(item.get_string())
			if seen.back().has(name):
				_fail("Duplicate key %s in %s" % [name, path])
				return null
			seen.back()[name] = true
	return parser.data

func load_catalog(source_directory: String) -> bool:
	last_error = ""
	var manifest: Variant = _read(source_directory.path_join("manifest.json"))
	if manifest == null:
		return false
	var locales: Variant = manifest.get("locales")
	var files: Variant = manifest.get("files")
	var fallback: Variant = manifest.get("fallbacks")
	var base: Variant = manifest.get("defaultLocale")
	if manifest.size() != 4 or not locales is Array or not files is Array or files.is_empty() or not fallback is Dictionary or not base is String or base not in locales:
		return _fail("Invalid text manifest in " + source_directory)
	var messages: Dictionary = {}
	var arguments: Dictionary = {}
	var term_refs: Array[String] = []
	for lang: Variant in locales:
		if not lang is String or not _locale_name.search(lang) or messages.has(lang):
			return _fail("Invalid or repeated locale " + str(lang))
		var entries: Dictionary = {}
		var params: Dictionary = {}
		var sources: Dictionary = {}
		for file: Variant in files:
			if not file is String or not _file_name.search(file) or sources.has(file):
				return _fail("Invalid or repeated catalogue filename " + str(file))
			sources[file] = true
			var doc: Variant = _read(source_directory.path_join(lang).path_join(file))
			if doc == null:
				return false
			for key: String in doc:
				if not _key.search(key) or entries.has(key) or not doc[key] is String:
					return _fail("Invalid or duplicate text entry " + key)
				var template: String = doc[key]
				var remaining := _token.sub(template, "", true)
				if remaining.contains("{") or remaining.contains("}"):
					return _fail("Invalid placeholder in " + key)
				var names: Dictionary = {}
				for match_: RegExMatch in _token.search_all(template):
					var name := match_.get_string(1)
					var ref := match_.get_string(2)
					if not name.is_empty() and ref.is_empty():
						names[name] = true
					elif not ref.is_empty():
						if name == "card":
							if not _locale_name.search(ref):
								return _fail("Invalid card reference in " + key)
						elif not ref.begins_with("terms.") or not _key.search(ref):
							return _fail("Invalid term reference in " + key)
						else:
							term_refs.append(ref)
				params[key] = names
				entries[key] = template
		messages[lang] = entries
		arguments[lang] = params
	for lang: Variant in fallback:
		if lang not in locales or fallback[lang] not in locales or lang == base:
			return _fail("Invalid fallback for " + str(lang))
	for ref: String in term_refs:
		if not arguments[base].has(ref) or not arguments[base][ref].is_empty():
			return _fail("Inline reference requires a term without parameters: " + ref)
	for lang: String in locales:
		var visited: Array[String] = []
		var current := lang
		while current != base:
			if current in visited:
				return _fail("Cyclic fallback for " + lang)
			visited.append(current)
			current = fallback.get(current, base)
		for key: String in messages[lang]:
			if not arguments[base].has(key) or arguments[base][key] != arguments[lang][key]:
				return _fail("Translation arguments differ for " + key)
	directory = ProjectSettings.globalize_path(source_directory).simplify_path()
	_default_locale = base
	_fallbacks = fallback
	_messages = messages
	return true

func _warn(message: String) -> void:
	if diagnostic.is_valid():
		diagnostic.call(message)
	else:
		push_warning(message)

func has_key(key: String) -> bool:
	return _messages.get(_default_locale, {}).has(key)

func t(key: String, params: Dictionary = {}) -> String:
	return _render(key, params, 0)

func render(reference: Dictionary) -> String:
	return t(str(reference.get("key", "")), reference.get("args", {}))

func _card(id: String) -> String:
	if not card_names.has(id):
		_warn("Unknown card " + id)
	return str(card_names.get(id, id))

func _render(key: String, params: Dictionary, depth: int) -> String:
	if depth >= 32:
		_warn("Recursive text reference " + key)
		return key
	var lang := locale if _messages.has(locale) else _default_locale
	while lang != _default_locale and not _messages[lang].has(key):
		lang = _fallbacks.get(lang, _default_locale)
	if not _messages.get(lang, {}).has(key):
		_warn("Missing text key " + key)
		return key
	var template: String = _messages[lang][key]
	if params.is_empty() and not template.contains("{") and not template.contains("}"):
		return template
	var output := ""
	var cursor := 0
	var used: Dictionary = {}
	for item: RegExMatch in _token.search_all(template):
		output += template.substr(cursor, item.get_start() - cursor)
		cursor = item.get_end()
		var name := item.get_string(1)
		var ref := item.get_string(2)
		if name.is_empty():
			output += item.get_string()[0]
		elif not ref.is_empty():
			output += _card(ref) if name == "card" else _render(ref, {}, depth + 1)
		elif params.has(name):
			used[name] = true
			var value: Variant = params[name]
			if value is Dictionary and value.has("key"):
				output += _render(str(value.key), value.get("args", {}), depth + 1)
			elif value is Dictionary and value.has("card"):
				output += _card(str(value.card))
			elif value is String or value is int or value is float:
				output += str(int(value)) if value is float and value == int(value) else str(value)
			else:
				_warn("Invalid text argument %s in %s" % [name, key])
				output += item.get_string()
		else:
			_warn("Missing text argument %s in %s" % [name, key])
			output += item.get_string()
	for name: Variant in params:
		if not used.has(name):
			_warn("Unexpected text argument %s in %s" % [name, key])
	return output + template.substr(cursor)
