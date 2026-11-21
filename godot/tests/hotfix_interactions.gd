extends SceneTree

const K = KernelContract.CommandKind
const F = KernelContract.ActionField
var app: WizardFrontend
var page: MatchPage
var service: FrontendService
var errors: Array[String] = []
var evidence: Array[Dictionary] = []
var output: String = ""
var state_log: String = ""

func _initialize() -> void:
	_run.call_deferred()

func check(ok: bool, message: String) -> bool:
	if not ok:
		errors.append(message)
		push_error(message)
	return ok

func settle() -> void:
	await process_frame
	await process_frame
	if DisplayServer.get_name() != "headless":
		await RenderingServer.frame_post_draw

func click(point: Vector2) -> void:
	var move: InputEventMouseMotion = InputEventMouseMotion.new()
	move.position = point
	root.push_input(move, true)
	for pressed: bool in [true, false]:
		var event: InputEventMouseButton = InputEventMouseButton.new()
		event.position = point
		event.global_position = point
		event.button_index = MOUSE_BUTTON_LEFT
		event.pressed = pressed
		root.push_input(event, true)
	await settle()

func button(control: Control) -> void:
	await settle()
	await click(control.get_global_rect().get_center())

func key(code: Key) -> void:
	for pressed: bool in [true, false]:
		var event: InputEventKey = InputEventKey.new()
		event.keycode = code
		event.pressed = pressed
		root.push_input(event, true)
	await settle()

func shot(filename: String) -> void:
	if not output.is_empty() and DisplayServer.get_name() != "headless":
		await RenderingServer.frame_post_draw
		check(root.get_texture().get_image().save_png(output.path_join(filename + ".png")) == OK, "save " + filename)

func mode(index: int) -> void:
	page._response_mode.select(index)
	page._response_mode.item_selected.emit(index)
	await settle()

func start(mode_name: String) -> bool:
	app.navigate(&"menu")
	if service.match_active:
		service.leave_match()
	if not check(service.start_match({"mode":mode_name, "seed":42}), service.error):
		return false
	return true

func show_match() -> void:
	app.navigate(&"match")
	page = app.page as MatchPage
	await settle()

func opening(mode_name: String) -> void:
	if not start(mode_name):
		return
	await show_match()
	if page._privacy_layer.visible:
		await button(page.get_node("%Reveal"))
	var s: Dictionary = service.match_snapshot
	check(s.openingStage == "gestures" and page._decision.visible, "Human starts with three gestures")
	var row: Dictionary = {"case":"B01 " + mode_name, "round":s.openingRound, "gesture":s.openingGesture, "revision":s.revision}
	await mode(ResponseTiming.Mode.IGNORE)
	row.afterIgnore = page._decision.visible
	await shot("opening-ignore-" + mode_name)
	check(page._decision.visible, "Switching to IGNORE retains this round's gesture chooser")
	await key(KEY_ESCAPE)
	row.afterEscape = page._decision.visible
	evidence.append(row)
	check(page._decision.visible and not page._match_menu.visible, "Escape returns to gestures")
	if not page._decision.visible:
		return
	await button(page._decision._options["0"])
	check(not page._decision._selected.is_empty(), "A gesture can be tentatively selected")
	await button(page._response_cancel)
	check(page._decision.visible and page._decision._selected.is_empty() and view().revision == s.revision, "Cancel immediately returns to this round without submitting")
	for preference: int in [ResponseTiming.Mode.ALL, ResponseTiming.Mode.AUTO, ResponseTiming.Mode.IGNORE]:
		await mode(preference)
		check(page._decision.visible and page._decision._options.size() == 3, "Mode cycling leaves all three gestures")
	var stages: Array = []
	for step: int in range(100):
		if page._privacy_layer.visible and page._pending_viewer >= 0:
			await button(page.get_node("%Reveal"))
		var frame: Dictionary = service.match_snapshot
		if not frame.opening:
			row.firstTurn = frame.globalTurn
			row.handCounts = [int(frame.players[0].handCount),int(frame.players[1].handCount)]
			break
		if page._decision.visible and page._decision.request.decision_id.begins_with("opening:"):
			var current: String = "%s/%s/%s" % [frame.openingStage,frame.openingRound,service.match_viewer]
			if not check(not stages.has(current),"Exactly one submission per seat and round: "+current):
				break
			stages.append(current)
			var option: Button
			if frame.openingStage == "gestures":
				await mode(ResponseTiming.Mode.AUTO)
				await key(KEY_ESCAPE)
				var gesture: int = 0 if mode_name == "hotseat" else (int(frame.openingRound)-1)%3
				if mode_name == "hotseat" and int(frame.openingRound) == 2 and service.match_viewer == 1:
					gesture = 2
				option = page._decision._options[str(gesture)]
			else:
				check(int(frame.players[0].handCount) == 5 and int(frame.players[1].handCount) == 5, "Both players draw five before formations")
				option = page._decision._options.values()[0]
			await button(option)
			await button(page._decision._confirm)
		else:
			await create_timer(0.06).timeout
	row.stages = stages
	check(row.has("firstTurn"), "Opening reaches the first turn")
	if mode_name == "hotseat":
		check(stages.has("gestures/2/0") and stages.has("gestures/2/1"), "Local tie reopens both private choices")

