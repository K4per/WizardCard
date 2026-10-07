class_name WizardBattlefield
extends Control

signal selected(card: Dictionary)
signal inspected(card: Dictionary)
signal dropped(data: Dictionary, destination: int, target: String)
signal browsed(side: int, zone: int)
const CARD_SCENE: PackedScene = preload("res://scenes/match/card_3d.tscn")
const CUE_EFFECTS: Dictionary = {2: "analysis_complete", 3: "prepare", 4: "release", 5: "prepare", 6: "counter", 7: "damage", 8: "heal", 9: "mana", 10: "load"}
@export var camera_pitch: float = 53.0
@export var camera_fov: float = 38.0
@onready var viewport: SubViewport = $ViewportContainer/Viewport
@onready var camera: Camera3D = $ViewportContainer/Viewport/World/Camera
@onready var world: Node3D = $ViewportContainer/Viewport/World
@onready var card_layer: Node3D = $ViewportContainer/Viewport/World/Cards
@onready var zone_layer: Node3D = $ViewportContainer/Viewport/World/Zones
@onready var surface: WizardTableSurface = $Surface
@onready var effects: Node3D = $ViewportContainer/Viewport/World/Effects
var view: Dictionary = {}
var cards: Dictionary = {}
var zones: Dictionary = {}
var texture_cache: Dictionary = {}
var skin: WizardSkin
var input_enabled: bool = true
var reduced: bool = false
var _context: String = ""
var _revision: String = ""
var _batch: String = ""
var _busy_left: float = 0.0
var _paused: bool = false
var _effect_tweens: Array[Tween] = []

func _ready() -> void:
	surface.board = self
	surface.card_selected.connect(func(card: Dictionary) -> void: selected.emit(card))
	surface.inspected.connect(func(card: Dictionary) -> void: inspected.emit(card))
	surface.zone_selected.connect(func(side: int, zone: int) -> void: browsed.emit(side, zone))
	surface.dropped.connect(func(data: Dictionary, destination: int, target: String) -> void: dropped.emit(data, destination, target))
	for node: Node in zone_layer.get_children():
		var zone: WizardZone3D = node as WizardZone3D
		zones["%d:%d" % [zone.player_side, zone.zone]] = zone
	resized.connect(_fit_camera)
	_fit_camera.call_deferred()
	set_process(false)

func _fit_camera() -> void:
	if not is_node_ready() or not is_inside_tree() or not camera.is_inside_tree() or size.x < 100 or size.y < 300:
		return
	camera.fov = camera_fov
	var direction: Vector3 = Vector3(0, sin(deg_to_rad(camera_pitch)), cos(deg_to_rad(camera_pitch)))
	# Fit actual projected corners; Godot's keep-aspect mode and perspective depth
	# make a width/height-only FOV approximation unreliable in narrow viewports.
	var near_distance: float = 8.0
	var far_distance: float = 90.0
	var bounds: Rect2 = Rect2(Vector2(16, 85), size - Vector2(32, 250))
	for iteration: int in range(16):
		var distance: float = (near_distance + far_distance) * 0.5
		camera.position = direction * distance
		camera.look_at(Vector3(0, 0, 1.1), Vector3.UP)
		var fits: bool = true
		for x: float in [-9.7, 9.7]:
			for z: float in [-7.2, 7.2]:
				if not bounds.has_point(camera.unproject_position(Vector3(x, 0.6, z))):
					fits = false
		if fits:
			far_distance = distance
		else:
			near_distance = distance
	camera.position = direction * far_distance
	camera.look_at(Vector3(0, 0, 1.1), Vector3.UP)
	# Keep dynamic area names readable as the same table is fitted to smaller windows.
	for zone: WizardZone3D in zones.values():
		zone.label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
		var label_distance: float = camera.global_position.distance_to(zone.label.global_position)
		zone.label.pixel_size = 2.0 * label_distance * tan(deg_to_rad(camera.fov) * 0.5) / size.y * 14.0 / zone.label.font_size
		zone.label.position.y = maxf(0.1, zone.label.pixel_size * zone.label.font_size * 0.65)

func clear_private() -> void:
	finish_effects()
	for node: Node in card_layer.get_children():
		card_layer.remove_child(node)
		node.queue_free()
	cards.clear()
	view.clear()
	_context = ""
	_revision = ""
	_batch = ""

