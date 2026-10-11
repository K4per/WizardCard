class_name WizardFrontend
extends Control

signal page_changed(page: StringName)

const PAGES: Dictionary[StringName, PackedScene] = {
	&"menu": preload("res://pages/menu_page.tscn"),
	&"library": preload("res://pages/library_page.tscn"),
	&"settings": preload("res://pages/settings_page.tscn"),
	&"field": preload("res://pages/field_page.tscn"),
	&"decks": preload("res://pages/decks_page.tscn"),
	&"deck_editor": preload("res://pages/deck_editor_page.tscn"),
	&"setup": preload("res://pages/setup_page.tscn"),
	&"match": preload("res://pages/match_page.tscn"),
	&"replay": preload("res://pages/replay_page.tscn"),
	&"rooms": preload("res://pages/rooms_page.tscn"),
}

var service: FrontendService = FrontendService.new()
var current_page: StringName = &""
var page: Control
@onready var _host: Control = %PageHost
@onready var _background: TextureRect = %Background
@onready var _dim: ColorRect = $Dim
var _network_timer: Timer

func _ready() -> void:
	get_tree().auto_accept_quit = false
	get_window().close_requested.connect(_on_close_requested)
	theme = ArtBank.make_theme()
	var assets: String = ProjectSettings.globalize_path("res://").path_join("../assets").simplify_path()
	# Exported PCK builds can report different feature flags across Godot
	# templates. Prefer the portable resource directory whenever it exists so a
	# release launched from any working directory never falls back to ../assets.
	var packaged_assets: String = OS.get_executable_path().get_base_dir().path_join("assets")
	if FileAccess.file_exists(packaged_assets.path_join("cards.json")):
		assets = packaged_assets
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
	_network_timer = Timer.new()
	_network_timer.wait_time = 0.25
	_network_timer.timeout.connect(_poll_network)
	add_child(_network_timer)
	_network_timer.start()

func navigate(destination: StringName) -> void:
	if not PAGES.has(destination):
		return
	if is_instance_valid(page):
		_host.remove_child(page)
		page.queue_free()
	current_page = destination
	ArtBank.trim_illustrations()
	var background_id: String = "c2d.background.realm"
	if destination == &"menu":
		background_id = "c2d.v2.menu.background"
	elif destination in [&"decks", &"deck_editor"]:
		background_id = "c2d.v2.deck.background"
	_background.texture = ArtBank.texture(background_id)
	_dim.visible = destination != &"menu"
	page = PAGES[destination].instantiate() as Control
	_host.add_child(page)
	page.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	page.connect(&"navigation_requested", navigate)
	page.call(&"configure", service)
	page_changed.emit(destination)

func _unhandled_key_input(event: InputEvent) -> void:
	if event.is_action_pressed(&"ui_cancel") and current_page != &"menu":
		if page is DeckEditorPage:
			(page as DeckEditorPage).request_leave()
		elif page is MatchPage:
			(page as MatchPage).request_leave()
		else:
			navigate(&"match" if service.match_active and current_page in [&"settings", &"replay"] else &"menu")
		get_viewport().set_input_as_handled()

func _on_close_requested() -> void:
	if page is DeckEditorPage:
		(page as DeckEditorPage).request_leave(true)
	elif page is MatchPage:
		(page as MatchPage).request_leave(true)
	elif service.match_active:
		if service.match_mode == "network":
			navigate(&"match")
			(page as MatchPage).request_leave(true)
		elif service.save_replay() and service.leave_match():
			get_tree().quit()
	elif service.network_active:
		if bool(service.network_request({"operation": "leave", "force": true}).get("ok", false)):
			get_tree().quit()
	else:
		get_tree().quit()

func _poll_network() -> void:
	if not service.network_active:
		return
	var state: Dictionary = service.poll_network()
	if state.is_empty():
		return
	if current_page == &"rooms" and bool(state.get("started", false)):
		navigate(&"match")
	elif page is RoomsPage:
		(page as RoomsPage).network_updated(state)
	elif page is MatchPage:
		(page as MatchPage).network_updated(state)

func _exit_tree() -> void:
	service.release()
