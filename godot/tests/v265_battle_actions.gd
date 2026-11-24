extends "res://tests/v264_match_acceptance.gd"

func _phase_for_owner() -> Dictionary:
	for step: int in range(160):
		var snapshot: Dictionary = view(0)
		if int(snapshot.actingPlayer) == 1:
			snapshot = view(1)
		var command: Dictionary = {}
		if snapshot.opening:
			if snapshot.openingStage == "gestures":
				command = {"type": K.SubmitOpeningGesture, "round": str(snapshot.openingRound), "gesture": "0" if snapshot.viewer == 0 else "2"}
			else:
				for card: Dictionary in snapshot.cards:
					if card.owner == snapshot.viewer and card.zone == 9 and card.get("definition", {}).get("id") == "v260-formation-0009":
						command = query(snapshot, str(card.id), K.SelectInitialFormation)
						break
		elif snapshot.viewer == 0 and snapshot.phase == 2 and snapshot.decision == null:
			return snapshot
		elif snapshot.actionSummary.get("safeAdvance") is Dictionary:
			command = snapshot.actionSummary.safeAdvance
		else:
			command = query(snapshot, "0", K.Choose)
		if not check(not command.is_empty(), "Real match can reach the owner's main phase") or not submit(snapshot, command):
			return {}
	return {}

func _side_card(snapshot: Dictionary, definition: String) -> CardPresentation:
	for card: Dictionary in snapshot.cards:
		if card.owner == 0 and card.zone == 9 and card.get("definition", {}).get("id") == definition:
			return MatchProjection.card(snapshot, str(card.id))
	return null

func _start_recipe(card: CardPresentation) -> void:
	page._select_card(card)
	page._selection.choose(str(K.SetFormation))
	page._formation_slot_selected(1)

func _run() -> void:
	app = load("res://app/main.tscn").instantiate()
	root.add_child(app)
	await process_frame
	service = app.service
	var deck: Dictionary = {"cards": [], "sideDeck": []}
	for i: int in range(40):
		deck.cards.append("v260-word-%04d" % (1 + floori(float(i) / 3)))
	for number: int in [9, 9, 7, 7, 1, 2, 3, 4, 5, 6]:
		deck.sideDeck.append("v260-formation-%04d" % number)
	if not check(service.start_match({"mode": "hotseat", "seed": 42, "players": [deck, deck]}), service.error):
		_finish()
		return
	var snapshot: Dictionary = _phase_for_owner()
	if snapshot.is_empty():
		_finish()
		return
	app.navigate(&"match")
	page = app.page as MatchPage
	page._reveal()
	await process_frame
	var recipe: CardPresentation = _side_card(snapshot, "v260-formation-0007")
	_start_recipe(recipe)
	if not check(page._material_bar.visible and not page._decision.visible and page.canvas.target_ids.size() >= 2, "Two-material recipe is selectable in the hand"):
		_finish()
		return
	var ids: Array[String] = page.canvas.target_ids.keys()
	page._select_card(MatchProjection.card(snapshot, ids[1]))
	page._select_card(MatchProjection.card(snapshot, ids[0]))
	check(page._material_count.text == "材料 2 / 2" and not page._continue_materials.disabled, "Full reverse-order material set enables confirmation")
	page._select_card(MatchProjection.card(snapshot, ids[1]))
	check(page._material_count.text == "材料 1 / 2" and page._continue_materials.disabled, "First selected instance can be removed after completing the set")
	page._dismiss_confirmation()
	check(view(0).revision == snapshot.revision and view(0).players == snapshot.players, "Cancelling material choice preserves the authoritative state")
	page._clear_card()
	var basic: CardPresentation = _side_card(snapshot, "v260-formation-0009")
	_start_recipe(basic)
	if not check(page._material_bar.visible, "Additional base formation has a legal discard recipe"):
		_finish()
		return
	var discard: String = page.canvas.target_ids.keys()[0]
	page._select_card(MatchProjection.card(snapshot, discard))
	page._continue_materials.pressed.emit()
	if not check(page._confirm.visible and view(0).revision == snapshot.revision, "Recipe payment waits for the command confirmation"):
		_finish()
		return
	page._confirm.get_node("%Confirm").pressed.emit()
	snapshot = _phase_for_owner()
	service.match_viewer = 0
	page._pending_viewer = -1
	service.poll_session()
	page._refresh()
	await process_frame
	var source: CardPresentation = page.canvas.field.hand.front()
	page._select_card(source)
	page._selection.choose(str(K.SetAmbush))
	if not check(page.canvas.target_rings.size() >= 2 and page.canvas.target_rings.has(basic.instance_id), "Both actual hosts expose their same-numbered ring together"):
		_finish()
		return
	page._ring_selected(basic.instance_id, 0)
	check(page._confirm.visible and str(page._pending.command.formation) == basic.instance_id, "Ring click binds the second host")
	page._confirm.get_node("%Cancel").pressed.emit()
	page._placement_requested(source.instance_id, basic.instance_id, 0)
	check(not page._confirm.visible, "Drop with no chosen operation waits")
	page._selection.choose(str(K.SetAmbush))
	if not check(page._confirm.visible and str(page._pending.command.formation) == basic.instance_id, "Drop reuses the selected host and ring after operation choice"):
		_finish()
		return
	page._confirm.get_node("%Confirm").pressed.emit()
	var placed: Dictionary = find_card(view(0), source.instance_id)
	check(str(placed.host) == basic.instance_id and placed.ringIndex == 0, "Actual submitted card occupies the dropped slot")
	_finish()
