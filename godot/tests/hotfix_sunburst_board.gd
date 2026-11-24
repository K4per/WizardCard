extends "res://tests/hotfix_interactions.gd"

const HandInput = preload("res://tests/hand_input.gd")
const SUN: String = "v260-analytic-0009"
const FUEL: String = "v260-analytic-0001"

func count_ash(s: Dictionary) -> int:
	var n: int = 0
	for item: Dictionary in s.cards:
		if item.owner == 0 and item.zone == 6 and str(item.get("definition",{}).get("school","")) == "evocation":
			n += 1
	return n

func ensure_hand(s: Dictionary, definition: String) -> Dictionary:
	var in_hand: Dictionary = card(s,definition,1)
	if in_hand.is_empty() and debug_op("add_card",{"cardId":definition}):
		in_hand = card(view(),definition,1)
	return in_hand

func progress(s: Dictionary) -> bool:
	if s.aiTurn:
		return check(service.match_request(&"step_ai", [s.generation,s.revision]).get("ok",false),"AI advances")
	var cmd: Dictionary = {}
	if s.opening:
		cmd = query(s,"0",K.SubmitOpeningGesture if s.openingStage == "gestures" else K.SelectInitialFormation)
		if s.openingStage == "gestures":
			cmd.gesture = str((int(s.openingRound)-1)%3)
	elif s.actionSummary.get("safeAdvance") is Dictionary:
		cmd = s.actionSummary.safeAdvance
	elif s.decision is Dictionary:
		cmd = query(s,"0",K.Choose)
	return check(not cmd.is_empty(), "Native setup can continue at "+str(s.decision)) and submit(s,cmd)

func advance_main() -> Dictionary:
	for step: int in range(100):
		var s: Dictionary = view()
		if not s.opening and not s.aiTurn and s.phase == 2 and s.decision == null:
			return s
		if not progress(s):
			return {}
	check(false,"Main phase is reachable")
	return {}

func setup_command(s: Dictionary, id: String, op: int) -> bool:
	var cmd: Dictionary = query(s,id,op)
	return check(not cmd.is_empty(),"Missing operation "+str(op)+" for "+id) and submit(s,cmd)

func ash_one() -> bool:
	var s: Dictionary = advance_main()
	var fuel: Dictionary = ensure_hand(s,FUEL)
	if fuel.is_empty() or not setup_command(view(),str(fuel.id),K.SetAmbush):
		return false
	s = advance_main()
	return setup_command(s,str(fuel.id),K.Abandon)

func find_id(s: Dictionary,id: String) -> Dictionary:
	for item: Dictionary in s.cards:
		if str(item.id) == id:
			return item
	return {}

