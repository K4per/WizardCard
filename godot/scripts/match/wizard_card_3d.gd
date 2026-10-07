class_name WizardCard3D
extends Node3D

@export var dimensions: Vector2 = Vector2(1.0, 1.5)
@onready var face: MeshInstance3D = $Face
@onready var back: MeshInstance3D = $Back
@onready var edge: MeshInstance3D = $Edge
@onready var title: Label3D = $Title
@onready var state_label: Label3D = $State
var data: Dictionary = {}
var destination: Vector3 = Vector3.ZERO
var motion: Tween
var material: StandardMaterial3D
var edge_material: StandardMaterial3D
var interaction_material: StandardMaterial3D
var interaction_overlay: MeshInstance3D
var visual_state: String = ""

func _ready() -> void:
	material = (preload("res://art/nonmodel/a-gilded-v15/materials/card_face.tres") as StandardMaterial3D).duplicate() as StandardMaterial3D
	face.material_override = material
	edge_material = (preload("res://art/nonmodel/a-gilded-v15/materials/paper_edge.tres") as StandardMaterial3D).duplicate() as StandardMaterial3D
	edge_material.albedo_color = Color("ac8650")
	edge.material_override = edge_material
	interaction_overlay = MeshInstance3D.new()
	var plane: PlaneMesh = PlaneMesh.new()
	plane.size = Vector2(1.17, 1.76)
	interaction_overlay.mesh = plane
	interaction_overlay.position.y = 0.04
	interaction_material = StandardMaterial3D.new()
	interaction_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	interaction_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	interaction_material.texture_filter = BaseMaterial3D.TEXTURE_FILTER_NEAREST
	interaction_overlay.material_override = interaction_material
	interaction_overlay.visible = false
	add_child(interaction_overlay)

func show_interaction(resources: WizardSkin, state: String) -> void:
	visual_state = state
	interaction_overlay.visible = not state.is_empty()
	if not state.is_empty():
		interaction_material.albedo_texture = resources.texture("nm_interaction_" + state)

func apply(card: Dictionary, texture: Texture2D, card_back: Texture2D, font: Font, chosen: bool, candidate: bool) -> void:
	data = card.duplicate(true)
	var hidden: bool = bool(data.get("hidden", false))
	material.albedo_texture = card_back if hidden else texture
	var reverse: StandardMaterial3D = (preload("res://art/nonmodel/a-gilded-v15/materials/card_back.tres") as StandardMaterial3D).duplicate() as StandardMaterial3D
	reverse.albedo_texture = card_back
	back.material_override = reverse
	var definition: Dictionary = data.get("definition", {}) as Dictionary
	title.font = font
	state_label.font = font
	title.text = "" if hidden else str(definition.get("name", ""))
	state_label.text = ""
	if not hidden:
		var state: int = int(data.get("spellState", 0))
		if state > 0:
			state_label.text = ["", "解析 %d" % int(data.get("turnsToReady", 0)), "就绪", "待释放", "持续"][clampi(state, 0, 4)]
		elif int(definition.get("type", -1)) == 3:
			state_label.text = "环 %d/%d" % [int(data.get("occupiedRings", 0)), int(data.get("effectiveRings", 0))]
	edge_material.albedo_color = Color("39d0b1") if candidate else (Color("fff0a0") if chosen else Color("ac8650"))
	edge.scale = Vector3(1.07, 1, 1.07) if chosen or candidate else Vector3.ONE

func settle() -> void:
	if motion != null and motion.is_valid():
		motion.kill()
	position = destination
	rotation = Vector3.ZERO

func move_to(at: Vector3, animate: bool) -> void:
	if destination.is_equal_approx(at):
		return
	if motion != null and motion.is_valid():
		motion.kill()
	destination = at
	if animate:
		motion = create_tween().set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_OUT)
		motion.tween_property(self, "position", at, 0.28)
	else:
		position = at

func flip() -> void:
	if motion != null and motion.is_valid():
		motion.kill()
	position = destination
	rotation.z = PI
	motion = create_tween().set_trans(Tween.TRANS_CUBIC).set_ease(Tween.EASE_OUT)
	motion.tween_property(self, "rotation:z", 0.0, 0.4)

func hit_distance(origin: Vector3, direction: Vector3) -> float:
	var local_origin: Vector3 = global_transform.affine_inverse() * origin
	var local_direction: Vector3 = global_transform.basis.inverse() * direction
	if absf(local_direction.y) < 0.00001:
		return -1.0
	var distance: float = (0.05 - local_origin.y) / local_direction.y
	var point: Vector3 = local_origin + distance * local_direction
	if distance < 0 or absf(point.x) > dimensions.x * 0.55 or absf(point.z) > dimensions.y * 0.55:
		return -1.0
	return distance
