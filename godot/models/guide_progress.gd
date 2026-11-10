class_name GuideProgress
extends RefCounted

static func completed(directory: String) -> Array:
	var path: String = directory.path_join("guide-progress.json")
	if not FileAccess.file_exists(path):
		return []
	var value: Variant = JSON.parse_string(FileAccess.get_file_as_string(path))
	if value is Dictionary and value.get("completed") is Array:
		return (value["completed"] as Array).map(func(chapter: Variant) -> int: return int(chapter))
	return []

static func mark(directory: String, chapter: int) -> bool:
	var chapters: Array = completed(directory)
	if chapters.has(chapter):
		return true
	chapters.append(chapter)
	var path: String = directory.path_join("guide-progress.json")
	var file: FileAccess = FileAccess.open(path + ".tmp", FileAccess.WRITE)
	if file == null:
		return false
	file.store_string(JSON.stringify({"version": "2.6.3", "completed": chapters}))
	file.flush()
	var written: bool = file.get_error() == OK
	file.close()
	return written and DirAccess.rename_absolute(path + ".tmp", path) == OK