func debug_op(which: String, details: Dictionary) -> bool:
	details["operation"] = which
	details["player"] = 0
	var result: Dictionary = service.match_request(&"debug_command", [details])
	return check(bool(result.get("ok", false)), "%s: %s" % [which, result.get("error", "")])

func view() -> Dictionary:
	return service.match_request(&"snapshot", [0]).get("data", {})

func card(s: Dictionary, def: String, zone: int = -1) -> Dictionary:
	for item: Dictionary in s.cards:
		if int(item.owner) == 0 and str(item.get("definition", {}).get("id", "")) == def and (zone < 0 or int(item.zone) == zone):
			return item
	return {}

func query(s: Dictionary, source: String, operation: int, preferred_host: String = "") -> Dictionary:
	var intent: Dictionary = {"source":source, "selections":[{"field":F.Operation,"value":str(operation)}]}
	for step: int in range(16):
		var data: Dictionary = service.match_request(&"query_actions", [0, intent.duplicate(true), s.generation, s.revision]).get("data", {})
		if data.get("complete") is Dictionary:
			return data.complete
		if data.get("options", []).is_empty():
			return {}
		var choice: Dictionary = data.options[0]
		if int(data.field) == F.Host and not preferred_host.is_empty():
			for candidate: Dictionary in data.options:
				if str(candidate.value) == preferred_host:
					choice = candidate
		intent.selections.append({"field":data.field, "value":choice.value})
	return {}

func submit(s: Dictionary, command: Dictionary) -> bool:
	var r: Dictionary = service.match_request(&"submit_command", [0, command, s.generation, s.revision])
	return check(bool(r.get("ok", false)), "%s: %s" % [command,r.get("error","")])

func confirm_setting(s: Dictionary, spell: Dictionary, command: Dictionary) -> bool:
	await show_match()
	for attempt: int in range(2):
		page._select_card(MatchProjection.card(s, str(spell.id)))
		page._selection.choose(str(K.StartAnalysis))
		for step: int in range(12):
			if page._confirm.visible:
				break
			if not check(page._selection.options != null and not page._selection.options.options.is_empty(),"Card exposes the next placement choice"):
				return false
			page._selection.choose(str(page._selection.options.options[0].value))
		if not check(page._confirm.visible and page._pending.command == command, "Selected target and ring stage the exact native command"):
			return false
		var detail: String = page._confirm.dialog_text
		check(detail.contains("匕首之云") and detail.contains("基础法阵") and detail.contains("设置费用：2 魔素"), "Actual payment, source and host appear together")
		if attempt == 0:
			await shot("native-setting-quote")
			await key(KEY_ESCAPE)
			check(view().revision == s.revision and card(view(),"v260-analytic-0013",1).id == spell.id,"Escape does not pay or move the native instance")
		else:
			await button(page._confirm.get_node("%Confirm"))
	var after: Dictionary = view()
	if not check(int(after.players[0].mana) == int(s.players[0].mana)-2 and not card(after,"v260-analytic-0013",3).is_empty(), "Confirm commits the selected payment once"):
		return false
	evidence.append({"case":"O02 native action", "source":spell.id, "host":command.formation, "ring":command.ringIndex,"manaBefore":s.players[0].mana,"manaAfter":after.players[0].mana})
	app.navigate(&"menu")
	await settle()
	return true

