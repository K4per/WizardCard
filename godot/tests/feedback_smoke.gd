extends SceneTree

var _failures: Array[String] = []
var _checks: int = 0

func _initialize() -> void:
	_run.call_deferred()

func _check(condition: bool, message: String) -> void:
	_checks += 1
	if not condition:
		_failures.append(message)
		push_error(message)

func _settle() -> void:
	await process_frame
	await process_frame

func _card(id: String) -> CardPresentation:
	var card: CardPresentation = CardPresentation.new()
	card.instance_id = id
	card.title = id
	card.type_key = "analysis"
	return card

func _run() -> void:
	var canvas: FieldCanvas = load("res://ui/field_canvas.tscn").instantiate() as FieldCanvas
	canvas.greybox = true
	root.add_child(canvas)
	root.size = Vector2i(1280, 720)
	var first: FieldPresentation = FieldPresentation.new()
	first.hand.assign([_card("one")])
	canvas.configure(first)
	await _settle()
	var motion := InputEventMouseMotion.new()
	motion.position = canvas.card_bounds(first.hand[0]).get_center()
	canvas._gui_input(motion)
	await create_timer(0.18).timeout
	var held: Rect2 = canvas.card_bounds(first.hand[0])
	canvas.configure(first)
	await _settle()
	_check(canvas._hover_id == "one" and canvas.card_bounds(first.hand[0]).is_equal_approx(held), "A stationary hover survives an authoritative refresh")
	_check(canvas.has_method("present_events"), "The field consumes confirmed viewer movement events")
	var second: FieldPresentation = FieldPresentation.new()
	second.hand.assign([_card("one"), _card("two")])
	second.own_life = 35
	canvas.configure(second)
	await _settle()
	_check(canvas.field == second and canvas.field.hand.size() == 2, "New projection is applied before animation completes")
	_check(canvas._transition.changed.has("two") and canvas._transition.life[0].a == 0, "A projection refresh does not flash red ahead of the typed life delta")
	canvas.present_events([
		{"kind": "life_delta", "affectedPlayer": 1, "actualAmount": -3, "damageType": "cold"},
		{"kind": "temporary_life_delta", "affectedPlayer": 0, "actualAmount": -4},
		{"kind": "life_delta", "affectedPlayer": 1, "actualAmount": -2, "damageType": "fire"},
		{"kind": "temporary_life_delta", "affectedPlayer": 0, "actualAmount": 4},
		{"kind": "life_delta", "affectedPlayer": 0, "actualAmount": -1}
	])
	var borders: Array[Dictionary] = canvas._damage_borders
	_check(borders.size() == 3, "Only regular life losses produce a border")
	if borders.size() == 3:
		_check(borders[0].kind == "cold" and borders[1].kind == "fire" and borders[2].kind == "", "Typed and untyped losses retain authority order")
		_check(borders[0].player == 1 and borders[2].player == 0 and borders[2].age < borders[1].age, "A later hit on the other side cannot start before an earlier queued hit")
	canvas.configure(second)
	_check(borders.size() == 3, "Same-viewer refresh preserves a running hit and its successor")
	canvas._process(0.85)
	_check(borders.size() == 2 and borders[0].kind == "fire" and borders[0].age > 0 and borders[1].age < 0, "The second hit can play while the third awaits its turn")
	await _settle()
	canvas.stop_feedback()
	canvas.present_events([
		{"kind": "life_delta", "affectedPlayer": 1, "actualAmount": -2},
		{"kind": "overload", "affectedPlayer": 1, "actualAmount": 2},
		{"kind": "life_delta", "affectedPlayer": 0, "actualAmount": -1},
		{"kind": "exhaustion", "affectedPlayer": 0, "actualAmount": 1},
		{"kind": "mana_gain", "affectedPlayer": 1, "actualAmount": 3},
		{"kind": "life_delta", "affectedPlayer": 0, "actualAmount": 4},
		{"kind": "mana_gain", "affectedPlayer": 0, "actualAmount": 0},
		{"kind": "mana_gain", "affectedPlayer": -1, "actualAmount": 1}
	])
	_check(borders.size() == 4, "Overload, exhaustion, mana gain, and life recovery each produce one border effect")
	if borders.size() == 4:
		_check(borders.map(func(hit: Dictionary) -> String: return hit.kind) == ["overload", "exhaustion", "mana_recovery", "life_recovery"], "Resource feedback preserves the native event order and skips zero gains")
		_check(borders[0].player == 1 and borders[1].player == 0 and borders[2].player == 1 and borders[3].player == 0, "Each resource effect targets the player from its event")
		_check(borders.all(func(hit: Dictionary) -> bool: return hit.duration == 0.8), "Each resource effect uses the extended border duration")
	canvas.stop_feedback()
	var card_bounds: Rect2 = canvas.card_bounds(second.hand[0])
	canvas._set_transition_strength(0.0)
	await _settle()
	_check(canvas.card_bounds(second.hand[0]).is_equal_approx(card_bounds), "Animation never shifts card hit regions")
	var third: FieldPresentation = FieldPresentation.new()
	var casting: CardPresentation = _card("two")
	casting.status = "准备施法"
	third.own_casting.assign([casting])
	third.own_life = 42
	canvas.configure(third)
	await _settle()
	_check(canvas.field.hand.is_empty() and canvas.field.own_casting.size() == 1, "Rapid transitions preserve the latest projection")
	_check(canvas._transition.changed.has("two") and not canvas._transition.departures.is_empty(), "Zone changes and departures have feedback")
	_check(canvas._transition.life[0] == Color("92d3a6"), "Healing uses the green feedback outline")
	canvas.set_privacy(true)
	_check(canvas._transition_strength == 0.0 and canvas._transition.changed.is_empty() and canvas._transition.departures.is_empty() and borders.is_empty(), "Privacy immediately clears all old feedback")
	canvas.present_events([{"kind": "life_delta", "affectedPlayer": 0, "actualAmount": -1, "damageType": "acid"}])
	_check(borders.is_empty(), "The privacy cover suppresses typed damage")
	canvas.set_action_targets(["two"])
	canvas.set_privacy(true)
	_check(canvas.target_ids.is_empty() and canvas.target_rings.is_empty(), "Privacy discards all old target hit regions")
	canvas._dragged = casting
	canvas._drag_active = true
	canvas._cancel_pointer_state()
	_check(canvas._dragged == null and not canvas._drag_active, "Focus loss cancels drag without changing the projection")
	canvas.set_privacy(false)
	canvas.present_events([{"kind": "life_delta", "affectedPlayer": 0, "actualAmount": -1, "damageType": "unknown"}])
	_check(borders.size() == 1 and borders[0].kind == "" and is_equal_approx(borders[0].duration, 0.24), "Unknown damage uses the original short red flash")
	var hit: Dictionary = {"kind": "life_delta", "affectedPlayer": 1, "actualAmount": -3, "damageType": "psychic"}
	canvas.present_events([hit])
	UiFactory.reduced_motion = true
	canvas._process(0.01)
	_check(borders.is_empty(), "Turning on reduced motion stops active borders")
	canvas.configure(first)
	canvas.present_events([hit])
	_check(borders.is_empty(), "Reduced motion suppresses border effects")
	_check(canvas.field == first and canvas._transition_strength == 0.0, "Reduced motion applies the result without animation")
	UiFactory.reduced_motion = false
	canvas.present_events([hit])
	var other_viewer := FieldPresentation.new()
	other_viewer.viewer = 1
	canvas.configure(other_viewer)
	_check(borders.is_empty(), "Changing viewers clears pending hits")
	canvas.configure(first)
	canvas.configure(second)
	await create_timer(0.35).timeout
	_check(is_zero_approx(canvas._transition_strength), "Feedback converges after completion")
	canvas.queue_free()
	await _settle()
	var page: MatchPage = (load("res://pages/match_page.tscn") as PackedScene).instantiate() as MatchPage
	root.add_child(page)
	page.service = FrontendService.new()
	page._show_cue({"cues": [{"text": "first", "duration": 0.2}]})
	await create_timer(0.1).timeout
	page._show_cue({"cues": [{"text": "latest", "duration": 1.5}]})
	await create_timer(0.7).timeout
	_check(page._cue.visible and page._cue.text == "latest" and page._cue.modulate.a > 0.99, "A superseded cue tween cannot hide the newest authoritative feedback")
	page._clear_cue()
	_check(not page._cue.visible and page._cue_tween == null, "Projection interruption immediately removes queued cue animation")
	page.queue_free()
	await _settle()
	var feedback: MatchFeedback = MatchFeedback.new()
	root.add_child(feedback)
	feedback.service = FrontendService.new()
	feedback.service.assets_directory = ProjectSettings.globalize_path("res://../assets")
	var arguments: PackedStringArray = OS.get_cmdline_user_args()
	for index: int in range(arguments.size() - 1):
		if arguments[index] == "--assets":
			feedback.service.assets_directory = arguments[index + 1]
	feedback.service.settings = {"masterVolume": 80, "soundVolume": 80, "responseTimingSound": false}
	feedback.present({"sounds": [{"file": "response.mp3", "gain": 1.0}]})
	_check(not feedback._last_played.has("response.mp3"), "Response popup sound is disabled by the presentation setting")
	feedback.service.settings["responseTimingSound"] = true
	feedback.present({"sounds": [{"file": "response.mp3", "gain": 1.0}]})
	_check(feedback._last_played.has("response.mp3"), "Response popup sound can be restored independently")
	for player: AudioStreamPlayer in feedback._players:
		if player.playing:
			await player.finished
	feedback.queue_free()
	await _settle()
	print(JSON.stringify({"checks": _checks, "failures": _failures}))
	quit(0 if _failures.is_empty() else 1)
