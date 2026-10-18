class_name MenuPage
extends Control

signal navigation_requested(page: StringName)

var _service: FrontendService
var _status: Label
var _column: VBoxContainer
var _logo: TextureRect
var _buttons: Array[MenuArtButton] = []

func _ready() -> void:
	_logo = %Logo
	_logo.texture = ArtBank.texture("c2d.v2.menu.logo")
	_column = %Buttons
	_status = %Status
	_buttons.assign([%SinglePlayer, %Multiplayer, %Decks, %Settings, %Quit, %Replay, %Library])
	_buttons[0].pressed.connect(_single_player)
	_buttons[1].pressed.connect(navigation_requested.emit.bind(&"rooms"))
	_buttons[2].pressed.connect(navigation_requested.emit.bind(&"decks"))
	_buttons[3].pressed.connect(navigation_requested.emit.bind(&"settings"))
	_buttons[4].pressed.connect(get_tree().quit)
	_buttons[5].pressed.connect(navigation_requested.emit.bind(&"replay"))
	_buttons[6].pressed.connect(navigation_requested.emit.bind(&"library"))

func configure(service: FrontendService) -> void:
	_service = service
	_status.text = "" if service.error.is_empty() else service.error
	_status.visible = not service.error.is_empty()

func _single_player() -> void:
	_service.setup_mode = "ai"
	navigation_requested.emit(&"setup")