# Reach an ordinary preparation decision using legal native commands. The built-in
# single-player debug console supplies a card, rank runes and deterministic mana.
func reach_upkeep(def: String, rank: int, fee: int) -> Dictionary:
	if not start("ai"):
		return {}
	var funded: bool = false
	var attached: int = 0
	var added: bool = false
	var mana_set: bool = false
	for step: int in range(650):
		var s: Dictionary = view()
		if not s.opening and not funded:
			if not debug_op("gain_mana", {"amount":40}):
				return {}
			funded = true
			continue
		var host: Dictionary = card(s, "v260-formation-0009", 3)
		var spell: Dictionary = card(s, def, 3)
		if spell.is_empty():
			spell = card(s, def, 5)
		if spell.is_empty():
			spell = card(s, def, 1)
		var row: String = "step=%d turn=%s phase=%s actor=%s decision=%s spell=%s host=%s" % [step,s.globalTurn,s.phase,s.actingPlayer,s.decision,spell.get("zone",-1),host.get("id","")]
		state_log = row
		if s.decision is Dictionary and int(s.decision.kind) == KernelContract.DecisionKind.Concentration:
			print("Stable preparation: ", row)
			return s
		if int(s.result) != -1:
			check(false,"Game ended before preparation: " + row)
			return {}
		var command: Dictionary = {}
		if not mana_set and not spell.is_empty() and int(spell.zone) == 3 and int(spell.get("spellState",-1)) == 4:
			var current: int = int(s.players[0].mana)
			if current != fee:
				if not debug_op("lose_mana" if current > fee else "gain_mana", {"amount":absi(current-fee)}):
					return {}
			mana_set = true
			continue
		if s.aiTurn:
			var r: Dictionary = service.match_request(&"step_ai", [s.generation, s.revision])
			if not check(bool(r.get("ok",false)), "AI: " + str(r)):
				return {}
			continue
		if s.opening:
			command = query(s,"0",K.SubmitOpeningGesture if s.openingStage=="gestures" else K.SelectInitialFormation)
			if s.openingStage=="gestures":
				command["gesture"] = str((int(s.openingRound)-1)%3)
		elif s.activePlayer == 0 and s.phase == 2 and s.decision == null and attached < rank - 1:
			var rune: Dictionary = card(s,"v260-seal-0003",1)
			if rune.is_empty():
				if not debug_op("add_card", {"cardId":"v260-seal-0003"}):
					return {}
				continue
			command = query(s,str(rune.id),K.AttachSeal,str(host.id))
			if not check(not command.is_empty(), "Cannot attach rank rune: " + row):
				return {}
			attached += 1
		elif not added and s.activePlayer == 0 and s.phase == 2 and s.decision == null:
			if spell.is_empty() and not debug_op("add_card", {"cardId":def}):
				return {}
			added = true
			continue
		elif added and not spell.is_empty():
			match int(spell.zone):
				1:
					command = query(s,str(spell.id),K.SetAmbush if def.contains("talisman") else K.StartAnalysis)
				3:
					for operation: int in [K.FlipAmbush,K.PrepareCast]:
						command = query(s,str(spell.id),operation)
						if not command.is_empty():
							break
				5:
					command = query(s,str(spell.id),K.ActivateSpell)
		if command.is_empty() and s.actionSummary.get("safeAdvance") is Dictionary:
			command = s.actionSummary.safeAdvance
		if command.is_empty() and s.decision is Dictionary:
			command = query(s,"0",K.Choose)
		if not check(not command.is_empty(), "No legal next move: " + row):
			return {}
		if def == "v260-analytic-0013" and int(command.type) == K.StartAnalysis:
			if not await confirm_setting(s,spell,command):
				return {}
		elif not submit(s,command):
			return {}
	check(false, "Preparation was not reached: " + state_log)
	return {}

