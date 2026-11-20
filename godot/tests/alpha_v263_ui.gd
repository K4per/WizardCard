extends SceneTree

class DelayedService extends QueryFixtureService:
	var delay: bool = false
	func match_request(method: StringName, arguments: Array = []) -> Dictionary:
		var result: Dictionary = super.match_request(method, arguments)
		if delay and method == &"query_actions":
			result["data"]["pending"] = true
		return result

var failures: int = 0
var completed_queries: int = 0

func _initialize() -> void:
	_run.call_deferred()

func check(value: bool, message: String) -> void:
	if not value:
		failures += 1
		push_error("V263_UI: " + message)

func _run() -> void:
	var delayed: DelayedService = DelayedService.new()
	delayed.match_snapshot = {"generation": "1", "revision": "1", "viewer": 0, "decision": {"id": "5", "kind": KernelContract.DecisionKind.Response}}
	delayed.set_actions([{"source": "0", "command": {"type": KernelContract.CommandKind.PassResponse, "decision": "5"}}])
	var delayed_selection: ActionSelectionController = ActionSelectionController.new()
	delayed_selection.service = delayed
	delayed_selection.completed.connect(func(_command: Dictionary, _intent: Dictionary) -> void: completed_queries += 1)
	delayed_selection.begin("0", KernelContract.CommandKind.PassResponse)
	completed_queries = 0
	delayed.delay = true
	delayed.match_snapshot["revision"] = "2"
	delayed_selection.revalidate()
	delayed.delay = false
	delayed_selection.poll()
	check(completed_queries == 0 and delayed_selection.options.options.size() == 1, "asynchronous revalidation waits for explicit confirmation")
	var service: QueryFixtureService = QueryFixtureService.new()
	service.match_mode = "ai"
	service.match_active = true
	service.match_snapshot = {
		"generation": "1", "revision": "2", "viewer": 0, "activePlayer": 0, "result": -1,
		"players": [{"life": 40, "castingCapacity": 2}, {"life": 40, "castingCapacity": 7}],
		"cards": [{"id": "33", "owner": 0, "zone": 1, "definition": {"id": "shield", "name": "护盾术", "type": 5, "flavor": "原文风味"}}],
		"decision": {"id": "9", "player": 0, "kind": KernelContract.DecisionKind.Response},
		"responseContext": {"text": "对方发动【魔法飞弹】", "waitingPlayer": 0},
		"actionSummary": {"hasNonPhaseAction": true, "safeAdvance": {"type": KernelContract.CommandKind.PassResponse, "decision": "9"}},
	}
	service.set_actions([
		{"id": "respond", "source": "33", "command": {"type": KernelContract.CommandKind.Respond, "card": "33", "decision": "9", "ability": "shield", "link": "7"}},
		{"id": "pass", "source": "0", "command": {"type": KernelContract.CommandKind.PassResponse, "decision": "9"}},
	])
	var projection: FieldPresentation = MatchProjection.field(service.match_snapshot)
	check(projection.own_casting_slots == 2 and projection.opponent_casting_slots == 7, "casting slots use authoritative capacities")
	check(projection.hand[0].detail_text().contains("风味") and projection.hand[0].detail_text().contains("原文风味"), "flavor has a distinct detail section")
	var selection: ActionSelectionController = ActionSelectionController.new()
	selection.service = service
	selection.begin("33", KernelContract.CommandKind.Respond)
	var selected_prefix: Dictionary = selection.intent.duplicate(true)
	service.match_snapshot["revision"] = "3"
	check(selection.has_method("revalidate"), "selection can revalidate after an unrelated state update")
	if selection.has_method("revalidate"):
		selection.call("revalidate")
		check(selection.current() and selection.intent == selected_prefix, "legal prefix survives an unrelated revision")
		service.set_actions([])
		service.match_snapshot["revision"] = "4"
		selection.call("revalidate")
		check(not selection.active, "lost candidates invalidate selection before submission")
	service.set_actions([
		{"id": "respond", "source": "33", "command": {"type": KernelContract.CommandKind.Respond, "card": "33", "decision": "9", "ability": "shield", "link": "7"}},
		{"id": "pass", "source": "0", "command": {"type": KernelContract.CommandKind.PassResponse, "decision": "9"}},
	])
	var page: MatchPage = load("res://pages/match_page.tscn").instantiate() as MatchPage
	root.add_child(page)
	await process_frame
	page.service = service
	page._selection.service = service
	page._selection.changed.connect(page._query_changed)
	page._selection.completed.connect(page._query_completed)
	page._selection.failed.connect(page._query_failed)
	page.canvas.configure(projection)
	page._render_decision()
	check(page._decision.request.description.contains("魔法飞弹"), "response explains the current event")
	check(page._decision.request.choices.size() == 2 and page._decision.request.choices[0].label == "是", "response starts with a yes/no question")
	if page._decision.request.choices.size() == 2 and page._decision.request.choices[0].id == "@yes":
		page._decision_confirmed("1", "4", "9", ["@yes"])
		check(not page._decision.visible and page.canvas.target_ids.has("33"), "yes highlights real response sources without a card collection")
		check(service.confirmations == 0, "viewing the field does not pass or pay")
		page._decision_cancelled("1", "4", "9")
		check(page._decision.visible and service.confirmations == 0, "cancel returns to the same response question")
	for situation: String in ["payable", "insufficient", "blocked"]:
		service.confirmations = 0
		service.match_snapshot["decision"] = {"id": situation, "player": 0, "kind": KernelContract.DecisionKind.Concentration, "source": "33"}
		service.match_snapshot["actionSummary"] = {}
		var choices: Array = [{"id": "abandon", "label": "销毁专注法术", "source": "0", "command": {"type": KernelContract.CommandKind.Choose, "decision": situation, "option": "0"}}]
		if situation == "payable":
			choices.push_front({"id": "maintain", "label": "维持专注（魔素 2）", "source": "33", "command": {"type": KernelContract.CommandKind.Choose, "decision": situation, "option": {"kind": KernelContract.ChoiceKind.Card, "value": "33"}}})
		service.set_actions(choices)
		page._render_decision()
		page._decision_confirmed("1", "4", situation, ["@concentration"])
		check(not page._decision.visible and page.canvas.target_ids.has("33") and page._end_phase.disabled, "concentration acknowledgement highlights the mandatory source: " + situation)
		page._select_card(projection.hand[0])
		check(service.confirmations == 0 and page._decision.visible, "clicking concentration source never pays: " + situation)
		var labels: String = ""
		for choice: DecisionChoice in page._decision.request.choices:
			labels += choice.label
		check(labels.contains("维持") and labels.contains("放弃"), "concentration explicitly offers maintain/abandon: " + situation)
		if page._decision.visible and labels.contains("放弃"):
			var selected_value: String = "1:33" if situation == "payable" else "0"
			page._decision_confirmed("1", "4", situation, [selected_value])
			check(service.confirmations == 1, "explicit concentration choice commits exactly once: " + situation)
		page._selection.cancel()
		page._network_waiting = false
	service.match_mode = "network"
	service.match_snapshot["decision"] = null
	service.match_snapshot["opening"] = true
	service.match_snapshot["openingStage"] = "formation"
	service.match_snapshot["initialFormationConfirmed"] = [false, true]
	service.set_actions([{ "id": "formation", "source": "33", "command": {"type": KernelContract.CommandKind.SelectInitialFormation, "card": "33"}}])
	page._render_opening()
	check(page._selection.active, "opening query is active before a network acknowledgement")
	page.network_updated({"exists": true, "connected": true, "blocked": true, "room": {"phase": "playing"}})
	page.network_updated({"exists": true, "connected": true, "blocked": false, "room": {"phase": "playing"}})
	check(page._selection.active and page._decision.visible, "network unblock reopens outstanding opening choice even without another revision")
	service.match_snapshot["opening"] = false
	page._selection.cancel()
	page._decision.hide()
	var stress: FieldPresentation = FieldPresentation.new()
	stress.own_casting_slots = 22
	stress.opponent_casting_slots = 22
	for index: int in range(22):
		for own: bool in [true, false]:
			var card: CardPresentation = projection.hand[0].duplicate() as CardPresentation
			card.instance_id = ("own-" if own else "opponent-") + str(index)
			if own:
				stress.own_casting.append(card)
			else:
				stress.opponent_casting.append(card)
	page.canvas.configure(stress)
	for dimensions: Vector2i in [Vector2i(1280, 720), Vector2i(1600, 900), Vector2i(1920, 1080), Vector2i(2560, 1440), Vector2i(1440, 900)]:
		root.size = dimensions
		page.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
		await process_frame
		page._layout_panels()
		await process_frame
		for own: bool in [true, false]:
			var seen: Dictionary = {}
			var cards: Array[CardPresentation] = stress.own_casting if own else stress.opponent_casting
			var region: Rect2 = page.canvas._own_casting_region if own else page.canvas._opponent_casting_region
			for step: int in range(cards.size()):
				for card: CardPresentation in cards:
					var bounds: Rect2 = page.canvas.card_bounds(card)
					if bounds.has_area():
						check(region.encloses(bounds), "full casting track never clips a card")
						check(page.canvas.card_at(bounds.get_center()) == card, "visible casting card remains selectable")
						seen[card.instance_id] = true
				if seen.size() == cards.size():
					break
				if page.canvas.has_method("_scroll_casting"):
					page.canvas.call("_scroll_casting", region.get_center(), 1)
					await process_frame
					await process_frame
			check(seen.size() == 22, "all 22 casting cards can be reached at every size")
			if page.canvas.has_method("_scroll_casting"):
				page.canvas.call("_scroll_casting", region.get_center(), -100)
		page._position_phase_button()
		check(page._end_phase.position.x >= page.canvas.position.x + page.canvas._own_casting_region.end.x, "phase control cannot cover casting cards or page arrows")
		check(page._detail.size.x <= maxf(244, dimensions.x * 0.20), "compact details leave more width for the battlefield")
		check(is_equal_approx(page.canvas._own_casting_region.position.y, page.canvas._opponent_casting_region.position.y), "both players cast on the same horizontal band")
		check(page._detail.position.x < dimensions.x * 0.25, "detail stays in left column")
		check(page.canvas.position.x > page._detail.position.x + page._detail.size.x, "detail never blocks the field")
		check(page._status_opponent.position.x > dimensions.x * 0.65, "opponent status remains top right")
	page._update_chain({"opening":true,"openingStage":"formation","viewer":0,"initialFormationConfirmed":[true,false]})
	check(page._phase.text.contains("等待玩家 2"), "opening heading names the player still choosing")
	page._update_chain({"activePlayer":0,"chain":{"mode":0,"priority":1,"links":[]}})
	check(page._phase.text.contains("等待玩家 2"), "replay heading uses public chain priority when no actingPlayer is projected")
	root.size = Vector2i(1280, 720)
	var typography: Node = root.get_node("GlobalTypography")
	typography.call(&"set_scale", 120)
	await process_frame
	await process_frame
	var fixed_frame: Rect2 = page._detail.get_global_rect()
	var fixed_board: Rect2 = page.canvas.get_global_rect()
	var long_card: CardPresentation = projection.hand[0].duplicate() as CardPresentation
	long_card.text = "持续效果与费用。".repeat(350)
	page._detail.configure(long_card)
	await process_frame
	await process_frame
	var body_scroll: VScrollBar = page._detail._body.get_v_scroll_bar()
	check(body_scroll.max_value > body_scroll.page and page._detail.get_global_rect() == fixed_frame, "long effect text scrolls inside the fixed frame at 720p and 120% type")
	check(page.canvas.get_global_rect() == fixed_board and (Rect2(Vector2.ZERO, root.size)).encloses(page._response_mode.get_global_rect()), "detail and zoom leave action controls in bounds")
	page._detail.clear_card()
	typography.call(&"set_scale", 100)
	await process_frame
	await process_frame
	var before_overflow: Array = service.match_snapshot.cards.duplicate(true)
	service.match_snapshot.players[0]["castingCapacity"] = 3
	for count: int in [5, 4]:
		var choices: Array = []
		service.match_snapshot.cards = []
		for slot: int in range(count):
			var id: String = str(100 + slot)
			service.match_snapshot.cards.append({"id":id,"owner":0,"zone":5,"definition":{"name":"公开法术 %s" % id,"type":1}})
			choices.append({"source":"0","command":{"type":KernelContract.CommandKind.Choose,"option":{"kind":KernelContract.ChoiceKind.Card,"value":id}}})
		service.match_snapshot.decision = {"id":str(count),"kind":KernelContract.DecisionKind.Overflow,"player":0}
		service.set_actions(choices)
		page._render_decision()
		check(page._notice.is_visible_in_tree() and page._notice.text.contains("还需选择 %d 张送灰" % [count - 3]), "overflow prompt follows current authoritative count and capacity")
		check(page._decision.request.choices.size() == count, "every overflow candidate remains available in the tray")
		page._decision_confirmed("1", "4", str(count), ["1:100"])
		page._network_waiting = false
	service.match_snapshot.cards = before_overflow
	page._update_chain({"chain":{"mode":1,"links":[{"source":"0","kindName":"设置"},{"source":"0","kindName":"响应"}]}})
	var active_chain: String = page._chain_text.get_parsed_text()
	check(active_chain.find("C1") < active_chain.find("C2") and active_chain.contains("正在结算 C2"), "active links follow declaration order and mark the reverse resolving link")
	service.match_snapshot["decision"] = null
	service.match_snapshot["actionSummary"] = {"safeAdvance":{"type":KernelContract.CommandKind.AdvancePhase}}
	service.set_actions([{"source":"33", "command":{"type": KernelContract.CommandKind.StartAnalysis, "card":"33", "formation":"55", "ringIndex":2, "targets":["44"]}}])
	page._selected = projection.hand[0]
	page._selection.begin("33", KernelContract.CommandKind.StartAnalysis)
	page._render_actions(false)
	check(page._end_phase.disabled, "phase cannot replace an unfinished source/target choice")
	page._open_match_menu()
	page.dismiss_cancel()
	check(not page._match_menu.visible and page._selection.active, "closing the menu restores an unfinished choice")
	page._selection.cancel()
	page._decision.hide()
	page._restore_tray_hand()
	var board: Rect2 = page.canvas.get_global_rect()
	var inspected: CardPresentation = CardPresentation.anonymous("hidden")
	page._selection.cancel()
	page._clear_card()
	check(page._detail.visible and page._detail.card == null, "clearing keeps the empty detail frame")
	page._selected = projection.hand[0]
	page.canvas.selected_id = page._selected.instance_id
	page._hover_card(inspected)
	check(page._detail.card == inspected and page._detail._title.text == "盖伏卡" and page.canvas.selected_id == "33", "inspecting a different card preserves the pinned source")
	page._hover_card(null)
	check(page._detail.card == page._selected, "leaving a preview restores the selected card")
	var events: Array = [{"kind":"link_resolved", "text":"结算链节 #17", "chainIndex":2}, {"kind":"link_skipped", "text":"取消链节 #16", "chainIndex":1}, {"kind":"chain_closed", "text":"连锁结束"}]
	page._events_changed({"reset":true, "events":events})
	page._update_chain({})
	check(page._chain_text.get_parsed_text().contains("C2") and page._chain_text.get_parsed_text().contains("C1"), "both finished links stay readable after fast resolution")
	var automatic_events: Array = [{"kind":"chain_open","text":"后续时点"}, {"kind":"link_resolved","text":"结算链节 #18","chainIndex":1}, {"kind":"chain_closed","text":"连锁结束"}]
	page._events_changed({"events":automatic_events})
	events.append_array(automatic_events)
	check(page._chain_text.get_parsed_text().contains("C2 → C1"), "an automatic one-link window cannot erase the just-finished multi-link sequence")
	check(page._logs_text.get_parsed_text().find("C2") >= 0 and page._logs_text.get_parsed_text().find("C2") < page._logs_text.get_parsed_text().find("C1"), "public log retains the reverse resolution order")
	check(page._logs_panel.is_visible_in_tree() and page._logs_panel.global_position.x >= board.end.x and page._chain_text.global_position.y < page._logs_panel.global_position.y, "both history panels stay on the right")
	check(page.canvas.get_global_rect() == board and page.canvas.selected_id == "33", "log updates preserve field geometry and selected action source")
	var replay: ReplayPage = load("res://pages/replay_page.tscn").instantiate()
	root.add_child(replay)
	await process_frame
	replay._layout_panels()
	await process_frame
	check(replay._canvas.get_global_rect() == board and replay._detail.get_global_rect() == page._detail.get_global_rect(), "replay and match use identical table geometry %s %s %s %s" % [replay._canvas.get_global_rect(), board, replay._detail.get_global_rect(), page._detail.get_global_rect()])
	replay._update_logs({"eventLog":events})
	check(replay._logs_text.get_parsed_text() == page._logs_text.get_parsed_text(), "the same public events have identical captions in replay")
	page._events_changed({"reset":true,"events":[]})
	for event_index: int in range(80):
		page._events_changed({"events":[{"text":"历史事件 %d" % event_index}]})
	await process_frame
	await process_frame
	var scroll: VScrollBar = page._logs_text.get_v_scroll_bar()
	scroll.value = scroll.min_value
	var previous_scroll: float = scroll.value
	page._events_changed({"events":[{"text":"新的公开事件"}]})
	await process_frame
	check(is_equal_approx(scroll.value, previous_scroll), "a new public event preserves reading position")
	replay.queue_free()
	page.queue_free()
	await process_frame
	print("Alpha v2.6.3 UI failures: ", failures)
	quit(0 if failures == 0 else 1)
