class_name FrontendDisplay
extends RefCounted

static func apply(settings: Dictionary) -> void:
	if DisplayServer.get_name() == "headless":
		return
	var mode: int = int(settings.get("windowMode", 0))
	DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_BORDERLESS, mode == 1)
	DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_FULLSCREEN if mode == 2 else DisplayServer.WINDOW_MODE_WINDOWED)
	if mode != 2:
		var dimensions: Vector2i = Vector2i(maxi(1280, int(settings.get("width", 1600))), maxi(720, int(settings.get("height", 1000))))
		DisplayServer.window_set_size(dimensions)
