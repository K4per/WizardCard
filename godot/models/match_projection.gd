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
		var command: Dictionary = action.get("command", {})
		var operation: int = int(command.get("type", -1))
		if operation in [0, 18]:
			result.can_set_formation = true
		var source: String = str(action.get("source", "0"))
		if source.is_empty() or source == "0":
			continue
		var highlight: int = 0
		if operation == 15:
			highlight = FieldPresentation.LEGAL_AMBUSH
		elif operation in [0, 2, 3, 4, 5, 6, 12, 16, 17, 18]:
			highlight = FieldPresentation.LEGAL_GOLD
		if highlight != 0:
			result.legal_cards[source] = int(result.legal_cards.get(source, 0)) | highlight
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
	if id in ["1073741825", "1073741826"]:
		return "己方玩家" if id == str(1073741825 + int(snapshot.get("viewer", 0))) else "对方玩家"
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
	var labels: Dictionary[int, String] = {1: "移除阵法", 2: "设置解析", 3: "准备施法", 4: "发动", 6: "附着", 7: "拆除符文", 8: "放弃", 9: "继续", 11: "投降", 12: "响应", 13: "放弃响应", 14: "切换阶段", 15: "盖伏", 16: "反转", 17: "发动", 18: "选择初始阵法"}
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
	if int(command.get("type", -1)) == 1:
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
		lines.append("免费，占用本回合一次阵法操作。")
		if spells.is_empty() and seals == 0:
			lines.append("离场影响：阵法进入灰烬区。")
		else:
			var impact: String = "离场影响：阵法及其承载对象进入灰烬区"
			if not spells.is_empty():
				impact += "（法术：" + "、".join(spells) + "）"
			if seals > 0:
				impact += "；附着符文 %d 张一并进入灰烬区" % seals
			lines.append(impact + "。")
		lines.append("离场效果照常结算，并重新计算环位、荷载上限和魔素收入。")
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

static func formation_removal_unavailable(snapshot: Dictionary, id: String) -> String:
	var viewer: int = int(snapshot.get("viewer", 0))
	var formation: bool = false
	for row: Dictionary in snapshot.get("cards", []):
		if str(row.get("id", "0")) == id:
			formation = int(row.get("owner", -1)) == viewer and int(row.get("zone", -1)) == 3 and int((row.get("definition", {}) as Dictionary).get("type", -1)) == 3
			break
	if not formation:
		return ""
	for action: Dictionary in snapshot.get("actions", []):
		if str(action.get("source", "0")) == id and int((action.get("command", {}) as Dictionary).get("type", -1)) == 1:
			return ""
	if int(snapshot.get("result", -1)) != -1:
		return "对局已结束，不能移除阵法。"
	if bool(snapshot.get("opening", false)):
		return "开局确认完成后，才能进行阵法操作。"
	if int(snapshot.get("phase", -1)) != 2 or int(snapshot.get("activePlayer", -1)) != viewer:
		return "移除阵法仅在己方主要阶段可用。"
	if snapshot.get("decision") is Dictionary or snapshot.get("chain") is Dictionary:
		return "请先完成当前决策或连锁，再移除阵法。"
	return "当前没有合法移除动作；设置与移除共享本回合一次阵法操作。"