func concentration(def: String, rank: int, fee: int, abandon: bool = false, short_mana: bool = false) -> void:
	var s: Dictionary = await reach_upkeep(def, rank, fee - 1 if short_mana else fee)
	if s.is_empty():
		return
	await show_match()
	var id: String = str(s.decision.source)
	var row: Dictionary = {"case":"B02 AI", "card":def, "source":id, "decision":s.decision.id, "beforeMana":s.players[0].mana,"revision":s.revision,"round":s.globalTurn, "abandon":abandon, "shortMana":short_mana}
	if not check(page._decision.visible and page._decision._options.has("@concentration"), "Preparation asks for acknowledgement"):
		return
	await button(page._decision._options["@concentration"])
	var target: CardPresentation = MatchProjection.card(s,id)
	var bounds: Rect2 = page.canvas.card_bounds(target)
	row.sourceFace = str(bounds)
	row.highlighted = page.canvas.target_ids.has(id)
	if def in ["v260-analytic-0013", "v260-talisman-0005"] and not abandon:
		await shot(def + "-source")
	var move: InputEventMouseMotion = InputEventMouseMotion.new()
	move.position = page.canvas.global_position + bounds.get_center()
	root.push_input(move, true)
	await settle()
	if page._detail.card != null:
		await button(page._detail.get_node("%Close"))
	row.highlightAfterDetail = page.canvas.target_ids.has(id)
	check(row.highlightAfterDetail,"Closing details keeps the required upkeep target highlighted")
	await click(page.canvas.global_position + bounds.get_center())
	row.choicesVisible = page._decision.visible
	row.labels = []
	if page._decision.visible:
		for option: DecisionChoice in page._decision.request.choices:
			row.labels.append(option.label)
	if def in ["v260-analytic-0013", "v260-talisman-0005"] and not abandon:
		await shot(def + "-choice")
	evidence.append(row)
	if not check(page._decision.visible and page._decision._options.has("0:0"), "Real card hit opens the upkeep choices at "+str(bounds)):
		return
	if short_mana:
		row.unavailableLabel = page._decision._labels["@unavailable"].text
		check(str(row.unavailableLabel).contains("准备收入前"), "Insufficient pre-income resources display the authoritative reason")
		check(page._decision._options.has("@unavailable") and page._decision._options["@unavailable"].disabled and not page._decision._options.has("1:"+id), "Insufficient mana disables maintain")
	else:
		if not check(page._decision._options.has("1:"+id), "Exact resources offer maintain"):
			return
		row.maintainCaption = page._decision._labels["1:"+id].text
		check(str(row.maintainCaption).contains("维持") and str(row.maintainCaption).contains(str(fee)), "Visible maintain action shows its actual fee")
	await mode(ResponseTiming.Mode.IGNORE)
	await button(page._decision._options["0:0" if abandon else "1:"+id])
	await button(page._decision._confirm)
	var after: Dictionary = view()
	row.afterRevision = after.revision
	row.afterMana = after.players[0].mana
	row.afterPhase = after.phase
	row.beforeLoad = s.players[0].load
	row.afterLoad = after.players[0].load
	row.zoneAfter = card(after,def,6 if abandon else 3).get("zone",-1)
	check(row.zoneAfter == (6 if abandon else 3), "Native card zone matches upkeep result")
	check(after.decision == null or str(after.decision.id) != str(s.decision.id), "Upkeep completes and native rules advance")
	check(after.players[0].load <= s.players[0].load, "Upkeep does not add load")
	if not abandon:
		var log: Dictionary = service.match_request(&"events_since",[0,"0","0"]).get("data",{})
		var paid: Array = log.get("events",[]).filter(func(event: Dictionary) -> bool: return event.kind == "concentration" and str(event.card) == id)
		check(paid.size() == 1 and int(paid[0].amount) == fee, "Native log records exactly one payment of "+str(fee))

	for step: int in range(40):
		var next: Dictionary = view()
		if int(next.phase) == 2:
			row.reached = "主要阶段"
			break
		if next.aiTurn:
			if not check(service.match_request(&"step_ai", [next.generation,next.revision]).get("ok",false),"Opponent advances after preparation"):
				break
		else:
			var advance: Dictionary = next.actionSummary.safeAdvance if next.actionSummary.get("safeAdvance") is Dictionary else query(next,"0",K.Choose)
			if advance.is_empty() or not submit(next,advance):
				break
	check(row.has("reached"), "Completing upkeep reaches the main phase")

