extends SceneTree

const K = KernelContract.CommandKind
const F = KernelContract.ActionField
var app: WizardFrontend
var service: FrontendService
var page: MatchPage
var failures: Array[String] = []
var evidence: Dictionary = {"seed": 42}

func _initialize() -> void:
	_run.call_deferred()

func check(ok: bool, message: String) -> bool:
	if not ok:
		failures.append(message)
		push_error(message)
	return ok

func view(player: int) -> Dictionary:
	return service.match_request(&"snapshot", [player]).get("data", {})

func submit(snapshot: Dictionary, command: Dictionary) -> bool:
	var result: Dictionary = service.match_request(&"submit_command", [snapshot.viewer, command, snapshot.generation, snapshot.revision])
	return check(result.get("ok", false), "Native command %s: %s" % [command, result.get("error", "")])

func query(snapshot: Dictionary, source: String, operation: int) -> Dictionary:
	var intent: Dictionary = {"source": source, "selections": [{"field": F.Operation, "value": str(operation)}], "limit": 64}
	for step: int in range(12):
		var result: Dictionary = service.match_request(&"query_actions", [snapshot.viewer, intent, snapshot.generation, snapshot.revision])
		var data: Dictionary = result.get("data", {})
		if data.get("complete") is Dictionary:
			return data.complete
		if data.get("options", []).is_empty():
			return {}
		intent.selections.append({"field": data.field, "value": data.options[0].value})
	return {}

func find_card(snapshot: Dictionary, id: String) -> Dictionary:
	for card: Dictionary in snapshot.cards:
		if str(card.id) == id:
			return card
	return {}

func reach_main() -> Dictionary:
	for step: int in range(240):
		var s: Dictionary = view(0)
		var actor: int = int(s.actingPlayer)
		if actor == 1:
			s = view(1)
		var command: Dictionary = {}
		if s.opening:
			if s.openingStage == "gestures":
				command = {"type": K.SubmitOpeningGesture, "round": str(s.openingRound), "gesture": "0" if actor == 0 else "2"}
			else:
				command = query(s, "0", K.SelectInitialFormation)
		elif actor == 0 and s.phase == 2 and s.decision == null:
			for card: Dictionary in s.cards:
				if card.get("definition", {}).get("id", "") != "v260-formation-0010" or card.owner != actor:
					continue
				if card.zone == 3:
					evidence["host"] = str(card.id)
					return s
				if card.zone == 9:
					command = query(s, str(card.id), K.SetFormation)
					if not command.is_empty():
						evidence["upgradeManaBefore"] = int(s.players[0].mana)
						break
		if command.is_empty() and s.actionSummary.get("safeAdvance") is Dictionary:
			command = s.actionSummary.safeAdvance
		if command.is_empty() and s.decision is Dictionary:
			command = query(s, "0", K.Choose)
		if not check(not command.is_empty(), "Missing legal stage advance at step %s, actor %s, phase %s, decision %s" % [step, actor, s.phase, s.decision]):
			return {}
		if not submit(s, command):
			return {}
	check(false, "Paid upgrade did not reach a two-ring formation")
	return {}

func stage_ring(card: CardPresentation, host: String) -> bool:
	page._select_card(card)
	if not check(page._selection.current() and page._selection.options.field == F.Operation, "Card exposes operations"):
		return false
	page._selection.choose(str(K.SetAmbush))
	if not check(page.canvas.target_rings.get(host, []).has(0) and page.canvas.target_rings.get(host, []).has(1), "Both rings are selectable"):
		return false
	var found: bool = false
	for hit: Dictionary in page.canvas._ring_hits:
		if str(hit.host) != host or int(hit.ring) != 1:
			continue
		var point: Vector2 = page.canvas.get_global_transform_with_canvas() * (hit.rect as Rect2).get_center()
		for pressed: bool in [true, false]:
			var event: InputEventMouseButton = InputEventMouseButton.new()
			event.button_index = MOUSE_BUTTON_LEFT
			event.position = point
			event.global_position = point
			event.pressed = pressed
			root.push_input(event, true)
		found = true
		break
	if not check(found, "Second-ring center is drawn and available to pointer input"):
		return false
	return check(page._confirm.visible and page._pending.get("command", {}).get("ringIndex", -1) == 1, "Second ring reaches final confirmation")

