extends Control

@onready var canvas: FieldCanvas = %FieldCanvas
@onready var detail: CardDetail = %Detail
@onready var own: PlayerStatusBar = %OwnStatus
@onready var opponent: PlayerStatusBar = %OpponentStatus
@onready var phase: Label = %Phase
@onready var clock: Label = %Clock
@onready var controls: VBoxContainer = %Controls
@onready var notice: Label = %Notice
@onready var chain_text: RichTextLabel = %ChainText
@onready var log_panel: PanelContainer = %LogPanel
@onready var log_text: RichTextLabel = %LogText

var _resolved: PackedStringArray = []
var _last_multi_chain: String = ""
var _chain: Dictionary = {}
# Older servers send a link id in the event text. Keep only public captions from
# observed links so fast resolution and history redraws retain their declarations.
var _seen_links: Dictionary = {}
var _window_actions: Dictionary = {}
var _opening_action: String = ""
var _preceding_action: String = ""
var _log_phase: String = ""

func _ready() -> void:
	# The exported instance can start with zero anchors even when its page fills the window.
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	detail.clear_card()
	own.custom_minimum_size.x = 0
	opponent.custom_minimum_size.x = 0
	resized.connect(arrange)
	own.minimum_size_changed.connect(arrange)
	opponent.minimum_size_changed.connect(arrange)
	arrange()

func arrange() -> void:
	if detail == null:
		return
	var gap: float = 8.0
	var stats_width: float = 224.0 * maxf(1.0, Typography.scale_percent / 100.0)
	var left: float = maxf(clampf(size.x * 0.161, stats_width, 306), own.get_combined_minimum_size().x)
	var right: float = maxf(clampf(size.x * 0.175, stats_width, 332), opponent.get_combined_minimum_size().x)
	own.size = Vector2(left, maxf(76, own.get_combined_minimum_size().y))
	opponent.size = Vector2(right, maxf(76, opponent.get_combined_minimum_size().y))
	own.position = Vector2(gap, size.y - own.size.y - gap)
	opponent.position = Vector2(size.x - right - gap, gap)
	detail.position = Vector2(gap, gap)
	detail.size = Vector2(left, own.position.y - gap * 2.0)
	canvas.position = Vector2(left + gap * 2.0, gap)
	canvas.size = Vector2(opponent.position.x - gap - canvas.position.x, size.y - gap * 2.0)
	var rail: VBoxContainer = %RightRail
	rail.position = Vector2(opponent.position.x, opponent.get_rect().end.y + gap)
	rail.size = Vector2(right, size.y - rail.position.y - gap)
	detail._art.custom_minimum_size = Vector2(0, minf(left * 1.04, detail.size.y * 0.43))

func show_frame(snapshot: Dictionary) -> void:
	if snapshot.get("viewer") != _chain.get("viewer") or snapshot.get("generation") != _chain.get("generation"):
		_seen_links.clear()
		_window_actions.clear()
		_opening_action = ""
		_preceding_action = ""
		_log_phase = ""
	_chain = snapshot
	_remember_public_links(snapshot)
	var player: int = int(snapshot.get("activePlayer", 0))
	var players: Array = snapshot.get("players", [])
	var turn: int = int(players[player].get("ownTurn", 0)) if player >= 0 and player < players.size() else 0
	var decision: Variant = snapshot.get("decision")
	var waiting: int = int(snapshot.get("actingPlayer", player))
	if snapshot.get("chain") is Dictionary:
		waiting = int(snapshot.chain.get("priority", waiting))
	if decision is Dictionary:
		waiting = int(decision.get("player", waiting))
	phase.text = "玩家 %d · 第 %d 个己方回合\n%s · 等待玩家 %d" % [player + 1, turn, str(snapshot.get("phaseName", "开局")), waiting + 1]
	if bool(snapshot.get("opening", false)):
		var gestures: bool = str(snapshot.get("openingStage", "")) == "gestures"
		var confirmed: Array = snapshot.get("gestureConfirmed" if gestures else "initialFormationConfirmed", [])
		var prompt: String = "等待双方确认"
		if confirmed.size() == 2 and bool(confirmed[0]) != bool(confirmed[1]):
			prompt = "等待玩家 %d" % [2 if bool(confirmed[0]) else 1]
		var stage: String = "猜拳第 %d 轮" % int(snapshot.get("openingRound", 1)) if gestures else "选择初始阵法"
		phase.text = "开局 · %s\n%s" % [stage, prompt]
	if int(snapshot.get("result", -1)) >= 0:
		phase.text = "对局结束 · " + ("平局" if int(snapshot.result) > 1 else "玩家 %d 获胜" % [int(snapshot.result) + 1])
	_render_chain()

