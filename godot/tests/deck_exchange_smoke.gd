extends SceneTree

var _failures: int = 0

func _initialize() -> void:
	_run.call_deferred()

func _expect(condition: bool, message: String) -> void:
	if not condition:
		_failures += 1
		push_error(message)

func _run() -> void:
	var app: WizardFrontend = load("res://app/main.tscn").instantiate() as WizardFrontend
	root.add_child(app)
	await process_frame
	var service: FrontendService = app.service
	_expect(service.error.is_empty(), service.error)
	if not service.error.is_empty():
		quit(2)
		return
	_expect(service.create_draft(str(service.presets[0]["id"])), "A legal preset opens")
	var original: Dictionary = service.editing_draft.duplicate(true)
	var path: String = service.user_directory.path_join("portable-deck.json")
	_expect(service.export_deck(path), "Legal deck exports: " + service.error)
	_expect(service.import_deck(path), "Exported deck imports: " + service.error)
	_expect(service.editing_draft["cards"] == original["cards"] and service.editing_draft["sideCards"] == original["sideCards"], "Round trip preserves both sections")
	_expect(service.editing_draft["id"] != original["id"], "Import has a new local identity")
	var valid: Dictionary = DeckExchange.encode(original)
	var malformed: Array[Dictionary] = []
	var wrong_version: Dictionary = valid.duplicate(true)
	wrong_version["version"] = 99
	malformed.append(wrong_version)
	var duplicate: Dictionary = valid.duplicate(true)
	duplicate["main"].append(duplicate["main"][0].duplicate())
	malformed.append(duplicate)
	var missing: Dictionary = valid.duplicate(true)
	missing["main"][0]["id"] = "missing-content"
	malformed.append(missing)
	var illegal_section: Dictionary = valid.duplicate(true)
	illegal_section["main"][0]["id"] = illegal_section["side"][0]["id"]
	malformed.append(illegal_section)
	var wrong_total: Dictionary = valid.duplicate(true)
	wrong_total["main"].pop_back()
	malformed.append(wrong_total)
	var excessive: Dictionary = valid.duplicate(true)
	excessive["main"][0]["count"] = 4
	malformed.append(excessive)
	var current: Dictionary = service.editing_draft.duplicate(true)
	for document: Dictionary in malformed:
		var file: FileAccess = FileAccess.open(path, FileAccess.WRITE)
		file.store_string(JSON.stringify(document))
		file.close()
		_expect(not service.import_deck(path) and not service.error.is_empty(), "Invalid deck reports a reason")
		_expect(service.editing_draft == current, "Failed import preserves current draft")
	print("DECK_EXCHANGE_SMOKE: ", _failures, " failures")
	app.queue_free()
	await process_frame
	quit(0 if _failures == 0 else 1)
