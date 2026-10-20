extends SceneTree

var _failures: Array[String] = []
var _checks: int = 0
var _typography: Node
var _capture_path: String = ""

func _initialize() -> void:
	_run.call_deferred()

func _check(condition: bool, message: String) -> void:
	_checks += 1
	if not condition:
		_failures.append(message)
		push_error(message)

func _settle() -> void:
	await process_frame
	await process_frame

func _run() -> void:
	_typography = root.get_node("GlobalTypography")
	var arguments: PackedStringArray = OS.get_cmdline_user_args()
	for index: int in range(arguments.size() - 1):
		if arguments[index] == "--capture-output":
			_capture_path = arguments[index + 1]
			DirAccess.make_dir_recursive_absolute(_capture_path)
	var holder: Control = Control.new()
	holder.theme = DeckTheme.create()
	root.add_child(holder)
	var inherited: Label = Label.new()
	inherited.text = "继承字号"
	holder.add_child(inherited)
	var explicit: Label = UiFactory.label("显式字号", 20)
	holder.add_child(explicit)
	var body: RichTextLabel = RichTextLabel.new()
	body.add_theme_font_size_override(&"normal_font_size", 17)
	holder.add_child(body)
	var options: OptionButton = OptionButton.new()
	options.add_item("弹出菜单")
	holder.add_child(options)
	await _settle()
	for scale: int in [120, 90, 110, 100, 120, 100]:
		_typography.call(&"set_scale", scale)
		await _settle()
		_check(inherited.get_theme_font_size(&"font_size") == roundi(15 * scale / 100.0), "Inherited font follows global scale without compounding")
		_check(explicit.get_theme_font_size(&"font_size") == roundi(20 * scale / 100.0), "Local font follows global scale without compounding")
		_check(body.get_theme_font_size(&"normal_font_size") == roundi(17 * scale / 100.0), "Rich text follows global scale")
		_check(options.get_popup().get_theme_font_size(&"font_size") == inherited.get_theme_font_size(&"font_size"), "Popup text follows the inherited scaled theme")
	_typography.call(&"set_scale", 120)
	var dynamic: Label = UiFactory.label("动态控件", 14)
	holder.add_child(dynamic)
	await _settle()
	_check(dynamic.get_theme_font_size(&"font_size") == 17, "Late controls pick up the current scale")
	dynamic.add_theme_font_size_override(&"font_size", 22)
	await _settle()
	_check(dynamic.get_theme_font_size(&"font_size") == 26, "Later layout font changes preserve scaling")
	Typography.override_size(dynamic, &"font_size", 26)
	_check(dynamic.get_theme_font_size(&"font_size") == 31, "Responsive sizes are unambiguous when matching a previous scaled value")
	holder.queue_free()
	_typography.call(&"set_scale", 100)
	await _settle()
	var app: WizardFrontend = load("res://app/main.tscn").instantiate() as WizardFrontend
	root.add_child(app)
	await _settle()
	_check(app.service.error.is_empty(), "Native content loads: " + app.service.error)
	if not app.service.error.is_empty():
		quit(2)
		return
	app.service.create_draft()
	for dimensions: Vector2i in [Vector2i(1280, 720), Vector2i(1600, 900), Vector2i(1600, 1000), Vector2i(1920, 1080), Vector2i(2560, 1440), Vector2i(1440, 900)]:
		root.size = dimensions
		for scale: int in [90, 100, 110, 120]:
			_typography.call(&"set_scale", scale)
			for destination: StringName in [&"menu", &"settings", &"deck_editor", &"library", &"field", &"replay"]:
				app.navigate(destination)
				await _settle()
				var bounds: Rect2 = Rect2(Vector2.ZERO, Vector2(dimensions))
				_check(bounds.encloses(app.page.get_global_rect()), "Page remains inside viewport: %s %s %d" % [destination, dimensions, scale])
				if destination == &"settings":
					var settings: SettingsPage = app.page as SettingsPage
					_check(settings._font_scale.item_count == 4, "Font setting exposes four documented sizes")
					_check(bounds.encloses(settings._font_scale.get_global_rect()), "Font selector remains reachable")
				elif destination == &"menu":
					var menu: MenuPage = app.page as MenuPage
					_check(bounds.encloses(menu._logo.get_global_rect()) and menu._logo.get_global_rect().end.x < menu._column.get_global_rect().position.x, "Logo remains visible to the left of all seven primary actions")
					for button: Button in menu._buttons:
						var font: Font = button.get_theme_font(&"font")
						var font_size: int = button.get_theme_font_size(&"font_size")
						var padding: Vector2 = button.get_theme_stylebox(&"normal").get_minimum_size()
						var text_size: Vector2 = font.get_string_size(button.text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size)
						var outline: int = button.get_theme_constant(&"outline_size") * 2
						_check(bounds.encloses(button.get_global_rect()), "Menu actions remain inside viewport")
						_check(text_size.x + outline <= button.size.x - padding.x and font.get_height(font_size) + outline <= button.size.y - padding.y, "Menu text fits at every font size and viewport")
				elif destination == &"library":
					var library: LibraryPage = app.page as LibraryPage
					library._show_detail(app.service.cards[0])
					await _settle()
					_check(bounds.encloses(library._detail.get_global_rect()), "Library details remain inside viewport")
					_check(library._detail._body.text.contains("编号："), "Details expose the stable card number")
				elif destination == &"deck_editor":
					var editor: DeckEditorPage = app.page as DeckEditorPage
					_check(bounds.encloses(editor._back.get_global_rect()) and bounds.encloses(editor._save.get_global_rect()), "Deck editing actions remain reachable")
					editor._show_problems()
					await _settle()
					_check(bounds.encloses(editor._dialog._panel.get_global_rect()), "Large-font modal remains inside viewport")
					editor._dialog.hide()
					_check(editor._pool.rows == 1 or editor._pool.size.y / editor._pool.rows >= 115.0, "Pool adapts pagination to keep useful card height")
				if not _capture_path.is_empty() and dimensions == Vector2i(1280, 720) and scale == 120:
					await RenderingServer.frame_post_draw
					var picture: Image = root.get_texture().get_image()
					picture.save_png(_capture_path.path_join("%s-1280x720-120.png" % destination))
	_typography.call(&"set_scale", 100)
	app.queue_free()
	await _settle()
	print(JSON.stringify({"checks": _checks, "failures": _failures}))
	quit(0 if _failures.is_empty() else 1)
