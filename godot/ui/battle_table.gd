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

func _ready() -> void:
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
	_chain = snapshot
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
	for event: Dictionary in events:
		var message: String = str(event.get("text", "对局状态已更新"))
		var index: int = int(event.get("chainIndex", 0))
		if index > 0:
			message = "C%d · %s" % [index, message]
		log_text.add_text(message + "\n")
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
