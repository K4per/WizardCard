class_name WizardHandCard
extends WizardCardView
var interaction_state: String = ""

func configure(resources: WizardSkin, data: Dictionary, kind: String = "hand") -> void:
	super.configure(resources, data, kind)
	if bool(card.get("hidden", false)):
		return
	var artwork: TextureRect = TextureRect.new()
	artwork.texture = skin.illustration(str(definition.get("id", "")))
	artwork.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	artwork.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	artwork.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	artwork.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(artwork)
	artwork.anchor_left = 0.15
	artwork.anchor_top = 0.25
	artwork.anchor_right = 0.85
	artwork.anchor_bottom = 0.64

func _draw() -> void:
	if skin == null or definition.is_empty():
		return
	if bool(card.get("hidden", false)):
		_fit(skin.texture("master_back"), Rect2(Vector2.ZERO, size))
		return
	var key: String = TYPE_KEYS[clampi(int(definition.get("type", 0)), 0, 4)]
	draw_rect(Rect2(Vector2(8, 10), size - Vector2(16, 20)), skin.color("cream"))
	_fit(skin.texture("master_" + key), Rect2(Vector2.ZERO, size))
	_text(str(definition.get("name", "")), Vector2(size.x * 0.13, size.y * 0.16), 12, size.x * 0.76, skin.color("cream"))
	var cost: String = str(definition.get("cost", 0))
	if int(definition.get("type", 0)) == 1:
		cost += " / " + str(definition.get("castCost", 0))
	_text("费用 " + cost, Vector2(size.x * 0.14, size.y * 0.77), 12, size.x * 0.74)
	_text(_state(), Vector2(size.x * 0.14, size.y * 0.87), 11, size.x * 0.74)
	var visual: String = interaction_state
	if visual.is_empty() and (has_focus() or get_rect().has_point(get_parent_control().get_local_mouse_position())):
		visual = "hover"
	if not visual.is_empty():
		draw_texture_rect(skin.texture("nm_interaction_" + visual), Rect2(Vector2.ZERO, size), false)
