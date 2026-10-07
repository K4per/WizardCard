class_name WizardArtGallery
extends Node3D

const NAMES: Array[String] = ["ambient_dust", "ambient_runes", "analysis_start", "analysis_loop", "analysis_complete", "prepare", "release", "counter", "damage", "heal", "mana", "load", "attach", "destroy", "leave"]
var reduced: bool = false
var paused: bool = false
var effects: Node3D = Node3D.new()
var timer: Timer = Timer.new()

func _ready() -> void:
	add_child(effects)
	var floor_mesh: MeshInstance3D = MeshInstance3D.new()
	var plane: PlaneMesh = PlaneMesh.new()
	plane.size = Vector2(18, 13)
	floor_mesh.mesh = plane
	floor_mesh.material_override = preload("res://art/nonmodel/a-gilded-v15/materials/leather.tres")
	add_child(floor_mesh)
	for index: int in range(NAMES.size()):
		var label: Label3D = Label3D.new()
		label.text = NAMES[index]
		label.font_size = 32
		label.pixel_size = 0.006
		label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		label.no_depth_test = true
		label.modulate = Color("f4d785")
		label.position = _anchor(index) + Vector3(0, 0.18, 1.05)
		add_child(label)
	var layer: CanvasLayer = CanvasLayer.new()
	add_child(layer)
	var bar: HBoxContainer = HBoxContainer.new()
	bar.position = Vector2(20, 20)
	layer.add_child(bar)
	for title: String in ["Replay", "Reduced / Normal", "Pause / Resume", "Clear"]:
		var button: Button = Button.new()
		button.text = title
		button.custom_minimum_size = Vector2(170, 40)
		button.pressed.connect(_command.bind(title))
		bar.add_child(button)
	timer.wait_time = 1.5
	timer.timeout.connect(replay)
	add_child(timer)
	timer.start()
	replay()

func _anchor(index: int) -> Vector3:
	return Vector3((index % 5 - 2) * 3.2, 0.05, (index / 5 - 1) * 3.5)

func clear() -> void:
	for node: Node in effects.get_children():
		effects.remove_child(node)
		node.queue_free()

func replay() -> void:
	if paused:
		return
	clear()
	for index: int in range(NAMES.size()):
		var scene: PackedScene = load("res://art/nonmodel/a-gilded-v15/effects/" + NAMES[index] + ".tscn") as PackedScene
		var effect: WizardArtEffect3D = scene.instantiate() as WizardArtEffect3D
		effects.add_child(effect)
		effect.position = _anchor(index)
		effect.play(reduced)

func _command(title: String) -> void:
	match title:
		"Reduced / Normal":
			reduced = not reduced
			replay()
		"Pause / Resume":
			paused = not paused
			for node: Node in effects.get_children():
				(node as WizardArtEffect3D).set_paused(paused)
		"Clear":
			timer.stop()
			clear()
		_:
			timer.start()
			replay()
