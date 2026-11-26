extends SceneTree

const HandInput = preload("res://tests/hand_input.gd")

# Runs the Windows viewport against real native commands. Movie mode records the
# unmodified presentation clock at 30 frames per second.
var service: FrontendService = FrontendService.new()
var errors: Array[String] = []
var output: String = ""
var evidence: Dictionary = {}

func _initialize() -> void:
	_run.call_deferred()

func check(ok: bool, label: String) -> bool:
	if not ok:
		errors.append(label)
		push_error(label)
	return ok

func frames(count: int = 2) -> void:
	for index: int in count:
		await process_frame

func submit(player: int, command: Dictionary) -> Dictionary:
	var snapshot: Dictionary = service.snapshot(player)
	return service.match_request(&"submit_command", [player, command, snapshot.generation, snapshot.revision])

func batch(viewer: int, cursor: Dictionary = {}) -> Dictionary:
	return service.match_request(&"events_since", [viewer, str(cursor.get("epoch","0")), str(cursor.get("next","0"))]).get("data", {})

func table_frame(table: Control, snapshot: Dictionary) -> void:
	table.canvas.configure(MatchProjection.field(snapshot))
	table.show_frame(snapshot)
	var viewer: int = int(snapshot.viewer)
	table.own.set_status("我方", viewer, snapshot.players[viewer])
	table.opponent.set_status("对手", 1 - viewer, snapshot.players[1 - viewer])

func screenshot(name_text: String) -> void:
	if output.is_empty() or DisplayServer.get_name() == "headless":
		return
	await RenderingServer.frame_post_draw
	check(root.get_texture().get_image().save_png(output.path_join(name_text + ".png")) == OK, "save " + name_text)

func enter_main() -> bool:
	for attempt: int in 40:
		var state: Dictionary = service.snapshot(0)
		var actor: int = int(state.actingPlayer)
		state = service.snapshot(actor)
		if int(state.phase) == 2 and state.decision == null and state.chain == null:
			return true
		var command: Dictionary = {}
		if state.decision is Dictionary and int(state.decision.kind) == KernelContract.DecisionKind.Response:
			command = {"type":13, "decision": state.decision.id}
		elif state.automaticCommand is Dictionary:
			command = state.automaticCommand
		elif str(state.phaseGate) != "0":
			command = {"type":14,"gate":state.phaseGate,"skipCast":false}
		else:
			return check(false, "unexpected before main " + str(state.decision))
		if not check(bool(submit(actor, command).get("ok")), "advance to main " + service.error):
			return false
	return check(false, "main phase timeout")

