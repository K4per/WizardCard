class_name MatchProjection
extends RefCounted

static func field(snapshot: Dictionary) -> FieldPresentation:
	var result: FieldPresentation = FieldPresentation.new()
	var viewer: int = int(snapshot.get("viewer", 0))
	result.viewer = viewer
	result.generation = str(snapshot.get("generation", ""))
	var players: Array = snapshot.get("players", [])
	if players.size() != 2:
		return result
	var own: Dictionary = players[viewer]
	var opponent: Dictionary = players[1 - viewer]
	result.own_life = int(own.get("life", 0))
	result.opponent_life = int(opponent.get("life", 0))
	result.own_mana = int(own.get("mana", 0))
	result.opponent_mana = int(opponent.get("mana", 0))
	result.own_load = int(own.get("load", 0))
	result.opponent_load = int(opponent.get("load", 0))
	result.own_load_limit = int(own.get("capacity", 0))
	result.opponent_load_limit = int(opponent.get("capacity", 0))
	result.own_deck_count = int(own.get("deckCount", 0))
	result.opponent_deck_count = int(opponent.get("deckCount", 0))
	result.own_side_count = int(own.get("sideDeckCount", 0))
	result.opponent_side_count = int(opponent.get("sideDeckCount", 0))
	result.phase_label = GameText.term(snapshot, "phaseName")
	result.global_turn = str(snapshot.get("globalTurn", "0"))
	result.opening = bool(snapshot.get("opening", false))
	result.opponent_hand_count = int(opponent.get("handCount", 0))
	var clock: Dictionary = snapshot.get("clock", {})
	result.clock_available = bool(clock.get("enabled", false))
	var remaining: Array = clock.get("remainingMs", [0, 0])
	var running_player: int = int(clock.get("runningPlayer", -1))
	var running_players: Array = clock.get("runningPlayers", [false, false])
	for player: int in range(2):
		result.clock_remaining[player] = int(remaining[player]) if remaining.size() > player else 0
		result.clock_running[player] = bool(clock.get("running", false)) and (bool(running_players[player]) if running_players.size() > player else running_player == player)
	result.can_set_formation = bool(snapshot.get("canSetFormation", false))
	for source: String in snapshot.get("availableCards", {}):
		result.legal_cards[source] = int(snapshot["availableCards"][source])
	var formations: Dictionary[String, FormationPresentation] = {}
	var rows: Array = (snapshot.get("cards", []) as Array).duplicate()
	rows.sort_custom(func(left: Dictionary, right: Dictionary) -> bool:
		var a: String = str(left.get("fieldOrder", "0"))
		var b: String = str(right.get("fieldOrder", "0"))
		return a.length() < b.length() if a.length() != b.length() else a < b)
	var ordered_occupants: Array = rows.duplicate()
	# Public occupied rings are placed before anonymous covers so a hidden card
	# never replaces a visible occupant when its private ring field is omitted.
	ordered_occupants.sort_custom(func(left: Dictionary, right: Dictionary) -> bool:
		return not bool(left.get("hidden", false)) and bool(right.get("hidden", false)))
	for row: Dictionary in rows:
		if int(row.get("zone", -1)) != 3 or bool(row.get("hidden", false)):
			continue
		var definition: Dictionary = row.get("definition", {})
		if int(definition.get("type", -1)) != 3:
			continue
		var formation: FormationPresentation = FormationPresentation.new()
		formation.card = CardPresentation.from_projection(row)
		formation.body_size = maxi(1, int(definition.get("body", 1)))
		formation.ring_count = int(row.get("effectiveRings", definition.get("rings", 0)))
		formations[formation.card.instance_id] = formation
		if int(row.get("owner", -1)) == viewer:
			result.own_formations.append(formation)
		else:
			result.opponent_formations.append(formation)
	for row: Dictionary in ordered_occupants:
		var card: CardPresentation = CardPresentation.from_projection(row)
		var is_own: bool = int(row.get("owner", -1)) == viewer
		var zone: int = int(row.get("zone", -1))
		result.cards_by_id[card.instance_id] = card
		result.card_zones[card.instance_id] = zone
		result.card_hosts[card.instance_id] = str(row.get("host", "0"))
		if zone == 1 and is_own:
			result.hand.append(card)
		elif zone == 7:
			var host: String = str(row.get("host", "0"))
			if host != "0" and not host.is_empty():
				if not result.attachments.has(host):
					result.attachments[host] = []
				result.attachments[host].append(card)
		elif zone in [3, 4] and not formations.has(card.instance_id):
			var host: String = str(row.get("host", "0"))
			if formations.has(host):
				var formation: FormationPresentation = formations[host]
				var ring: int = int(row.get("ringIndex", -1))
				if ring < 0:
					ring = 0
					while formation.occupants.has(ring):
						ring += 1
				formation.occupants[ring] = card
			else:
				if is_own:
					result.own_casting.append(card)
				else:
					result.opponent_casting.append(card)
		elif zone in [2, 5, 8]:
			if is_own:
				result.own_casting.append(card)
			else:
				result.opponent_casting.append(card)
		elif zone == 6:
			if is_own:
				result.own_ash_count += 1
			else:
				result.opponent_ash_count += 1
	result.own_casting_slots = int((snapshot.get("players", [{}, {}])[viewer] as Dictionary).get("castingCapacity", 0))
	result.opponent_casting_slots = int((snapshot.get("players", [{}, {}])[1 - viewer] as Dictionary).get("castingCapacity", 0))
	return result

