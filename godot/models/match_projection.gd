class_name MatchProjection
extends RefCounted

static func field(snapshot: Dictionary) -> FieldPresentation:
	var result: FieldPresentation = FieldPresentation.new()
	var viewer: int = int(snapshot.get("viewer", 0))
	result.viewer = viewer
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
	result.phase_label = str(snapshot.get("phaseName", ""))
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
	for action: Dictionary in snapshot.get("actions", []):
		var source: String = str(action.get("source", "0"))
		if source.is_empty() or source == "0":
			continue
		var command: Dictionary = action.get("command", {})
		var activation: bool = int(command.get("type", -1)) in [3, 12, 16, 17]
		result.legal_cards[source] = maxi(result.legal_cards.get(source, 0), 2 if activation else 1)
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
		if zone == 1 and is_own:
			result.hand.append(card)
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
	result.own_casting_slots = maxi(4, result.own_casting.size())
	result.opponent_casting_slots = maxi(4, result.opponent_casting.size())
	return result

static func card(snapshot: Dictionary, id: String) -> CardPresentation:
	for row: Dictionary in snapshot.get("cards", []):
		if str(row.get("id", "")) == id:
			return CardPresentation.from_projection(row)
	return null

static func card_name(snapshot: Dictionary, id: String) -> String:
	if id.is_empty() or id == "0":
		return "无"
	var value: CardPresentation = card(snapshot, id)
	return value.title if value != null else "公开对象"

static func action_group_key(action: Dictionary) -> String:
	var command: Dictionary = action.get("command", {})
	var type: int = int(command.get("type", -1))
	var key: String = str(type)
	if type == 0:
		key += ":replace" if str(command.get("replace", "0")) != "0" else ":set"
	elif type == 12:
		key += ":" + str(command.get("ability", ""))
	elif type == 14:
		key += ":end" if bool(command.get("skipCast", false)) else ":next"
	return key

static func action_label(snapshot: Dictionary, action: Dictionary) -> String:
	var command: Dictionary = action.get("command", {})
	var type: int = int(command.get("type", -1))
	var labels: Dictionary[int, String] = {1: "拆除阵法", 2: "设置解析", 3: "准备施法", 4: "发动", 6: "附着", 7: "拆除符文", 8: "放弃", 9: "继续", 11: "投降", 12: "响应", 13: "放弃响应", 14: "切换阶段", 15: "盖伏", 16: "反转", 17: "发动", 18: "选择初始阵法"}
	if type == 0:
		return "替换阵法" if str(command.get("replace", "0")) != "0" else "设置阵法"
	if type == 5:
		return "设置并发动" if bool(command.get("release", false)) else "设置言灵"
	if type == 8:
		for row: Dictionary in snapshot.get("cards", []):
			if str(row.get("id", "0")) == str(action.get("source", "0")):
				return "放弃施法" if int(row.get("zone", -1)) == 5 else "放弃"
	if type == 14:
		if bool(command.get("skipCast", false)):
			return "切换到结束阶段"
		return "切换到施法阶段" if int(snapshot.get("phase", -1)) == 2 else "结束当前阶段"
	if type == 10:
		var label: String = str(action.get("label", "确认"))
		var option: String = str(command.get("option", "0"))
		return label.replace(" #" + option, "") if option != "0" else label
	return labels.get(type, "确认")

static func action_detail(snapshot: Dictionary, action: Dictionary) -> String:
	var command: Dictionary = action.get("command", {})
	var lines: Array[String] = [action_label(snapshot, action)]
	var source: String = str(action.get("source", "0"))
	if source != "0":
		lines[0] += " · " + card_name(snapshot, source)
	for entry: Array in [["formation", "环位所属阵法"], ["host", "附着宿主"], ["replace", "替换阵法"], ["target", "目标"], ["discard", "弃牌成本"]]:
		var id: String = str(command.get(entry[0], "0"))
		if id != "0" and not id.is_empty():
			lines.append(str(entry[1]) + "：" + card_name(snapshot, id))
	for entry: Array in [["targets", "目标"], ["materials", "材料成本"]]:
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
				lines.append("连锁目标：" + (card_name(snapshot, link_source) if link_source != "0" else str(entry.get("kindName", "阶段"))))
				break
	return "\n".join(lines)
