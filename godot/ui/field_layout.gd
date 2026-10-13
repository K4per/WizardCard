@tool
class_name FieldLayout
extends Resource

# Shared presentation geometry. These values never determine legal ring counts,
# slots, actions or costs; those still arrive in the viewer projection.
@export_group("Field Regions")
@export_range(0.0, 0.4, 0.005) var left_margin_ratio: float = 0.13:
	set(value):
		left_margin_ratio = value
		emit_changed()
@export_range(0.0, 400.0, 1.0) var deck_label_clearance: float = 0.0:
	set(value):
		deck_label_clearance = value
		emit_changed()
@export_range(0.0, 0.4, 0.005) var right_margin_ratio: float = 0.13:
	set(value):
		right_margin_ratio = value
		emit_changed()
## X is the top edge as a fraction of field height; Y is the height fraction.
@export var opponent_analysis: Vector2 = Vector2(0.12, 0.32):
	set(value):
		opponent_analysis = value
		emit_changed()
## X is the top edge as a fraction of field height; Y is the height fraction.
@export var own_analysis: Vector2 = Vector2(0.56, 0.32):
	set(value):
		own_analysis = value
		emit_changed()
## X is the top edge as a fraction of field height; Y is the height fraction.
@export var opponent_casting: Vector2 = Vector2(0.445, 0.055):
	set(value):
		opponent_casting = value
		emit_changed()
## X is the top edge as a fraction of field height; Y is the height fraction.
@export var own_casting: Vector2 = Vector2(0.50, 0.055):
	set(value):
		own_casting = value
		emit_changed()

@export_group("Pile Positions")
## Positions used unless FieldCanvas enables scene-region overrides.
## Values are normalized viewport coordinates so the same resource scales
## cleanly across 1280p, 1600p and 1920p windows.
@export var main_deck_position: Vector2 = Vector2(0.895, 0.74):
	set(value):
		main_deck_position = value
		emit_changed()
@export var ash_position: Vector2 = Vector2(0.895, 0.57):
	set(value):
		ash_position = value
		emit_changed()
@export var side_deck_position: Vector2 = Vector2(0.035, 0.74):
	set(value):
		side_deck_position = value
		emit_changed()
@export var pile_size: Vector2 = Vector2(0.068, 0.13):
	set(value):
		pile_size = value
		emit_changed()
## Horizontal and vertical insets as fractions of each analysis region.
@export var analysis_safe_margin: Vector2 = Vector2(0.025, 0.025):
	set(value):
		analysis_safe_margin = value
		emit_changed()
@export_range(0.0, 80.0, 1.0) var analysis_header_height: float = 28.0:
	set(value):
		analysis_header_height = value
		emit_changed()

@export_group("Formation Tiles")
@export_range(1, 5, 1) var formation_slot_count: int = 5:
	set(value):
		formation_slot_count = value
		emit_changed()
@export_range(2.0, 32.0, 1.0) var slot_gap: float = 6.0:
	set(value):
		slot_gap = value
		emit_changed()
@export_range(16.0, 160.0, 1.0) var tile_max_size: float = 60.0:
	set(value):
		tile_max_size = value
		emit_changed()
@export_range(0.02, 0.2, 0.001) var tile_height_ratio: float = 0.058:
	set(value):
		tile_height_ratio = value
		emit_changed()
@export_range(0.0, 64.0, 1.0) var formation_spacing: float = 6.0:
	set(value):
		formation_spacing = value
		emit_changed()
@export_range(0.0, 1.0, 0.01) var ring_gap_ratio: float = 0.30:
	set(value):
		ring_gap_ratio = value
		emit_changed()

@export_range(0.08, 0.35, 0.01) var formation_slot_width_ratio: float = 0.12:
	set(value):
		formation_slot_width_ratio = value
		emit_changed()

@export_group("Hand Fan")
@export_range(0.05, 0.4, 0.005) var hand_height_ratio: float = 0.13:
	set(value):
		hand_height_ratio = value
		emit_changed()
@export_range(64.0, 400.0, 1.0) var hand_max_height: float = 224.0:
	set(value):
		hand_max_height = value
		emit_changed()
@export_range(0.2, 0.9, 0.01) var hand_width_ratio: float = 0.70:
	set(value):
		hand_width_ratio = value
		emit_changed()
@export_range(0.1, 1.0, 0.01) var hand_card_spacing_ratio: float = 0.66:
	set(value):
		hand_card_spacing_ratio = value
		emit_changed()
@export_range(0.0, 40.0, 0.5) var fan_max_angle: float = 22.0:
	set(value):
		fan_max_angle = value
		emit_changed()
@export_range(0.0, 10.0, 0.1) var fan_angle_per_card: float = 4.5:
	set(value):
		fan_angle_per_card = value
		emit_changed()
@export_range(0.0, 0.5, 0.01) var fan_arch_ratio: float = 0.10:
	set(value):
		fan_arch_ratio = value
		emit_changed()
@export_range(0.0, 0.5, 0.01) var fan_edge_drop_ratio: float = 0.16:
	set(value):
		fan_edge_drop_ratio = value
		emit_changed()
@export_range(0.0, 1.0, 0.01) var hover_raise_ratio: float = 0.62:
	set(value):
		hover_raise_ratio = value
		emit_changed()
@export_range(0.0, 64.0, 1.0) var hand_bottom_margin: float = 12.0:
	set(value):
		hand_bottom_margin = value
		emit_changed()

func field_region(dimensions: Vector2, vertical: Vector2) -> Rect2:
	var left: float = maxf(dimensions.x * left_margin_ratio, minf(deck_label_clearance, dimensions.x * 0.25))
	var width: float = maxf(1.0, dimensions.x - left - maxf(30.0, dimensions.x * right_margin_ratio))
	var top: float = clampf(vertical.x, 0.0, 1.0)
	var height: float = clampf(vertical.y, 0.0, 1.0 - top)
	return Rect2(left, dimensions.y * top, width, dimensions.y * height)

func analysis_content(region: Rect2) -> Rect2:
	var margin: Vector2 = analysis_safe_margin.clamp(Vector2.ZERO, Vector2.ONE * 0.45)
	var inset: Vector2 = region.size * margin
	var header: float = minf(analysis_header_height, maxf(0.0, region.size.y - inset.y * 2.0))
	return Rect2(region.position + inset + Vector2(0, header), (region.size - inset * 2.0 - Vector2(0, header)).max(Vector2.ZERO))
