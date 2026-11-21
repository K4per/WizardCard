extends ColorRect

# Response preferences stay reachable while the battlefield is modal.
var passthrough: Control

func _has_point(point: Vector2) -> bool:
	return passthrough == null or not passthrough.get_global_rect().has_point(get_global_transform() * point)