func drag_title(destination: Vector2) -> void:
	var heading: Control = page._confirm.get_node("%Heading")
	var start_point: Vector2 = heading.get_global_rect().get_center()
	var press: InputEventMouseButton = InputEventMouseButton.new()
	press.button_index = MOUSE_BUTTON_LEFT
	press.pressed = true
	press.position = start_point
	press.global_position = start_point
	root.push_input(press, true)
	var motion: InputEventMouseMotion = InputEventMouseMotion.new()
	motion.position = destination
	motion.global_position = destination
	motion.relative = destination - start_point
	motion.button_mask = MOUSE_BUTTON_MASK_LEFT
	root.push_input(motion, true)
	var release: InputEventMouseButton = InputEventMouseButton.new()
	release.button_index = MOUSE_BUTTON_LEFT
	release.position = destination
	release.global_position = destination
	root.push_input(release, true)
	await settle()

func check_drag(size_px: Vector2i) -> void:
	root.size = size_px
	await settle()
	var viewport: Rect2 = Rect2(Vector2.ZERO, Vector2(size_px))
	for point: Vector2 in [Vector2(1,1), Vector2(size_px.x-1,1), Vector2(size_px)-Vector2.ONE, Vector2(1,size_px.y-1)]:
		var prior: Vector2 = page._confirm.position
		await drag_title(point)
		check(page._confirm.position != prior, "Dragging the heading moves the dialog at "+str(size_px))
		check(viewport.encloses(page._confirm.get_global_rect()), "Dialog buttons remain accessible at "+str(point))
		check(page._confirm.visible, "Releasing a drag does not choose an action")

