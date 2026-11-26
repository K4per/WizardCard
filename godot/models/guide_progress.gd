class_name GuideProgress
extends RefCounted

static func read(directory: String) -> Dictionary:
	var state := {"schemaVersion": 2, "version": "2.6.6", "askedIntro": false, "isVeteran": false,
		"completed": [], "skipped": [], "deckSkipCount": 0}
	var path := directory.path_join("guide-progress.json")
	if not FileAccess.file_exists(path):
		return state
	var value: Variant = JSON.parse_string(FileAccess.get_file_as_string(path))
	if not value is Dictionary:
		return state
	if not value.has("schemaVersion"):
		state.askedIntro = true
		state.isVeteran = true
		_write(directory, state)
	elif value.schemaVersion == 2:
		for key: String in ["completed", "skipped"]:
			if value.get(key) is Array:
				for id: Variant in value[key]:
					if id is String and not id.is_empty() and not state[key].has(id):
						state[key].append(id)
		state.askedIntro = value.get("askedIntro") == true
		state.isVeteran = value.get("isVeteran") == true
		state.deckSkipCount = clampi(int(value.get("deckSkipCount", 0)), 0, 2)
	return state

static func _write(directory: String, state: Dictionary) -> bool:
	var path := directory.path_join("guide-progress.json")
	var file := FileAccess.open(path + ".tmp", FileAccess.WRITE)
	if file == null:
		return false
	file.store_string(JSON.stringify(state))
	file.flush()
	var written := file.get_error() == OK
	file.close()
	return written and DirAccess.rename_absolute(path + ".tmp", path) == OK

static func answer(directory: String, veteran: bool) -> bool:
	var state := read(directory)
	state.askedIntro = true
	state.isVeteran = veteran
	return _write(directory, state)

static func completed(directory: String) -> Array:
	return read(directory).completed

static func mark(directory: String, lesson: Variant) -> bool:
	var id := str(lesson) if lesson is String else "legacy-chapter-%d" % int(lesson)
	var state := read(directory)
	if state.completed.has(id):
		return true
	state.completed.append(id)
	state.skipped.erase(id)
	return _write(directory, state)

static func skip(directory: String, id: String, editor: bool) -> bool:
	var state := read(directory)
	if not state.completed.has(id) and not state.skipped.has(id):
		state.skipped.append(id)
	if editor:
		state.deckSkipCount = mini(2, int(state.deckSkipCount) + 1)
	return _write(directory, state)