func apply(resources: WizardSkin, snapshot: Dictionary, interaction: Dictionary, reduced_motion: bool) -> void:
	skin = resources
	reduced = reduced_motion
	(world.get_node("Ambient") as Node3D).visible = not reduced
	for node: Node in world.get_node("Ambient").get_children():
		(node as WizardArtEffect3D).play(reduced)
	var context: String = str(snapshot["generation"]) + ":" + str(snapshot["viewer"])
	if _context != context:
		clear_private()
		_context = context
	var changed: bool = _revision != str(snapshot["revision"])
	if changed:
		finish_effects()
	view = snapshot.duplicate(true)
	var visible_cards: Dictionary = {}
	for raw: Variant in view["cards"] as Array:
		var data: Dictionary = raw as Dictionary
		if int(data["zone"]) != 0:
			visible_cards[str(data["id"])] = data
	var viewer: int = int(view["viewer"])
	for side: int in range(2):
		var player: int = viewer if side == 0 else 1 - viewer
		var stats: Dictionary = (view["players"] as Array)[player] as Dictionary
		var deck_id: String = "deck:%d" % side
		if int(stats["deckCount"]) > 0:
			visible_cards[deck_id] = {"id": deck_id, "owner": player, "zone": 0, "hidden": true}
		if side == 1:
			for index: int in range(int(stats["handCount"])):
				var id: String = "opponent-hand:%d" % index
				visible_cards[id] = {"id": id, "owner": player, "zone": 1, "hidden": true}
	for id: String in cards.keys():
		if not visible_cards.has(id):
			var node: WizardCard3D = cards[id] as WizardCard3D
			card_layer.remove_child(node)
			node.queue_free()
			cards.erase(id)
	var counts: Dictionary = {}
	var slots: Dictionary = {}
	var targets: Dictionary = {}
	# Formations precede their hosts and seals, so relationships have stable anchors.
	var ordered: Array = visible_cards.values()
	ordered.sort_custom(func(a: Dictionary, b: Dictionary) -> bool: return _rank(a) < _rank(b))
	for raw: Variant in ordered:
		var data: Dictionary = raw as Dictionary
		var id: String = str(data["id"])
		var side: int = 0 if int(data["owner"]) == viewer else 1
		var zone: int = int(data["zone"])
		var key: String = "%d:%d" % [side, zone]
		var index: int = int(counts.get(key, 0))
		counts[key] = index + 1
		var at: Vector3 = _position_for(data, side, index, slots, targets)
		targets[id] = at
		var node: WizardCard3D = cards.get(id) as WizardCard3D
		var fresh: bool = node == null
		if fresh:
			node = CARD_SCENE.instantiate() as WizardCard3D
			card_layer.add_child(node)
			cards[id] = node
			node.destination = at
			node.position = at
		var was_hidden: bool = bool(node.data.get("hidden", false))
		var selected_id: String = str(interaction.get("selected", "0"))
		var selected_data: Dictionary = visible_cards.get(selected_id, {}) as Dictionary
		var related: bool = selected_id != "0" and not selected_id.is_empty() and (str(data.get("host", "0")) == selected_id or str(selected_data.get("host", "0")) == id)
		node.apply(data, _texture(data), skin.texture("master_back"), skin.font, selected_id == id or related, (interaction.get("candidates", []) as Array).has(id))
		var visual_state: String = "host" if related else ("selected" if selected_id == id else "")
		if related and int(data.get("zone", 0)) == 7:
			visual_state = "seal"
		if (interaction.get("candidates", []) as Array).has(id):
			visual_state = "cost" if int(interaction.get("step", 0)) == 2 else "candidate"
		var command: Dictionary = interaction.get("command", {}) as Dictionary if interaction.get("command") is Dictionary else {}
		if str(command.get("target", "0")) == id or (command.get("targets", []) as Array).has(id):
			visual_state = "target"
		if str(command.get("discard", "0")) == id:
			visual_state = "cost"
		node.show_interaction(skin, "" if bool(data.get("hidden", false)) else visual_state)
		node.visible = not (zone == 1 and side == 0)
		node.move_to(at, changed and not fresh and not reduced)
		if changed and not reduced and was_hidden and not bool(data.get("hidden", false)):
			node.flip()
	for key: String in zones:
		(zones[key] as WizardZone3D).configure(skin, int(counts.get(key, 0)))
	_revision = str(view["revision"])
	_fit_camera()

func _rank(data: Dictionary) -> int:
	if int(data.get("zone", 0)) == 7:
		return 2
	if str(data.get("host", "0")) != "0":
		return 1
	return 0

