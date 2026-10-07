class_name WizardArtEffect3D
extends Node3D

const SHADER: Shader = preload("res://art/nonmodel/a-gilded-v15/shaders/spatial_fx_rune.gdshader")
@export var effect_texture: Texture2D
@export_range(0.05, 10.0) var duration: float = 0.5
@export var looping: bool = false
@export_enum("Expand", "Rise", "Contract") var motion_style: int = 0
var material: ShaderMaterial
var motion: Tween
var _paused: bool = false
var _phase: float = 0.0

func _ready() -> void:
	var mesh: MeshInstance3D = MeshInstance3D.new()
	var plane: PlaneMesh = PlaneMesh.new()
	plane.size = Vector2(1.6, 1.6)
	mesh.mesh = plane
	material = ShaderMaterial.new()
	material.shader = SHADER
	material.set_shader_parameter("effect_texture", effect_texture)
	material.set_shader_parameter("looping", looping)
	mesh.material_override = material
	add_child(mesh)
	set_process(looping)
	if not looping:
		play(false)

func play(reduced: bool) -> void:
	if motion != null and motion.is_valid():
		motion.kill()
	if looping:
		visible = not reduced
		set_process(not reduced)
		return
	var seconds: float = minf(duration, 0.16) if reduced else duration
	motion = create_tween().set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_OUT)
	if not reduced:
		scale = Vector3.ONE * (1.15 if motion_style == 2 else 0.75)
		motion.tween_property(self, "scale", Vector3.ONE * (0.6 if motion_style == 2 else 1.2), seconds)
		if motion_style == 1:
			motion.parallel().tween_property(self, "position:y", position.y + 0.35, seconds)
	motion.parallel().tween_method(_opacity, 0.85, 0.0, seconds)
	motion.tween_callback(queue_free)
	set_paused(_paused)

func _opacity(value: float) -> void:
	material.set_shader_parameter("opacity", value)

func set_paused(paused: bool) -> void:
	_paused = paused
	if motion != null and motion.is_valid():
		if paused:
			motion.pause()
		else:
			motion.play()

func _process(delta: float) -> void:
	if not _paused:
		_phase += delta * TAU / duration
		material.set_shader_parameter("phase", _phase)