static func card(snapshot: Dictionary, id: String) -> CardPresentation:
	for row: Dictionary in snapshot.get("cards", []):
		if str(row.get("id", "")) == id:
			return CardPresentation.from_projection(row)
	return null

static func card_name(snapshot: Dictionary, id: String) -> String:
	if id.is_empty() or id == "0":
		return GameText.label("ui.object.none")
	if id in ["1073741825", "1073741826"]:
		return GameText.label("ui.object.own_player") if id == str(1073741825 + int(snapshot.get("viewer", 0))) else GameText.label("ui.object.opponent_player")
	var value: CardPresentation = card(snapshot, id)
	return value.title if value != null else GameText.label("ui.object.public_object")

static func action_group_key(action: Dictionary) -> String:
	var command: Dictionary = action.get("command", {})
	var type: int = int(command.get("type", -1))
	var key: String = str(type)
	if type == KernelContract.CommandKind.SetFormation:
		key += ":replace" if str(command.get("replace", "0")) != "0" else ":set"
	elif type == KernelContract.CommandKind.Respond:
		key += ":" + str(command.get("ability", ""))
	elif type == KernelContract.CommandKind.AdvancePhase:
		key += ":end" if bool(command.get("skipCast", false)) else ":next"
	return key

const ACTION_LABEL_KEYS: Dictionary[int, String] = {
	KernelContract.CommandKind.RemoveFormation: "ui.action.remove_formation",
	KernelContract.CommandKind.StartAnalysis: "ui.action.start_analysis",
	KernelContract.CommandKind.PrepareCast: "ui.action.prepare_cast",
	KernelContract.CommandKind.PlayAction: "ui.action.activate",
	KernelContract.CommandKind.AttachSeal: "ui.action.attach_seal",
	KernelContract.CommandKind.RemoveSeal: "ui.action.remove_seal",
	KernelContract.CommandKind.Abandon: "ui.action.abandon",
	KernelContract.CommandKind.Advance: "ui.action.advance",
	KernelContract.CommandKind.Surrender: "ui.action.surrender",
	KernelContract.CommandKind.Respond: "ui.action.respond",
	KernelContract.CommandKind.PassResponse: "ui.action.pass_response",
	KernelContract.CommandKind.AdvancePhase: "ui.action.advance_phase",
	KernelContract.CommandKind.PreloadWord: "ui.action.preload_word",
	KernelContract.CommandKind.SetAmbush: "ui.card.face_down",
	KernelContract.CommandKind.FlipAmbush: "ui.action.flip_ambush",
	KernelContract.CommandKind.ActivateSpell: "ui.action.activate",
	KernelContract.CommandKind.SelectInitialFormation: "ui.action.select_initial_formation",
	KernelContract.CommandKind.Timeout: "ui.action.timeout",
	KernelContract.CommandKind.ActivateAbility: "ui.action.activate_ability",
}