func response_windows() -> void:
	app.navigate(&"menu")
	service.leave_match()
	page = load("res://pages/match_page.tscn").instantiate() as MatchPage
	root.add_child(page)
	await settle()
	var fixture: QueryFixtureService = QueryFixtureService.new()
	fixture.match_mode = "network"
	fixture.match_viewer = 0
	fixture.match_snapshot = {"viewer":0, "generation":"10","revision":"1","result":-1,"activePlayer":0,"players":[{"life":40},{"life":40}], "cards":[{"id":"33","owner":0,"zone":3,"definition":{"id":"v260-talisman-0005","name":"避火咒","type":5,"illustrationId":"protective-flame"}}], "responseContext":{"text":"对手发动【魔法飞弹】"}}
	page.service = fixture
	page._selection.service = fixture
	page._selection.changed.connect(page._query_changed)
	page._selection.completed.connect(page._query_completed)
	page.canvas.configure(MatchProjection.field(fixture.match_snapshot))
	var counter: int = 0
	for preference: int in [ResponseTiming.Mode.ALL,ResponseTiming.Mode.AUTO,ResponseTiming.Mode.IGNORE]:
		var prompts: int = 0
		var auto_passes: int = 0
		for available: bool in [false,false,true]:
			counter += 1
			var id: String = str(counter)
			var s: Dictionary = fixture.match_snapshot
			s.revision = id
			s.decision = {"id":id,"player":0,"kind":KernelContract.DecisionKind.Response}
			var pass_cmd: Dictionary = {"type":K.PassResponse,"decision":id}
			var respond_cmd: Dictionary = {"type":K.Respond,"decision":id,"card":"33"}
			s.actionSummary = {"hasNonPhaseAction":available,"safeAdvance":pass_cmd}
			var actions: Array = [{"id":"pass","source":"0","command":pass_cmd}]
			if available:
				actions.append({"id":"respond","source":"33","command":respond_cmd})
			fixture.set_actions(actions)
			page._network_waiting = false
			var before: int = fixture.confirmations
			await mode(preference)
			page._render_decision()
			page._schedule_automatic()
			var expect_prompt: bool = preference == ResponseTiming.Mode.ALL or (preference == ResponseTiming.Mode.AUTO and available)
			check(page._confirm.visible == expect_prompt,"Prompt visibility at mode %d, available %s" % [preference,available])
			if page._confirm.visible:
				prompts += 1
				if counter == 1:
					await button(page._response_mode)
					check(page._response_mode.get_popup().visible, "Timing control remains clickable through response shade")
					page._response_mode.get_popup().hide()
					for size_px: Vector2i in [Vector2i(1280,720),Vector2i(1920,1080)]:
						await check_drag(size_px)
						var dragged: Vector2 = page._confirm.position
						page._render_decision()
						check(dragged == page._confirm.position, "Same question refresh preserves position")
						await shot("response-dialog-" + str(size_px.x))
					await button(page._confirm.get_node("%Confirm"))
					check(not page._confirm.visible and page._response_browsing and before == fixture.confirmations, "Viewing field submits nothing")
					await button(page._response_cancel)
				if counter == 3:
					await button(page._confirm.get_node("%Confirm"))
					page._select_card(MatchProjection.card(s,"33"))
					page._selection.choose(str(K.Respond))
					check(page._confirm.visible and not page._pending.is_empty(),"Response action reaches shared modal")
					for size_px: Vector2i in [Vector2i(1280,720),Vector2i(1920,1080)]:
						await check_drag(size_px)
						await shot("action-dialog-" + str(size_px.x))
					await key(KEY_ESCAPE)
					check(before == fixture.confirmations and page._pending.is_empty() and page._confirm.visible, "Canceling staged action returns to response without passing")
					await key(KEY_ENTER)
					check(page._response_browsing and before == fixture.confirmations,"Enter opens response sources exactly once")
					await button(page._response_cancel)
				await button(page._confirm.get_node("%Cancel"))
			else:
				auto_passes += 1
				await create_timer(0.18).timeout
			check(fixture.confirmations == before+1 and fixture.selected_actions.back()=="pass","One and only one pass per current decision")
		evidence.append({"case":"O01", "mode":preference, "prompts":prompts, "automaticPasses":auto_passes, "totalPasses":3})
	page._network_waiting = false
	await mode(ResponseTiming.Mode.ALL)
	fixture.match_snapshot.revision = "999"
	fixture.match_snapshot.decision.id = "999"
	var count_before: int = fixture.confirmations
	await button(page._confirm.get_node("%Cancel"))
	check(fixture.confirmations == count_before, "Expired dialog cannot submit even before a repaint")
	evidence.append({"case":"O02","resolutions":[[1280,720],[1920,1080]],"dialogTypes":["response","action"],"draggedToAllEdges":true, "cancelDidNotPass":fixture.confirmations == count_before})
	page.queue_free()
	await settle()

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
	if not check(service.error.is_empty(), service.error):
		quit(2)
		return
	root.size = Vector2i(1280,720)
	await settle()
	await opening("ai")
	await opening("hotseat")
	await concentration("v260-analytic-0013",3,1)
	await concentration("v260-talisman-0005",4,2)
	await concentration("v260-talisman-0005",4,2,true)
	await concentration("v260-talisman-0005",4,2,true,true)
	for item: Array in [["0014",3,1],["0027",5,4],["0017",4,1],["0031",2,2],["0008",3,2],["0028",6,6]]:
		await concentration("v260-analytic-" + str(item[0]), int(item[1]), int(item[2]))
	await response_windows()
	print("INTERACTIONS ", JSON.stringify({"observations":evidence,"errors":errors}, "  "))
	if not output.is_empty():
		var file: FileAccess = FileAccess.open(output.path_join("interactions.json"),FileAccess.WRITE)
		file.store_string(JSON.stringify({"observations":evidence,"errors":errors}, "\t") + "\n")
	for player: Node in root.find_children("*", "AudioStreamPlayer", true, false):
		(player as AudioStreamPlayer).stop()
	await create_timer(0.25).timeout
	app.queue_free()
	await process_frame
	await process_frame
	quit(0 if errors.is_empty() else 2)