func _position_for(data: Dictionary, side: int, index: int, slots: Dictionary, targets: Dictionary) -> Vector3:
	var sign_z: float = 1.0 if side == 0 else -1.0
	var zone: int = int(data["zone"])
	if zone == 0:
		return Vector3(-8.1 * sign_z, 0.13, 5.4 * sign_z)
	if zone == 1:
		if side == 1:
			var count: int = int(((view["players"] as Array)[1 - int(view["viewer"])] as Dictionary)["handCount"])
			return Vector3(2.8 + (index - (count - 1) * 0.5) * minf(0.55, 5.5 / maxf(1, count)), 0.35 + index * 0.008, -6.1)
		return Vector3((index - 3) * 0.6, 0.12 + index * 0.008, 6.1 * sign_z)
	var host: String = str(data.get("host", "0"))
	if host != "0" and targets.has(host):
		var n: int = int(slots.get(host, 0))
		slots[host] = n + 1
		return (targets[host] as Vector3) + Vector3(0.28 * (n + 1), 0.07 * (n + 1), 0.28 * (n + 1))
	var key: String = "%d:%d" % [side, zone]
	var panel: WizardZone3D = zones.get(key) as WizardZone3D
	if panel == null:
		panel = zones["%d:5" % side] as WizardZone3D
	if zone == 3:
		var slot_key: String = "slots:%d" % side
		var used: int = int(slots.get(slot_key, 0))
		var body: int = int((data.get("definition", {}) as Dictionary).get("body", 1))
		slots[slot_key] = used + body
		return panel.position + Vector3(-3.3 + (used + (body - 1) * 0.5) * 1.65, 0.1 + index * 0.01, 0.35)
	var step: float = minf(1.2, (panel.dimensions.x - 1.3) / maxf(1, _zone_count(int(data["owner"]), zone) - 1))
	return panel.position + Vector3(-panel.dimensions.x * 0.5 + 0.8 + index * step, 0.1 + index * 0.012, 0.12)

func _zone_count(owner: int, zone: int) -> int:
	var count: int = 0
	for raw: Variant in view.get("cards", []) as Array:
		var data: Dictionary = raw as Dictionary
		if int(data["owner"]) == owner and int(data["zone"]) == zone:
			count += 1
	return count

func _texture(data: Dictionary) -> Texture2D:
	if bool(data.get("hidden", false)):
		return skin.texture("master_back")
	var definition: Dictionary = data.get("definition", {}) as Dictionary
	var id: String = str(definition.get("id", ""))
	if texture_cache.has(id):
		return texture_cache[id] as Texture2D
	var kind: int = clampi(int(definition.get("type", 0)), 0, 4)
	var frame: Texture2D = skin.texture("master_" + WizardCardView.TYPE_KEYS[kind])
	var illustration: Texture2D = skin.illustration(id)
	if DisplayServer.get_name() == "headless":
		return frame
	var image: Image = frame.get_image()
	if image == null or image.is_empty():
		return frame
	image.convert(Image.FORMAT_RGBA8)
	image.resize(384, 576, Image.INTERPOLATE_LANCZOS)
	# Transparent art belongs on paper; keep the source master and illustration intact.
	var paper: Image = Image.create(384, 576, false, Image.FORMAT_RGBA8)
	paper.fill(Color("ead5a6"))
	paper.blend_rect(image, Rect2i(0, 0, 384, 576), Vector2i.ZERO)
	image = paper
	if illustration != null:
		var art: Image = illustration.get_image()
		if art != null and not art.is_empty():
			art.convert(Image.FORMAT_RGBA8)
			var ratio: float = minf(270.0 / art.get_width(), 190.0 / art.get_height())
			art.resize(maxi(1, roundi(art.get_width() * ratio)), maxi(1, roundi(art.get_height() * ratio)), Image.INTERPOLATE_NEAREST)
			image.blend_rect(art, Rect2i(Vector2i.ZERO, art.get_size()), Vector2i(57, 115) + (Vector2i(270, 190) - art.get_size()) / 2)
	image.generate_mipmaps()
	var texture: ImageTexture = ImageTexture.create_from_image(image)
	texture_cache[id] = texture
	return texture

func pick(at: Vector2) -> Dictionary:
	if not input_enabled or view.is_empty() or not Rect2(Vector2.ZERO, size).has_point(at):
		return {}
	var origin: Vector3 = camera.project_ray_origin(at)
	var direction: Vector3 = camera.project_ray_normal(at)
	var nearest: float = INF
	var found: WizardCard3D
	for node: WizardCard3D in cards.values():
		if not node.visible:
			continue
		var distance: float = node.hit_distance(origin, direction)
		if distance >= 0 and distance < nearest:
			nearest = distance
			found = node
	if found != null:
		var zone: int = int(found.data["zone"])
		if zone == 7:
			var host: WizardCard3D = cards.get(str(found.data.get("host", "0"))) as WizardCard3D
			zone = int(host.data["zone"]) if host != null else 3
		var result: Dictionary = {"zone": zone, "side": 0 if int(found.data["owner"]) == int(view["viewer"]) else 1}
		if not str(found.data["id"]).contains(":"):
			result["card"] = found.data
		return result
	if absf(direction.y) < 0.00001:
		return {}
	var point: Vector3 = origin + direction * (-origin.y / direction.y)
	for zone: WizardZone3D in zones.values():
		if zone.contains(point):
			return {"zone": zone.zone, "side": zone.player_side}
	return {}