static func action_label(snapshot: Dictionary, action: Dictionary) -> String:
	if action.get("labelRef") is Dictionary:
		return GameText.field(action, "label")
	var command: Dictionary = action.get("command", {})
	var type: int = int(command.get("type", -1))
	if type == KernelContract.CommandKind.SetFormation:
		return GameText.label("ui.action.replace_formation") if str(command.get("replace", "0")) != "0" else GameText.label("ui.action.set_formation")
	if type == KernelContract.CommandKind.PreloadWord:
		return GameText.label("ui.action.set_and_activate") if bool(command.get("release", false)) else GameText.label("ui.action.preload_word")
	if type == KernelContract.CommandKind.Abandon:
		for row: Dictionary in snapshot.get("cards", []):
			if str(row.get("id", "0")) == str(action.get("source", "0")):
				return GameText.label("ui.action.abandon_cast") if int(row.get("zone", -1)) == 5 else GameText.label("ui.action.abandon")
	if type == KernelContract.CommandKind.AdvancePhase:
		if bool(command.get("skipCast", false)):
			return GameText.label("ui.action.skip_to_end")
		return GameText.label("ui.action.switch_to_cast") if int(snapshot.get("phase", -1)) == 2 else GameText.label("ui.action.end_current_phase")
	if type == KernelContract.CommandKind.Choose:
		var label: String = str(action.get("label", GameText.label("ui.action.confirm")))
		var choice: Dictionary = command.get("option", {})
		var option: String = str(choice.get("value", "0"))
		return label.replace(" #" + option, "") if option != "0" else label
	return GameText.label(ACTION_LABEL_KEYS.get(type, "ui.action.confirm"))

static func action_detail(snapshot: Dictionary, action: Dictionary) -> String:
	var command: Dictionary = action.get("command", {})
	var lines: Array[String] = [action_label(snapshot, action)]
	var source: String = str(action.get("source", "0"))
	if source != "0":
		lines[0] += " · " + card_name(snapshot, source)
	var blood_hint: String = blood_heartmark_hint(snapshot, action)
	if not blood_hint.is_empty():
		lines.append(blood_hint)
	if int(command.get("type", -1)) == KernelContract.CommandKind.RemoveFormation:
		var spells: Array[String] = []
		var seals: int = 0
		var departing: Array[String] = [source]
		var cursor: int = 0
		while cursor < departing.size():
			var host: String = departing[cursor]
			cursor += 1
			for row: Dictionary in snapshot.get("cards", []):
				var id: String = str(row.get("id", "0"))
				var zone: int = int(row.get("zone", -1))
				if str(row.get("host", "0")) != host or not zone in [3, 7] or departing.has(id):
					continue
				departing.append(id)
				if zone == 7:
					seals += 1
				else:
					spells.append(card_name(snapshot, id))
		lines.append(GameText.label("ui.formation.free_operation"))
		if spells.is_empty() and seals == 0:
			lines.append(GameText.label("ui.formation.depart_alone"))
		else:
			var impact: String = GameText.label("ui.formation.depart_with_cards")
			if not spells.is_empty():
				impact += GameText.label("ui.formation.departing_spells", {"spells": "、".join(spells)})
			if seals > 0:
				impact += GameText.label("ui.formation.departing_runes", {"count": seals})
			lines.append(impact + "。")
		lines.append(GameText.label("ui.formation.recalculate_on_departure"))
	for entry: Array in [["formation", GameText.label("ui.selection.ring_formation")], ["host", GameText.label("ui.selection.attached_host")], ["replace", GameText.label("ui.action.replace_formation")], ["target", GameText.label("ui.selection.target")], ["discard", GameText.label("ui.selection.discard_cost")]]:
		var id: String = str(command.get(entry[0], "0"))
		if id != "0" and not id.is_empty():
			lines.append(str(entry[1]) + "：" + card_name(snapshot, id))
	for entry: Array in [["targets", GameText.label("ui.selection.target")], ["materials", GameText.label("ui.selection.material_cost")]]:
		var names: Array[String] = []
		for id: Variant in command.get(entry[0], []):
			names.append(card_name(snapshot, str(id)))
		if not names.is_empty():
			lines.append(str(entry[1]) + "：" + "、".join(names))
	var link: String = str(command.get("link", "0"))
	if link != "0":
		var chain: Dictionary = snapshot.get("chain", {}) if snapshot.get("chain") is Dictionary else {}
		for entry: Dictionary in chain.get("links", []):
			if str(entry.get("id", "0")) == link:
				var link_source: String = str(entry.get("source", "0"))
				lines.append(GameText.label("ui.selection.chain_target", {"target": (card_name(snapshot, link_source) if link_source != "0" else GameText.term(entry, "kindName", GameText.label("terms.link.phase")))}))
				break
	return "\n".join(lines)