func field_routes(table: Control) -> void:
	var formations: Array[String] = []
	var cursor: Dictionary = batch(0)
	for player: int in [0, 1]:
		var view: Dictionary = service.snapshot(player)
		for row: Dictionary in view.cards:
			if int(row.zone) == 9 and int(row.definition.level) == 1 and int(row.owner) == player:
				formations.append(str(row.id))
				break
		if not check(formations.size() > player, "find actual legal opening formation"):
			return
		check(bool(submit(player, {"type":18,"card":formations[player]}).get("ok")), "confirm actual initial formation")
		if player == 0:
			check(batch(1).events.filter(func(ev: Dictionary) -> bool: return ev.get("kind") == "initial_formation").is_empty(), "first private choice has no public formation yet")
	var formation_moves: Array = batch(0,cursor).events.filter(func(ev: Dictionary) -> bool: return ev.get("kind") == "card_move" and int(ev.travel.from) == 9)
	check(formation_moves.size() == 2, "simultaneous public reveal moves both selected formations")
	evidence["formations"] = formation_moves
	if not await enter_main():
		return
	var phase_events: Array = batch(0,cursor).events
	var income: Array = phase_events.filter(func(ev: Dictionary) -> bool: return ev.get("kind") == "mana_gain")
	check(not income.is_empty() and int(income[0].actualAmount) > 0, "prepare income has actual positive mana delta")
	evidence["income"] = income
	var state: Dictionary = service.snapshot(0)
	var active: int = int(state.activePlayer)
	state = service.snapshot(active)
	table_frame(table, state)
	await frames()
	var played: String = ""
	cursor = batch(active)
	for row: Dictionary in state.cards:
		if int(row.zone) == 1 and int(row.owner) == active and int(row.definition.type) in [1,2,5]:
			var candidate: String = str(row.id)
			var action: Dictionary = {"type":15,"card":candidate,"formation":formations[active],"ringIndex":0}
			if bool(submit(active, action).get("ok")):
				played = candidate
				break
	if not check(not played.is_empty(), "set an actual hidden card"):
		return
	var moves: Array = batch(active,cursor).events.filter(func(ev: Dictionary) -> bool: return ev.get("kind") == "card_move" and str(ev.card) == played)
	if check(moves.size() == 1, "exactly one accepted hand move"):
		var move: Dictionary = moves[0].travel
		check(int(move.from) == 1 and int(move.to) == 3 and str(move.toHost) == formations[active] and bool(move.back), "cover travels to actual host ring face down")
		table_frame(table, service.snapshot(active))
		table.present_events(moves)
		await frames()
		check(table.canvas._flights.size() == 1, "accepted cover creates one in-flight card")
		evidence["ambush"] = moves
	if not await enter_main():
		return
	cursor = batch(active)
	check(bool(submit(active, {"type":8,"card":played}).get("ok")), "abandon occupied ring to ash")
	var ash: Array = batch(active,cursor).events.filter(func(ev: Dictionary) -> bool: return ev.get("kind") == "card_move" and str(ev.card) == played)
	if check(ash.size() == 1, "exactly one ash move"):
		check(int(ash[0].travel.from) == 3 and int(ash[0].travel.to) == 6 and str(ash[0].travel.fromHost) == formations[active], "source ring to owner's ash")
	evidence["ash"] = ash
	table_frame(table, service.snapshot(active))
	table.present_events(ash)
	table.clear_feedback()
	check(table.canvas._flights.is_empty(), "interrupt clears existing flights")
	UiFactory.reduced_motion = true
	table.present_events(ash)
	check(table.canvas._flights.is_empty(), "reduced motion keeps final board without card travel")
	table.present_events(income)
	check(not table.own._delta_badges.is_empty() or not table.opponent._delta_badges.is_empty(), "reduced motion retains signed actual resource results")
	UiFactory.reduced_motion = false

