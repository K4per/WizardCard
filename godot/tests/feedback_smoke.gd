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
	var second: FieldPresentation = FieldPresentation.new()
	second.hand.assign([_card("one"), _card("two")])
	second.own_life = 35
	canvas.configure(second)
	await _settle()
	_check(canvas.field == second and canvas.field.hand.size() == 2, "New projection is applied before animation completes")
	_check(canvas._transition.changed.has("two") and canvas._transition.life[0].a > 0, "Draw and damage have feedback")
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
	_check(canvas._transition.life[0].g > canvas._transition.life[0].r, "Healing uses a distinct feedback color")
	canvas.set_privacy(true)
	_check(canvas._transition_strength == 0.0 and canvas._transition.changed.is_empty() and canvas._transition.departures.is_empty(), "Privacy immediately clears all old feedback")
	canvas.set_privacy(false)
	UiFactory.reduced_motion = true
	canvas.configure(first)
	_check(canvas.field == first and canvas._transition_strength == 0.0, "Reduced motion applies the result without animation")
	UiFactory.reduced_motion = false
	canvas.configure(second)
	await create_timer(0.35).timeout
	_check(is_zero_approx(canvas._transition_strength), "Feedback converges after completion")
	canvas.queue_free()
	await _settle()
	print(JSON.stringify({"checks": _checks, "failures": _failures}))
	quit(0 if _failures.is_empty() else 1)
