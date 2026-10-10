class_name WizardFrontend
extends Control

signal page_changed(page: StringName)

const PAGES: Dictionary[StringName, PackedScene] = {
	&"menu": preload("res://pages/menu_page.tscn"),
	&"library": preload("res://pages/library_page.tscn"),
	&"settings": preload("res://pages/settings_page.tscn"),
	&"field": preload("res://pages/field_page.tscn"),
}

var service: FrontendService = FrontendService.new()
var current_page: StringName = &""
var page: Control
@onready var _host: Control = %PageHost
@onready var _background: TextureRect = %Background

func _ready() -> void:
	theme = ArtBank.make_theme()
	_background.texture = ArtBank.texture("c2d.background.realm")
	var assets: String = ProjectSettings.globalize_path("res://").path_join("../assets").simplify_path()
	var user_directory: String = ProjectSettings.globalize_path("user://alpha-v2-frontend")
	var arguments: PackedStringArray = OS.get_cmdline_user_args()
	for index: int in range(arguments.size() - 1):
		if arguments[index] == "--assets":
			assets = arguments[index + 1]
		elif arguments[index] == "--user-data":
			user_directory = arguments[index + 1]
	get_window().min_size = Vector2i(1280, 720)
	if service.initialize(assets, user_directory):
		FrontendDisplay.apply(service.settings)
	navigate(&"menu")

func navigate(destination: StringName) -> void:
	if not PAGES.has(destination):
		return
	if is_instance_valid(page):
		_host.remove_child(page)
		page.queue_free()
	current_page = destination
	page = PAGES[destination].instantiate() as Control
	_host.add_child(page)
	page.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	page.call(&"configure", service)
	page.connect(&"navigation_requested", navigate)
	page_changed.emit(destination)

func _unhandled_key_input(event: InputEvent) -> void:
	if event.is_action_pressed(&"ui_cancel") and current_page != &"menu":
		navigate(&"menu")
		get_viewport().set_input_as_handled()

func _exit_tree() -> void:
	service.release()
