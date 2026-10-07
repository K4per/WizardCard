@tool
class_name WizardZone3D
extends Node3D

@export var player_side: int = 0
@export var zone: int = 3
@export var title_text: String = "解析区"
@export var dimensions: Vector2 = Vector2(6, 3)
@export var texture_key: String = "region_analysis"
const ENGRAVING: Shader = preload("res://art/nonmodel/a-gilded-v15/shaders/spatial_zone_engraving.gdshader")
var _material: ShaderMaterial
@onready var surface: MeshInstance3D = $Surface
@onready var label: Label3D = $Label

func _ready() -> void:
	var plane: PlaneMesh = PlaneMesh.new()
	plane.size = dimensions
	surface.mesh = plane
	label.text = title_text
	label.position.z = -dimensions.y * 0.5 + 0.18
	$Slots.visible = zone == 3

func configure(skin: WizardSkin, count: int) -> void:
	if _material == null:
		_material = ShaderMaterial.new()
		_material.shader = ENGRAVING
		_material.set_shader_parameter("engraving", skin.texture("nm_zone_" + texture_key.trim_prefix("region_")))
		surface.material_override = _material
		var slot_material: ShaderMaterial = ShaderMaterial.new()
		slot_material.shader = ENGRAVING
		slot_material.set_shader_parameter("engraving", skin.texture("nm_zone_slot"))
		slot_material.set_shader_parameter("opacity", 0.19)
		for node: Node in $Slots.get_children():
			(node as MeshInstance3D).material_override = slot_material
	label.font = skin.font
	label.text = "%s · %d" % [title_text, count]

func contains(point: Vector3) -> bool:
	var local: Vector3 = to_local(point)
	return absf(local.x) <= dimensions.x * 0.5 and absf(local.z) <= dimensions.y * 0.5