func _run() -> void:
	app = (load("res://app/main.tscn") as PackedScene).instantiate() as WizardFrontend
	root.add_child(app)
	await process_frame
	service = app.service
	if not check(service.start_match({"mode": "hotseat", "seed": 42}), service.error):
		_finish()
		return
	var start: Dictionary = reach_main()
	if start.is_empty():
		_finish()
		return
	var host: String = evidence.host
	app.navigate(&"match")
	page = app.page as MatchPage
	await process_frame
	if page._privacy_layer.visible:
		page._reveal()
	await process_frame
	await process_frame
	service.poll_session()
	var prior_text: String = page._logs_text.get_parsed_text()
	check(not prior_text.is_empty(), "The active player sees the real opening history")
	page._cover(-1)
	check(page._logs_text.get_parsed_text().is_empty(), "The handoff cover conceals the previous log")
	page._refresh()
	check(page._logs_text.get_parsed_text() == prior_text, "If the same viewer continues, previous history is immediately restored")
	await process_frame
	await process_frame
	var card: CardPresentation
	for candidate: CardPresentation in page.canvas.field.hand:
		if not query(start, candidate.instance_id, K.SetAmbush).is_empty():
			card = candidate
			break
	if not check(card != null, "A real hand card can be set face down") or not stage_ring(card, host):
		_finish()
		return
	var id: String = card.instance_id
	evidence["card"] = id
	evidence["name"] = card.title
	evidence["selectedRingIndex"] = 1
	evidence["manaBefore"] = int(start.players[0].mana)
	page._confirm.get_node("%Cancel").pressed.emit()
	var canceled: Dictionary = view(0)
	check(canceled.revision == start.revision and canceled.players[0].mana == start.players[0].mana and find_card(canceled, id).zone == 1 and find_card(canceled, host) == find_card(start, host), "Cancel leaves revision, mana and hand intact")
	if not stage_ring(card, host):
		_finish()
		return
	page._confirm.get_node("%Confirm").pressed.emit()
	var owner: Dictionary = view(0)
	var other: Dictionary = view(1)
	if page._privacy_layer.visible and page._pending_viewer == 1:
		service.poll_session()
		check(page._logs_text.get_parsed_text().is_empty(), "New events cannot repopulate the covered history")
		page._reveal()
		service.poll_session()
		var opponent_text: String = page._logs_text.get_parsed_text()
		check(service.event_log.viewer == 1 and opponent_text.contains("盖伏法术") and not opponent_text.contains(card.title), "Opponent handoff reveals only the native seat-one history")
	else:
		check(false, "The real ambush prompts a privacy handoff to the opponent")
	var own_card: Dictionary = find_card(owner, id)
	var hidden: Dictionary = find_card(other, id)
	check(own_card.get("zone") == 3 and str(own_card.get("host")) == host and own_card.get("ringIndex") == 1 and own_card.get("faceDown"), "Owner sees the card at the selected actual second ring")
	check(hidden.get("hidden", false) and not hidden.has("definition") and str(hidden.get("host")) == host and hidden.get("ringIndex") == 1, "Opponent sees occupied ring and no private card identity")
	evidence["manaAfter"] = int(owner.players[0].mana)
	check(owner.players[0].mana == start.players[0].mana and evidence.upgradeManaBefore - start.players[0].mana == 2, "Ambush costs zero and the formation upgrade was payable")
	var log: Dictionary = service.match_request(&"events_since", [1, "0", "0"])
	check(log.get("ok", false) and not log.data.events.is_empty() and log.data.events.any(func(event: Dictionary) -> bool: return event.kind == "ambush") and not JSON.stringify(log.data.events).contains(card.title), "Opponent log withholds the set card name")
	page._save_replay()
	page._replay_name.text = "盖伏环位-" + str(Time.get_ticks_usec())
	page._write_named_replay()
	var record: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(service.last_replay_path))
	var placement_step: int = record.commands.size()
	evidence["placementStep"] = placement_step
	check(int(record.commands.back().command.type) == K.SetAmbush and int(record.commands.back().command.ringIndex) == 1, "Recorded command uses selected index")
	var acting: Dictionary = view(int(owner.actingPlayer))
	if not submit(acting, {"type": K.Surrender}):
		_finish()
		return
	service.poll_session()
	page._refresh()
	check(page._settlement.visible and page._settlement.dialog_text.contains("投降"), "Authoritative loss reason appears in the settlement")
	page._open_named_save(true)
	page._replay_name.text = "终局-" + str(Time.get_ticks_usec())
	page._write_named_replay()
	page._settlement.hide()
	page._open_replay()
	await process_frame
	var replay: ReplayPage = app.page as ReplayPage
	if not check(replay != null and replay._rows.size() == placement_step + 1, "Completed game opens from match menu in replay: page=%s count=%s notice=%s last=%s error=%s" % [app.current_page, replay._rows.size() if replay else -1, replay._notice.text if replay else page._notice.text, service.last_replay_path, service.error]):
		_finish()
		return
	for viewer: int in [0, 1]:
		replay._viewer.select(viewer)
		replay._viewer.item_selected.emit(viewer)
		replay._seek(placement_step - 1)
		replay._manual_move(1)
		var expected_display: Array = [replay._frame.duplicate(true), replay._logs_text.text, replay._command.text]
		replay._seek(placement_step - 1)
		replay._change_speed(2)
		replay._toggle_playback()
		replay._timer.timeout.emit()
		check([replay._frame, replay._logs_text.text, replay._command.text] == expected_display, "4× and manual stepping show the same authoritative projection")
		replay._seek(replay._rows.size())
		replay._begin_seek()
		replay._progress.set_value_no_signal(placement_step)
		replay._end_seek(true)
		check(replay._timer.is_stopped() and [replay._frame, replay._logs_text.text, replay._command.text] == expected_display, "Dragging backwards pauses and shows the same projection, log and command")
		var replayed: Dictionary = find_card(replay._frame, id)
		check(replayed == (own_card if viewer == 0 else hidden), "Replay frame matches exact live card projection, player " + str(viewer))
		check(replay._frame.players[0].mana == owner.players[0].mana, "Replay mana matches live mana")
	replay._toggle_hand()
	replay._move(1)
	check(replay._canvas.hand_collapsed, "Advancing a replay preserves the collapsed hand")
	replay._move(-1)
	check(replay._canvas.hand_collapsed, "Stepping backward also keeps the selected hand display")
	check(replay._status_own.visible and replay._status_opponent.visible and not replay._table.clock.text.is_empty(), "A valid replay shows both status bars and clock")
	var incompatible: String = service.user_directory.path_join("incompatible-version-test.json")
	var test_record: FileAccess = FileAccess.open(incompatible, FileAccess.WRITE)
	record.programVersion = "2.6.3"
	test_record.store_string(JSON.stringify(record))
	test_record.close()
	replay._paths.append(incompatible)
	replay._files.add_item("incompatible-version-test.json")
	replay._files.select(replay._paths.size() - 1)
	replay._load()
	check(replay._notice.text.contains("复盘读取失败") and replay._path.is_empty(), "The selected incompatible recording reports load failure")
	check(not replay._status_own.visible and not replay._status_opponent.visible and replay._table.clock.text.is_empty(), "An incompatible replay clears the previous player statistics and clock")
	evidence["replay"] = service.last_replay_path
	evidence["opponentSeesIdentity"] = hidden.has("definition")
	evidence["cancelRevision"] = canceled.revision
	evidence["startRevision"] = start.revision
	await exercise_tutorial_chain()
	if failures.is_empty():
		await exercise_overflow()
	_finish()