static func blood_heartmark_hint(snapshot: Dictionary, action: Dictionary) -> String:
	# The native legal action remains the authority. This only projects a hint
	# when that exact spell action has an attached rune with the declared trait.
	var command: Dictionary = action.get("command", {})
	var command_type: int = int(command.get("type", -1))
	if command_type not in [KernelContract.CommandKind.PlayAction, KernelContract.CommandKind.Respond, KernelContract.CommandKind.ActivateSpell]:
		return ""
	var source: String = str(action.get("source", command.get("card", "0")))
	if source.is_empty() or source == "0":
		return ""
	var source_definition: Dictionary = {}
	for row: Dictionary in snapshot.get("cards", []):
		if str(row.get("id", "")) == source:
			source_definition = row.get("definition", {}) as Dictionary
			break
	if source_definition.is_empty() or not _definition_deals_damage(source_definition):
		return ""
	for row: Dictionary in snapshot.get("cards", []):
		if str(row.get("host", "0")) != source or int(row.get("zone", -1)) != 7:
			continue
		var rune_definition: Dictionary = row.get("definition", {}) as Dictionary
		var traits: Array = rune_definition.get("traits", []) as Array
		if traits.has("blood_heartmark"):
			return GameText.label("ui.action.blood_heartmark_hint")
	return ""

static func _definition_deals_damage(definition: Dictionary) -> bool:
	for effect: Dictionary in definition.get("effects", []):
		var kind: String = str(effect.get("kind", ""))
		if kind in ["damage", "blood_damage", "ray_volley"] or effect.has("damageType"):
			return true
	for ability: Dictionary in definition.get("responses", []):
		for effect: Dictionary in ability.get("effects", []):
			var kind: String = str(effect.get("kind", ""))
			if kind in ["damage", "blood_damage", "ray_volley"] or effect.has("damageType"):
				return true
	return false

static func formation_removal_unavailable(snapshot: Dictionary, id: String) -> String:
	var viewer: int = int(snapshot.get("viewer", 0))
	var formation: bool = false
	for row: Dictionary in snapshot.get("cards", []):
		if str(row.get("id", "0")) == id:
			formation = int(row.get("owner", -1)) == viewer and int(row.get("zone", -1)) == 3 and int((row.get("definition", {}) as Dictionary).get("type", -1)) == 3
			break
	if not formation:
		return ""
	if int(snapshot.get("result", -1)) != -1:
		return GameText.label("ui.formation.removal_after_match")
	if bool(snapshot.get("opening", false)):
		return GameText.label("ui.formation.removal_during_opening")
	if int(snapshot.get("phase", -1)) != 2 or int(snapshot.get("activePlayer", -1)) != viewer:
		return GameText.label("ui.formation.removal_wrong_phase")
	if snapshot.get("decision") is Dictionary or snapshot.get("chain") is Dictionary:
		return GameText.label("ui.formation.removal_blocked")
	return GameText.label("ui.formation.removal_unavailable")