func _run() -> void:
	var args: PackedStringArray = OS.get_cmdline_user_args()
	var data: String = ProjectSettings.globalize_path("user://hotfix-visuals")
	var assets: String = ProjectSettings.globalize_path("res://../assets/release-v260")
	for index: int in range(args.size() - 1):
		if args[index] == "--output":
			output = args[index + 1]
			DirAccess.make_dir_recursive_absolute(output)
		if args[index] == "--user-data":
			data = args[index + 1]
		if args[index] == "--assets":
			assets = args[index + 1]
	if not check(service.initialize(assets, data), service.error):
		quit(1)
		return
	root.size = Vector2i(1280, 720)
	if not check(service.start_match({"mode": "tutorial", "lessonId": "lesson-05-runes"}), service.error):
		quit(1)
		return
	var page: MatchPage = load("res://pages/match_page.tscn").instantiate()
	root.add_child(page)
	page.configure(service)
	await frames(6)
	var canvas: FieldCanvas = page.canvas
	var card: CardPresentation = canvas.field.hand[0]
	var initial: Rect2 = canvas.card_bounds(card)
	var motion: InputEventMouseMotion = InputEventMouseMotion.new()
	motion.position = HandInput.point(canvas, card)
	var marker: Polygon2D = Polygon2D.new()
	marker.polygon = PackedVector2Array([Vector2.ZERO, Vector2(3, 17), Vector2(6, 11), Vector2(13, 10)])
	marker.color = Color.WHITE
	marker.z_index = 100
	page.add_child(marker)
	marker.global_position = motion.position
	marker.hide()
	await create_timer(0.4).timeout
	root.push_input(motion, true)
	marker.show()
	await create_timer(0.26).timeout
	var held: Rect2 = canvas.card_bounds(card)
	check(canvas._hover_id == card.instance_id and held.get_center().y < initial.get_center().y, "hover lifts actual hand card")
	var samples: Array = []
	for mode: int in [ResponseTiming.Mode.ALL, ResponseTiming.Mode.IGNORE, ResponseTiming.Mode.AUTO]:
		page._response_mode.select(mode)
		page._response_mode.item_selected.emit(mode)
		page._refresh()
		await frames(2)
		var bounds: Rect2 = canvas.card_bounds(card)
		check(canvas._hover_id == card.instance_id and bounds.is_equal_approx(held), "stationary hover holds through mode %d" % mode)
		samples.append({"mode":mode,"y":bounds.get_center().y,"hover":canvas._hover_id})
		await create_timer(0.48).timeout
	evidence["hover"] = {"instance":card.instance_id,"beforeY":initial.get_center().y,"samples":samples}
	page.queue_free()
	await frames()
	service.surrender_match()
	service.leave_match()
	if not check(service.start_match({"mode":"hotseat","seed":73}), service.error):
		quit(1)
		return
	check(bool(submit(0, {"type":21,"round":"1","gesture":"0"}).get("ok")), "first private gesture")
	var previous: Array[Dictionary] = [service.snapshot(0), service.snapshot(1)]
	var cursors: Array[Dictionary] = [batch(0),batch(1)]
	check(bool(submit(1, {"type":21,"round":"1","gesture":"2"}).get("ok")), "second gesture confirms opening draw")
	var table: Control = load("res://ui/battle_table.tscn").instantiate()
	root.add_child(table)
	var views: Array = []
	for viewer: int in [0, 1]:
		table.clear_feedback()
		table_frame(table, previous[viewer])
		await frames()
		var snapshot: Dictionary = service.snapshot(viewer)
		var update: Dictionary = batch(viewer, cursors[viewer])
		table_frame(table, snapshot)
		var sample: Dictionary = {}
		var draw_order: Array = []
		var flight_order: Array = []
		var own_count: int = 0
		var other_count: int = 0
		for event: Dictionary in update.get("events", []):
			if event.get("kind") == "draw":
				draw_order.append(str(event.card))
			if event.get("kind") != "card_move":
				continue
			var move: Dictionary = event.travel
			check(int(move.from) == 0 and int(move.to) == 1 and int(move.fromPlayer) == int(move.toPlayer), "opening moves from deck to matching hand")
			if int(move.toPlayer) == viewer:
				own_count += 1
				flight_order.append(str(event.card))
				check(str(event.card) != "0" and not bool(move.back), "drawer may see own drawn identity")
			else:
				other_count += 1
				check(str(event.card) == "0" and bool(move.back), "opponent gets only anonymous back")
			if int(move.toPlayer) == 0 and sample.is_empty():
				sample = event
		check(draw_order == flight_order, "flights preserve actual order of drawing from shuffled deck")
		check(own_count == 5 and other_count == 5, "both opening draws produce five viewer-permitted flights")
		table.present_events([sample] if not sample.is_empty() else [])
		await create_timer(0.17).timeout
		if check(not table.canvas._flights.is_empty(), "in-flight pose present during native draw"):
			var flight: Dictionary = table.canvas._flights[0]
			check(flight.card.hidden == (viewer == 1), "same draw paints owner face and opponent back")
			await screenshot("hidden-draw-viewer-%d" % (viewer + 1))
		views.append({"viewer":viewer,"ownDraws":own_count,"opponentDraws":other_count,"sample":sample})
		await create_timer(0.4).timeout
	evidence["openingDraws"] = views
	await field_routes(table)
	table.queue_free()
	await frames()
	if not output.is_empty():
		var report: FileAccess = FileAccess.open(output.path_join("visuals.json"), FileAccess.WRITE)
		report.store_string(JSON.stringify({"evidence":evidence,"failures":errors}, "\t"))
	print(JSON.stringify({"evidence":evidence,"failures":errors}))
	quit(0 if errors.is_empty() else 1)