func inspect_board() -> void:
	if not start("ai"):
		return
	var s: Dictionary = advance_main()
	if s.is_empty() or not debug_op("gain_mana",{"amount":40}) or not debug_op("set_infinite_load",{"enabled":true}):
		return
	for i: int in range(5):
		var rune: Dictionary = ensure_hand(advance_main(),"v260-seal-0003")
		if rune.is_empty() or not setup_command(view(),str(rune.id),K.AttachSeal):
			return
	for i: int in range(3):
		if not ash_one():
			return
	s = advance_main()
	if not check(count_ash(s) == 3, "Exactly three evocations in ash before setting"):
		return
	var first: Dictionary = ensure_hand(s,SUN)
	if first.is_empty() or not debug_op("gain_mana",{"amount":40}):
		return
	s = view()
	var mana: int = int(s.players[0].mana)
	if not setup_command(s,str(first.id),K.StartAnalysis):
		return
	s = advance_main()
	var paid: int = mana-int(s.players[0].mana)
	if not check(paid == 17 and int(find_id(s,str(first.id)).effectiveSettingCost)==17,"First instance paid and displayed 17"):
		return
	evidence.append({"step":"setting","ash":3,"first":first.id,"manaBefore":mana,"manaAfter":s.players[0].mana,"paid":paid,"revision":s.revision})
	if not ash_one():
		return
	var other: Dictionary = ensure_hand(advance_main(),SUN)
	if other.is_empty():
		return
	s = view()
	var first_id: String = str(first.id)
	var other_id: String = str(other.id)
	# Free the fan so the second copy's printed fee is legible alongside the
	# fixed detail panel for the first copy, in one unmodified game screenshot.
	for occupant: Dictionary in s.cards:
		if occupant.owner == 0 and occupant.zone == 1 and str(occupant.id) != other_id:
			if not setup_command(advance_main(),str(occupant.id),K.SetAmbush):
				return
	s = advance_main()
	if not check(first_id != other_id and count_ash(s) == 4, "Second instance in hand with four evocations in ash"):
		return
	var field_card: Dictionary = find_id(s,first_id)
	var hand_card: Dictionary = find_id(s,other_id)
	check(field_card.effectiveSettingCost==17 and hand_card.effectiveSettingCost==16,"Native viewer reports locked 17 and fresh 16 in the very same frame")
	service.response_modes[0] = ResponseTiming.Mode.IGNORE
	await show_match()
	root.size = Vector2i(1920,1080)
	await settle()
	for id: String in [first_id,other_id]:
		var actual: CardPresentation = MatchProjection.card(service.match_snapshot,id)
		var bounds: Rect2 = page.canvas.card_bounds(actual)
		var point: Vector2 = page.canvas.global_position + bounds.get_center()
		if id == other_id:
			actual = page.canvas.field.hand[0]
			point = HandInput.point(page.canvas, actual)
			var motion: InputEventMouseMotion = InputEventMouseMotion.new()
			motion.position = point
			root.push_input(motion, true)
			await create_timer(0.2).timeout
			await settle()
			bounds = page.canvas.card_bounds(actual)
			check(Rect2(Vector2.ZERO, page.canvas.size).encloses(bounds), "Hovered hand card is fully visible before detail selection")
			point = page.canvas.global_position + bounds.get_center()
		await click(point)
		if page._action_popup.visible:
			page._action_popup.hide()
		await settle()
		var expected: String = "设置费用：%d" % (17 if id == first_id else 16)
		check(page._detail.card.instance_id == id and page._detail.card.cost_detail().contains(expected), "Live detail shows "+expected)
		await shot("sunburst-board-17" if id == first_id else "sunburst-hand-16")
	check(service.match_snapshot.revision == s.revision, "Two detail views are from the same authoritative revision")
	evidence.append({"step":"same view","revision":s.revision,"ash":count_ash(s),"boardId":first_id,"boardFee":field_card.effectiveSettingCost,"handId":other_id,"handFee":hand_card.effectiveSettingCost})
	if not check(service.save_replay(),service.error):
		return
	var record: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(service.last_replay_path))
	var visible: Dictionary = service.match_request(&"replay_frame",[service.last_replay_path,record.commands.size(),1]).get("data",{})
	if not check(not visible.is_empty(),"Replay projects the same step for the opponent"):
		return
	check(find_id(visible,first_id).get("effectiveSettingCost",-1)==17 and find_id(visible,other_id).is_empty(),"Opponent sees the face-up locked fee, the private hand instance is absent")
	app.navigate(&"menu")
	if not setup_command(view(),other_id,K.SetAmbush) or not check(service.save_replay(),service.error):
		return
	record = JSON.parse_string(FileAccess.get_file_as_string(service.last_replay_path))
	visible = service.match_request(&"replay_frame",[service.last_replay_path,record.commands.size(),1]).get("data",{})
	var covered: Dictionary = find_id(visible,other_id)
	check(covered.get("hidden",false) and not covered.has("definition") and not covered.has("effectiveSettingCost"),"Opponent cannot distinguish the second copy by its paid or preview cost after ambush")
	evidence.append({"step":"opponent replay","faceUpFee":find_id(visible,first_id).get("effectiveSettingCost",-1),"privateHandProjected":false,"coveredInstance":covered})

func _run() -> void:
	var args: PackedStringArray = OS.get_cmdline_user_args()
	var index: int = args.find("--output")
	if index >= 0:
		output = args[index+1]
		DirAccess.make_dir_recursive_absolute(output)
	app = load("res://app/main.tscn").instantiate() as WizardFrontend
	root.add_child(app)
	await settle()
	service = app.service
	service.settings.soundVolume = 0
	await inspect_board()
	print("SUNBURST ",JSON.stringify({"evidence":evidence,"errors":errors},"  "))
	if not output.is_empty():
		var file: FileAccess = FileAccess.open(output.path_join("sunburst.json"),FileAccess.WRITE)
		file.store_string(JSON.stringify({"evidence":evidence,"errors":errors},"\t")+"\n")
	app.queue_free()
	await process_frame
	await process_frame
	quit(0 if errors.is_empty() else 2)
