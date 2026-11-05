extends SceneTree

func _initialize() -> void:
	var verification: Node = load("res://diagnostics/central_verification.tscn").instantiate()
	root.add_child.call_deferred(verification)
