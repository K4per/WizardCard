class_name FrontendDisplay
extends RefCounted

const DESIGN_SIZE: Vector2i = Vector2i(1920, 1080)

static func apply(settings: Dictionary) -> void:
	if DisplayServer.get_name() == "headless":
		return
	var mode: int = int(settings.get("windowMode", 0))
	DisplayServer.window_set_flag(DisplayServer.WINDOW_FLAG_BORDERLESS, mode == 1)
	DisplayServer.window_set_mode(DisplayServer.WINDOW_MODE_FULLSCREEN if mode == 2 else DisplayServer.WINDOW_MODE_WINDOWED)
	if mode != 2:
		var dimensions: Vector2i = Vector2i(maxi(1280, int(settings.get("width", DESIGN_SIZE.x))), maxi(720, int(settings.get("height", DESIGN_SIZE.y))))
		var usable: Rect2i = DisplayServer.screen_get_usable_rect(DisplayServer.window_get_current_screen())
		if usable.size.x >= 1280 and usable.size.y >= 720:
			dimensions.x = mini(dimensions.x, usable.size.x)
			dimensions.y = mini(dimensions.y, usable.size.y - (40 if mode == 0 else 0))
		DisplayServer.window_set_size(dimensions)
		DisplayServer.window_set_position(usable.position + (usable.size - dimensions) / 2)