func exercise_tutorial_chain() -> void:
	if not check(service.leave_match(), "Can finish the previous hotseat session"):
		return
	app.navigate(&"guide")
	await process_frame
	(app.page as GuidePage).lesson_requested.emit("lesson-07-chain")
	await process_frame
	var lesson_page: MatchPage = app.page as MatchPage
	# A4 jointly exercises the real tutorial entry, short ordered deck,
	# restricted debug surface, course AI, result panel and progress file.
	var opening := view(0)
	var hand: Array[String] = []
	for card: Dictionary in opening.cards:
		if card.owner == 0 and card.zone == 1:
			hand.append(str(card.definition.id))
	check(hand == ["v260-analytic-0006", "v260-word-0021", "v260-word-0021", "v260-word-0021", "v260-word-0021"], "A4 fixed teaching hand from the short main deck")
	check(int(opening.players[0].deckCount) == 5 and not opening.debug.available, "A4 accepts a short course and disables debug")
	var links: Array = []
	var shield_cost: int = -1
	var complete: bool = false
	for step: int in range(160):
		var s: Dictionary = view(0)
		if not check(s.get("tutorial") is Dictionary, "Tutorial page starts the chain lesson: " + service.error):
			return
		if s.tutorial.complete:
			complete = true
			break
		if s.tutorial.canContinue:
			var next := service.match_request(&"continue_tutorial", [s.generation, s.revision])
			if not check(next.get("ok", false), "Tutorial explanation advances"):
				return
			continue
		if s.aiTurn:
			var ai: Dictionary = service.match_request(&"step_ai", [s.generation, s.revision])
			if not check(ai.get("ok", false), "Tutorial opponent submits its legal command"):
				return
		else:
			var cmd: Dictionary = {}
			for op: int in [K.FlipAmbush, K.StartAnalysis, K.PrepareCast, K.ActivateSpell, K.Choose, K.PassResponse, K.AdvancePhase]:
				cmd = query(s, "0", op)
				if cmd.is_empty() and op in [K.FlipAmbush, K.StartAnalysis, K.PrepareCast, K.ActivateSpell]:
					for card: Dictionary in s.cards:
						if int(card.get("owner", -1)) == 0 and not bool(card.get("hidden", false)):
							cmd = query(s, str(card.id), op)
							if not cmd.is_empty():
								break
				if not cmd.is_empty():
					break
			if not check(not cmd.is_empty(), "Tutorial has a legal learner action") or not submit(s, cmd):
				return
			if int(cmd.type) in [K.Respond, K.FlipAmbush]:
				links = view(0).chain.links.duplicate(true)
				lesson_page._update_chain(view(0))
				var chain_text: String = lesson_page._table.chain_text.get_parsed_text()
				check(chain_text.contains("C1") and chain_text.contains("C2"), "MatchPage shows both live links for the shield response")
				evidence["responseChain"] = chain_text
				shield_cost = int(s.players[0].mana) - int(view(0).players[0].mana)

	check(complete and links.size() == 2 and shield_cost == 3, "Tutorial resolves both links after the paid shield response")
	if not complete:
		return
	var events: Array = service.match_request(&"events_since", [0, "0", "0"]).get("data", {}).get("events", [])
	var resolution: Array[int] = []
	var resolution_ids: Array[String] = []
	if links.size() == 2:
		for event: Dictionary in events:
			if event.kind == "link_resolved" and str(event.card) in [str(links[0].source), str(links[1].source)]:
				resolution.append(int(event.chainIndex))
				resolution_ids.append(str(event.card))
	check(resolution.slice(-2) == [2, 1] and resolution_ids.slice(-2) == [str(links[1].source), str(links[0].source)], "Shield C2 resolves before Fireball C1")

	service.poll_session()
	check(lesson_page._lesson_end.visible and not lesson_page._settlement.visible, "A4 course outcome replaces the normal settlement")
	check(GuideProgress.completed(service.user_directory).has("lesson-07-chain") and FileAccess.file_exists(service.user_directory.path_join("guide-progress.json")), "A4 completion is recorded in the local guide progress file")
	lesson_page._save_replay()
	lesson_page._replay_name.text = "教学-" + str(Time.get_ticks_usec())
	lesson_page._write_named_replay()
	evidence["tutorialReplay"] = service.last_replay_path
	evidence["shieldCost"] = shield_cost
	evidence["resolution"] = resolution.slice(-2)
	evidence["resolvedCards"] = resolution_ids.slice(-2)
	evidence["protectedLife"] = view(0).players[0].life
	check(evidence.protectedLife == 1, "Shield reduces seven fire damage to four")
	service.poll_session()
	check(service.save_replay("教学终局-" + str(Time.get_ticks_usec())), "Finished tutorial can be saved explicitly")
	lesson_page._open_replay()
	await process_frame
	var replay: ReplayPage = app.page as ReplayPage
	if not check(replay != null, "Ended tutorial opens through the match replay control"):
		return
	var found_reverse := false
	for index in range(replay._rows.size()):
		replay._move(1)
		if replay._table.chain_text.get_parsed_text().contains("C2 → C1"):
			found_reverse = true
			break
	check(found_reverse, "Replay displays the same multi-link resolution order")
	evidence["tutorialReplay"] = service.last_replay_path

