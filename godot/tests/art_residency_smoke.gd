extends SceneTree

class CatalogueCanvas extends Control:
	var cards: Array[CardPresentation] = []
	var compact: bool = false
	var columns: int = 10
	var frames: Array[Rect2] = []

	func _draw() -> void:
		frames.clear()
		draw_rect(Rect2(Vector2.ZERO, size), DeckTheme.BACKGROUND)
		var cell: Vector2 = size / Vector2(columns, ceili(float(cards.size()) / columns))
		for index: int in range(cards.size()):
			var frame: Rect2 = Rect2(Vector2(index % columns, index / columns) * cell + Vector2(5, 5), cell - Vector2(10, 22))
			frames.append(frame)
			if compact:
				DeckCardPainter.paint(self, cards[index], frame)
			else:
				CardPainter.paint(self, cards[index], frame, "detail")
			draw_string(Typography.BODY, Vector2(frame.position.x, frame.end.y + 13), cards[index].number, HORIZONTAL_ALIGNMENT_LEFT, -1, 11, DeckTheme.TEXT)

var _failures: Array[String] = []
var _checks: int = 0

func _initialize() -> void:
	_run.call_deferred()

func _check(condition: bool, message: String) -> void:
	_checks += 1
	if not condition:
		_failures.append(message)
		push_error(message)

func _run() -> void:
	var arguments: PackedStringArray = OS.get_cmdline_user_args()
	var output: String = ""
	for index: int in range(arguments.size() - 1):
		if arguments[index] == "--output":
			output = arguments[index + 1]
	var app: WizardFrontend = (load("res://app/main.tscn") as PackedScene).instantiate() as WizardFrontend
	root.add_child(app)
	await process_frame
	await process_frame
	_check(app.service.error.is_empty(), "Formal catalogue loads: " + app.service.error)
	if not app.service.error.is_empty():
		quit(1)
		return
	var official_cards: int = app.service.cards.size()
	app.hide()
	ArtBank.trim_illustrations()
	var references: Dictionary[String, WeakRef] = {}
	for card: CardPresentation in app.service.cards:
		var id: String = "illustration." + card.art_id()
		_check(not ArtBank.metadata(id).is_empty(), "Published art mapping exists: " + id)
		var texture: Texture2D = ArtBank.texture(id)
		_check(texture != null and texture != ArtBank._fallback, "Published art loads without fallback: " + id)
		references[id] = weakref(texture)
	for id: String in references:
		_check(references[id].get_ref() != null, "Same-page texture survives more than 48 illustrations: " + id)
	root.size = Vector2i(1600, 1800)
	var canvas: CatalogueCanvas = CatalogueCanvas.new()
	canvas.cards.assign(app.service.cards)
	canvas.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	root.add_child(canvas)
	for compact: bool in [false, true, false, true]:
		canvas.compact = compact
		canvas.queue_redraw()
		await process_frame
		await process_frame
		_check(canvas.frames.size() == app.service.cards.size(), "Every formal card receives a rendered frame")
		for id: String in references:
			_check(ArtBank.texture(id) == references[id].get_ref(), "Renderer switches preserve texture identity: " + id)
		if not output.is_empty() and DisplayServer.get_name() != "headless":
			DirAccess.make_dir_recursive_absolute(output)
			await RenderingServer.frame_post_draw
			var image: Image = root.get_texture().get_image()
			_check(image.save_png(output.path_join("official-art-" + ("deck" if compact else "library") + ".png")) == OK, "All-card screenshot saves")
	canvas.queue_free()
	app.queue_free()
	await process_frame
	print(JSON.stringify({"checks": _checks, "failures": _failures, "renderer": DisplayServer.get_name(), "cards": official_cards, "uniqueIllustrations": references.size()}))
	quit(0 if _failures.is_empty() else 1)
