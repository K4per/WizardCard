extends SceneTree

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var expected: String = OS.get_environment("WIZARD_EXPECTED_USER_DATA")
	var app: WizardFrontend = load("res://app/main.tscn").instantiate() as WizardFrontend
	root.add_child(app)
	await process_frame
	if expected.is_empty() or app.service.user_directory != expected or not app.service.error.is_empty():
		push_error("Linux user directory mismatch: %s, expected %s; %s" % [app.service.user_directory, expected, app.service.error])
		quit(1)
		return
	var settings: Dictionary = app.service.settings.duplicate(true)
	settings["fontScale"] = 110
	if not app.service.apply_settings(settings) or not FileAccess.file_exists(expected.path_join("settings.json")):
		push_error("Linux settings must be saved under the XDG directory")
		quit(1)
		return
	app.queue_free()
	await process_frame
	app = load("res://app/main.tscn").instantiate() as WizardFrontend
	root.add_child(app)
	await process_frame
	var passed: bool = int(app.service.settings.get("fontScale", 0)) == 110
	app.queue_free()
	await process_frame
	print("Linux user data smoke: ", "PASS" if passed else "FAIL", "; directory=", expected)
	quit(0 if passed else 1)