func exercise_overflow() -> void:
	if not check(service.leave_match(), "Tutorial session closes cleanly"):
		return
	app.navigate(&"menu")
	await process_frame
	var options: Dictionary = {"mode": "hotseat", "seed": 42}
	var player: Dictionary = {"cards": [], "sideDeck": []}
	for index: int in range(40):
		player.cards.append("v260-word-%04d" % (1 + floori(index / 3.0)))
	for index: int in range(1, 11):
		player.sideDeck.append("v260-formation-%04d" % index)
	options["players"] = [player, player]
	if not check(service.start_match(options), "Legal 40+10 word deck starts"):
		return
	for step: int in range(240):
		var s: Dictionary = view(0)
		if s.actingPlayer == 1:
			s = view(1)
		var cmd: Dictionary = {}
		if s.opening:
			if s.openingStage == "gestures":
				cmd = {"type": K.SubmitOpeningGesture, "round": str(s.openingRound), "gesture": "0" if s.viewer == 0 else "2"}
			else:
				cmd = query(s, "0", K.SelectInitialFormation)
		elif s.decision is Dictionary and s.decision.kind == KernelContract.DecisionKind.Overflow:
			check(s.chain == null and s.players[0].castingCapacity == 0 and s.players[0].castingCount == 1, "Forced overflow after the last formation is removed and its chain has closed")
			app.navigate(&"match")
			await process_frame
			var overflow_page: MatchPage = app.page as MatchPage
			if overflow_page._privacy_layer.visible:
				overflow_page._reveal()
			var panel: DecisionPanel = overflow_page._decision
			if not check(panel.visible and panel.request != null and panel.request.choices.size() == 1 and overflow_page._end_phase.disabled, "MatchPage shows the forced choice and blocks phase advance"):
				return
			var choice: DecisionChoice = panel.request.choices[0]
			var card_id: String = str(choice.card.instance_id)
			overflow_page._decision_confirmed(panel.request.generation, panel.request.revision, panel.request.decision_id, [choice.id])
			var done: Dictionary = view(0)
			check(find_card(done, card_id).zone == 6 and done.players[0].castingCount == 0, "Chosen excess spell leaves the casting area for ash")
			overflow_page._save_replay()
			overflow_page._replay_name.text = "环位超额-" + str(Time.get_ticks_usec())
			overflow_page._write_named_replay()
			evidence["overflow"] = {"playedCard": card_id, "capacity": s.players[0].castingCapacity, "before": s.players[0].castingCount, "after": done.players[0].castingCount, "replay": service.last_replay_path}
			return
		elif s.viewer == 0 and s.phase == 2 and s.decision == null:
			var operation: int = K.RemoveFormation if int(s.players[0].castingCount) > 0 else K.PreloadWord
			cmd = query(s, "0", operation)
		if cmd.is_empty() and s.actionSummary.get("safeAdvance") is Dictionary:
			cmd = s.actionSummary.safeAdvance
		if cmd.is_empty():
			cmd = query(s, "0", K.Choose)
		if not check(not cmd.is_empty(), "Full match can progress toward capacity change") or not submit(s, cmd):
			return
	check(false, "Full hotseat game reached overflow within the action limit")

func _finish() -> void:
	evidence["failures"] = failures
	var args: PackedStringArray = OS.get_cmdline_user_args()
	var index: int = args.find("--evidence")
	if index >= 0 and index + 1 < args.size():
		var output: FileAccess = FileAccess.open(args[index + 1], FileAccess.WRITE)
		if check(output != null, "Evidence output is writable"):
			output.store_string(JSON.stringify(evidence, "\t"))
			output.close()
	print("V264_MATCH_ACCEPTANCE ", JSON.stringify(evidence))
	quit(0 if failures.is_empty() else 1)