func finish_effects() -> void:
	for node: WizardCard3D in cards.values():
		node.settle()
	for tween: Tween in _effect_tweens:
		if tween.is_valid():
			tween.kill()
	_effect_tweens.clear()
	for node: Node in effects.get_children():
		effects.remove_child(node)
		node.queue_free()
	_busy_left = 0
	set_process(false)

func play_cues(batch: Dictionary) -> void:
	var key: String = str(batch.get("generation", "")) + ":" + str(batch.get("revision", ""))
	if key == _batch or str(batch.get("generation", "")) != str(view.get("generation", "")) or str(batch.get("revision", "")) != str(view.get("revision", "")):
		return
	_batch = key
	for raw: Variant in batch.get("cues", []) as Array:
		var cue: Dictionary = raw as Dictionary
		var node: WizardCard3D = cards.get(str(cue.get("source", "0"))) as WizardCard3D
		var kind: int = int(cue["kind"])
		var side: int = 0 if int(cue["player"]) == int(view["viewer"]) else 1
		var at: Vector3 = Vector3(0, 0.8, 3.5 if side == 0 else -3.5)
		if node != null and node.visible and kind < 7:
			at = node.destination + Vector3(0, 0.8, 0)
		if CUE_EFFECTS.has(kind) and effects.get_child_count() < 12:
			var scene: PackedScene = load("res://art/nonmodel/a-gilded-v15/effects/" + str(CUE_EFFECTS[kind]) + ".tscn") as PackedScene
			var effect: WizardArtEffect3D = scene.instantiate() as WizardArtEffect3D
			effects.add_child(effect)
			effect.position = Vector3(at.x, 0.09, at.z)
			if kind in [9, 10]:
				effect.motion_style = 1 if int(cue.get("amount", 0)) > 0 else 2
			effect.play(reduced)
		if not reduced and kind in [0, 1] and int(cue.get("from", 0)) in [0, 1] and node != null and node.visible:
			var destination: Vector3 = node.destination
			var start: Vector3 = Vector3(-8, 0.5, 5) if int(cue.get("from", 0)) == 0 else Vector3(0, 1.3, 6)
			node.settle()
			node.position = start * (1.0 if side == 0 else -1.0)
			node.destination = node.position
			node.move_to(destination, true)
		# Anonymous draw feedback never invents an opponent card identity.
		if kind >= 2 or node == null or not node.visible:
			var label: Label3D = Label3D.new()
			label.font = skin.font
			label.text = str(cue.get("text", ""))
			if int(cue.get("amount", 0)) != 0:
				label.text += " %+d" % int(cue["amount"])
			label.font_size = 48
			label.pixel_size = 0.012
			label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
			var cue_colors: Dictionary = {5: "bda2dd", 6: "bfcbd0", 7: "cf7361", 8: "70bda0", 9: "72aacf", 10: "f4d785"}
			label.modulate = Color(str(cue_colors.get(kind, "ffe4a0")))
			label.no_depth_test = true
			effects.add_child(label)
			label.position = at + Vector3(0, effects.get_child_count() * 0.12, 0)
			var tween: Tween = create_tween()
			tween.tween_property(label, "position:y", label.position.y + 0.7, 0.45)
			tween.parallel().tween_property(label, "modulate:a", 0.0, 0.45)
			tween.tween_callback(label.queue_free)
			_effect_tweens.append(tween)
	_busy_left = 0.16 if reduced else 0.5
	set_process(true)

func set_paused(paused: bool) -> void:
	_paused = paused
	input_enabled = not paused
	for node: WizardCard3D in cards.values():
		if node.motion != null and node.motion.is_valid():
			if paused:
				node.motion.pause()
			else:
				node.motion.play()
	for tween: Tween in _effect_tweens:
		if tween.is_valid():
			if paused:
				tween.pause()
			else:
				tween.play()
	for node: Node in effects.get_children():
		if node is WizardArtEffect3D:
			(node as WizardArtEffect3D).set_paused(paused)
	for node: Node in world.get_node("Ambient").get_children():
		(node as WizardArtEffect3D).set_paused(paused)

func busy() -> bool:
	return _busy_left > 0

func _process(delta: float) -> void:
	if not _paused:
		_busy_left = maxf(0, _busy_left - delta)
		if _busy_left == 0:
			set_process(false)
