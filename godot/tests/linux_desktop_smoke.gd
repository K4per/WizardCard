extends SceneTree

var _failures: Array[String] = []
var _report: String = ""
var _expected_driver: String = ""
var _real_input: bool = false
var _checks: int = 0

func _initialize() -> void:
	_run.call_deferred()

func _check(condition: bool, message: String) -> void:
	_checks += 1
	if not condition:
		_failures.append(message)
		push_error(message)

func _settle() -> void:
	await create_timer(0.15).timeout

func _run() -> void:
	var arguments: PackedStringArray = OS.get_cmdline_user_args()
	for index: int in range(arguments.size() - 1):
		if arguments[index] == "--report":
			_report = arguments[index + 1]
		elif arguments[index] == "--expected-driver":
			_expected_driver = arguments[index + 1]
	_real_input = arguments.has("--real-input")
	_check(DisplayServer.get_name().to_lower() == _expected_driver, "Requested native display backend is active")
	_check(DisplayServer.get_name() != "headless", "Desktop test requires a rendered window")
	DisplayServer.window_set_title("WizardCard Linux Desktop Smoke")
	var app: WizardFrontend = load("res://app/main.tscn").instantiate() as WizardFrontend
	root.add_child(app)
	await _settle()
	_check(app.service.error.is_empty(), "Native bridge initializes: " + app.service.error)
	if not app.service.error.is_empty():
		quit(2)
		return
	var service: FrontendService = app.service
	var menu: MenuPage = app.page as MenuPage
	var font: Font = menu._buttons[3].get_theme_font(&"font")
	for character: String in ["巫", "师", "牌", "设", "置"]:
		_check(font.has_char(character.unicode_at(0)), "Bundled font contains Chinese glyph: " + character)
	menu._buttons[3].grab_focus()
	if _real_input:
		var ready: FileAccess = FileAccess.open(_report + ".ready", FileAccess.WRITE)
		ready.store_string("ready")
		ready.close()
		var deadline: int = Time.get_ticks_msec() + 15000
		while app.current_page == &"menu" and Time.get_ticks_msec() < deadline:
			await _settle()
	else:
		var key: InputEventKey = InputEventKey.new()
		key.keycode = KEY_ENTER
		key.pressed = true
		Input.parse_input_event(key)
		await _settle()
		key = InputEventKey.new()
		key.keycode = KEY_ENTER
		key.pressed = false
		Input.parse_input_event(key)
		await _settle()
	_check(app.current_page == &"library", "Keyboard input activates the focused library button")
	var window_states: Array[Dictionary] = []
	for mode: int in [1, 2, 0]:
		var settings: Dictionary = service.settings.duplicate(true)
		settings["windowMode"] = mode
		settings["width"] = 1280
		settings["height"] = 720
		settings["fontScale"] = 110
		_check(service.apply_settings(settings), "Settings save for window mode " + str(mode))
		FrontendDisplay.apply(settings)
		Typography.set_scale(110)
		await _settle()
		var expected_mode: int = DisplayServer.WINDOW_MODE_FULLSCREEN if mode == 2 else DisplayServer.WINDOW_MODE_WINDOWED
		var mode_deadline: int = Time.get_ticks_msec() + 3000
		while (DisplayServer.window_get_mode() != expected_mode or
				(mode != 2 and DisplayServer.window_get_size() != Vector2i(1280, 720))) and Time.get_ticks_msec() < mode_deadline:
			await _settle()
		_check(DisplayServer.window_get_mode() == expected_mode, "Window mode is applied")
		_check(DisplayServer.window_get_flag(DisplayServer.WINDOW_FLAG_BORDERLESS) == (mode == 1), "Borderless flag matches mode " + str(mode))
		var dimensions: Vector2i = DisplayServer.window_get_size()
		if mode != 2:
			_check(dimensions == Vector2i(1280, 720), "Requested window resolution is applied for mode " + str(mode))
		window_states.append({"requestedMode": mode, "actualMode": DisplayServer.window_get_mode(),
			"borderless": DisplayServer.window_get_flag(DisplayServer.WINDOW_FLAG_BORDERLESS),
			"windowSize": [dimensions.x, dimensions.y]})
	_check(service.create_draft(str(service.presets[0]["id"])), "Deck can be created")
	var deck_name: String = "Linux 桌面保存测试"
	service.editing_draft["name"] = deck_name
	_check(service.save_draft(), "Deck saves to the isolated user directory")
	_check(service.start_match({"mode": "hotseat", "seed": 42}), "Match starts")
	_check(service.save_replay() and FileAccess.file_exists(service.last_replay_path), "Replay is saved")
	_check(service.leave_match(), "Match exits cleanly")
	var reopened: FrontendService = FrontendService.new()
	_check(reopened.initialize(service.assets_directory, service.user_directory), "Saved data can be reopened")
	_check(int(reopened.settings.get("fontScale", 0)) == 110, "Font scale persists after reopening")
	var saved_deck: bool = false
	for draft: Dictionary in reopened.drafts:
		saved_deck = saved_deck or str(draft.get("name", "")) == deck_name
	_check(saved_deck, "Chinese deck name persists after reopening")
	reopened.release()
	await RenderingServer.frame_post_draw
	var screenshot: String = _report.get_basename() + ".png"
	_check(root.get_texture().get_image().save_png(screenshot) == OK, "Rendered desktop screenshot is saved")
	var report: Dictionary = {"displayDriver": DisplayServer.get_name(), "realInput": _real_input,
		"checks": _checks, "failures": _failures, "renderer": RenderingServer.get_video_adapter_name(),
		"windowSize": [DisplayServer.window_get_size().x, DisplayServer.window_get_size().y],
		"viewportSize": [root.size.x, root.size.y], "windowStates": window_states, "userData": service.user_directory,
		"screenshot": screenshot}
	var file: FileAccess = FileAccess.open(_report, FileAccess.WRITE)
	file.store_string(JSON.stringify(report, "\t"))
	file.close()
	print("LINUX_DESKTOP_SMOKE: ", JSON.stringify(report))
	app.queue_free()
	await process_frame
	quit(0 if _failures.is_empty() else 1)