func update_events(events: Array, reset: bool) -> void:
	if reset:
		log_text.clear()
		_resolved.clear()
		_last_multi_chain = ""
		_window_actions.clear()
		_opening_action = ""
		_preceding_action = ""
		_log_phase = ""
	for position: int in events.size():
		var event: Dictionary = events[position]
		var index: int = int(event.get("chainIndex", 0))
		var following: Dictionary = events[position + 1] if position + 1 < events.size() else {}
		log_text.add_text(_event_caption(event, following) + "\n")
		match str(event.get("kind", "")):
			"chain_open":
				_resolved.clear()
			"link_resolved", "link_skipped":
				if index > 0:
					_resolved.append("C%d%s" % [index, "（失效）" if event.kind == "link_skipped" else ""])
			"chain_closed":
				if _resolved.size() > 1:
					_last_multi_chain = " → ".join(_resolved)
				_resolved.clear()
	_render_chain()

func _public_card(source: String) -> Dictionary:
	for card: Dictionary in _chain.get("cards", []):
		if str(card.get("id", "")) == source:
			return card
	return {}

func _legacy_action(action: String, source: String, actor: int = -1, show_name: bool = true) -> String:
	var card: Dictionary = _public_card(source)
	if actor < 0:
		actor = int(card.get("controller", card.get("owner", -1)))
	var name_text: String = str(card.get("definition", {}).get("name", ""))
	var public_zone: bool = int(card.get("zone", -1)) in [2, 3, 4, 5, 6, 7, 8, 10]
	if show_name and not name_text.is_empty() and not bool(card.get("hidden", false)) and not bool(card.get("faceDown", false)) and public_zone and action != "盖伏法术":
		action += "《%s》" % name_text
	return ("玩家 %d " % (actor + 1) if actor in [0, 1] else "") + action

func _remember_public_links(snapshot: Dictionary) -> void:
	var chain: Variant = snapshot.get("chain")
	if not chain is Dictionary:
		return
	for link: Dictionary in chain.get("links", []):
		var id: String = str(link.get("id", ""))
		if id.is_empty() or _seen_links.has(id):
			continue
		var source: String = str(link.get("source", "0"))
		var action: String = str(link.get("kindName", "行动"))
		if action == "阶段":
			if source != "0":
				action = "盖伏法术"
			elif int(chain.get("window", -1)) in [0, 1]:
				action = str(snapshot.get("phaseName", "")) + str(chain.get("windowName", "阶段时点"))
			else:
				action = "行动后的响应时点"
		var actor: int = int(link.get("owner", -1))
		_seen_links[id] = {"actor":actor, "action":_legacy_action(action, source, actor)}
		if _seen_links.size() > MatchEventLog.CAPACITY:
			_seen_links.erase(_seen_links.keys()[0])

func _old_link_id(message: String) -> String:
	return message.get_slice("链节 #", 1) if message.contains("链节 #") else ""

func _event_caption(event: Dictionary, following: Dictionary) -> String:
	var kind: String = str(event.get("kind", ""))
	var message: String = str(event.get("text", "对局状态已更新"))
	var source: String = str(event.get("card", "0"))
	var index: int = int(event.get("chainIndex", 0))
	var previous: String = _preceding_action
	_preceding_action = ""
	if kind == "phase_enter" and message.begins_with("进入"):
		_log_phase = message.trim_prefix("进入").trim_suffix("阶段")
	if kind in ["ambush", "formation_set"]:
		_preceding_action = message if message.begins_with("玩家") else _legacy_action(message, source)
		return _preceding_action
	if kind == "chain_open":
		_window_actions.clear()
		_opening_action = message.trim_prefix("建立连锁：")
		if _opening_action.begins_with("玩家"):
			return message
		if not previous.is_empty() and _opening_action == "行动宣告":
			_opening_action = previous
		elif following.get("kind") == "link_declared":
			var id: String = _old_link_id(str(following.get("text", "")))
			var first: String = str(following.get("card", "0"))
			if _seen_links.has(id):
				_opening_action = _seen_links[id].action
			elif _opening_action in ["阶段开始", "阶段结束"]:
				_opening_action = _log_phase + _opening_action
			else:
				# Only actions which openly declare their source can recover its
				# identity from a later frame. A trigger may have a private source.
				var known_public: bool = _opening_action in ["准备施法", "释放法术"]
				_opening_action = _legacy_action(_opening_action, first, -1, known_public)
		return "建立连锁：" + _opening_action
	var id: String = _old_link_id(message)
	if kind == "link_declared" and not id.is_empty():
		var action: String = _opening_action if index == 1 else ""
		if action.is_empty():
			action = str(_seen_links[id].action) if _seen_links.has(id) else (_legacy_action("发动响应", source) if index > 1 else "")
		if action.is_empty():
			action = _legacy_action("行动", source, -1, false)
		_window_actions[index] = action
		return "宣告：" + action
	if kind in ["cast", "word_response", "prepare_declared", "flip"] and not message.contains("玩家"):
		var action: String = _legacy_action(message.trim_prefix("宣告"), source)
		if index > 0:
			_window_actions[index] = action
		return "宣告：" + action
	if kind in ["link_resolved", "link_skipped"] and not id.is_empty():
		var action: String = str(_window_actions.get(index, _seen_links[id].action if _seen_links.has(id) else "行动"))
		return ("结算：" if kind == "link_resolved" else "取消或失效：") + action
	if kind == "pay" and not message.contains("玩家"):
		var payment := "支付 %d 魔素" % int(event.get("amount", 0)) if message == "支付魔素" else message
		var next_link := _old_link_id(str(following.get("text", "")))
		var payer: int = int(_seen_links.get(next_link, {}).get("actor", -1))
		if payer in [0, 1] and str(following.get("card", "0")) == source:
			return "玩家 %d %s" % [payer + 1, payment]
		return payment
	# Draw events are delivered only to the player who drew the card.
	if kind == "draw" and not message.contains("玩家") and int(_chain.get("viewer", -1)) in [0, 1]:
		return "玩家 %d %s" % [int(_chain.viewer) + 1, message]
	if kind == "chain_closed":
		_preceding_action = "进入连锁处理后的响应时点"
		_window_actions.clear()
		_opening_action = ""
	return message

func _render_chain() -> void:
	chain_text.clear()
	var chain: Variant = _chain.get("chain")
	if chain is Dictionary:
		var links: Array = chain.get("links", [])
		var resolving: bool = int(chain.get("mode", 0)) == 1
		for index: int in range(links.size()):
			var link: Dictionary = links[index]
			var caption: String = str(link.get("kindName", "阶段")) if str(link.get("source", "0")) == "0" else MatchProjection.card_name(_chain, str(link.source))
			var current: bool = resolving and index == links.size() - 1
			if current:
				chain_text.push_color(Color("f3ce80"))
			chain_text.add_text("%sC%d · %s%s\n" % ["正在结算 " if current else "", index + 1, caption, "（无效）" if bool(link.get("canceled", false)) else ""])
			if current:
				chain_text.pop()
		if resolving and not links.is_empty():
			chain_text.scroll_to_paragraph(links.size() - 1)
		if links.is_empty():
			chain_text.add_text("等待响应")
	else:
		chain_text.add_text("当前无连锁")
	if not _last_multi_chain.is_empty():
		chain_text.add_text("\n最近多节结算  " + _last_multi_chain)

func show_clock(state: Dictionary, viewer: int, elapsed_ms: int) -> void:
	if not bool(state.get("enabled", false)):
		clock.text = "未计时"
		return
	var remaining: Array = state.get("remainingMs", [])
	var running: Array = state.get("runningPlayers", [])
	var lines: PackedStringArray = []
	for player: int in [viewer, 1 - viewer]:
		var active: bool = bool(running[player]) if running.size() > player else int(state.get("runningPlayer", -1)) == player
		var ms: int = int(remaining[player]) if remaining.size() > player else 0
		if active and bool(state.get("running", false)):
			ms -= elapsed_ms
		var seconds: int = maxi(0, ceili(ms / 1000.0))
		lines.append("%s %02d:%02d" % ["己" if player == viewer else "对", seconds / 60, seconds % 60])
	clock.text = "\n".join(lines)

func place_casting_control(control: Control, left: bool) -> void:
	var bounds: Rect2 = canvas.casting_control_rect(left)
	if bounds.has_area():
		control.size = Vector2(bounds.size.x, minf(54, bounds.size.y))
		control.global_position = canvas.global_position + bounds.get_center() - control.size * 0.5
