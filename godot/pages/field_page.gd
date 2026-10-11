class_name FieldPage
extends Control

signal navigation_requested(page: StringName)

var canvas: FieldCanvas
var _detail: CardDetail
var _decision: DecisionPanel
var _privacy_button: Button
var _hand_button: Button
var _notice: Label
var _modal_shade: ColorRect
var _actions: CardActionMenu
var _active_card: CardPresentation

func _ready() -> void:
	var margin: MarginContainer = UiFactory.margin(self, 16)
	var column: VBoxContainer = VBoxContainer.new()
	column.add_theme_constant_override(&"separation", 8)
	margin.add_child(column)
	var header: HBoxContainer = UiFactory.heading(column, "场地预览", navigation_requested.emit.bind(&"menu"))
	var status: Label = UiFactory.label("地下城场地 · 展示数据", 16)
	status.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	status.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	header.add_child(status)
	canvas = FieldCanvas.new()
	canvas.size_flags_vertical = Control.SIZE_EXPAND_FILL
	column.add_child(canvas)
	canvas.card_selected.connect(_show_detail)
	canvas.selection_cleared.connect(_close_detail)
	canvas.hand_toggle_requested.connect(_toggle_hand)
	canvas.placement_requested.connect(_placement_preview)
	var footer: HBoxContainer = HBoxContainer.new()
	column.add_child(footer)
	_notice = UiFactory.label("展示数据 · 右键收回选中牌，再按收起手牌", 16, true)
	footer.add_child(_notice)
	_hand_button = UiFactory.button("收起手牌", _toggle_hand)
	BattleArt.button(_hand_button)
	footer.add_child(_hand_button)
	_privacy_button = UiFactory.button("交接遮挡", _toggle_privacy)
	BattleArt.button(_privacy_button)
	footer.add_child(_privacy_button)
	var effect_button: Button = UiFactory.button("效果选择示例", _show_decision)
	BattleArt.button(effect_button)
	footer.add_child(effect_button)
	_detail = CardDetail.new()
	_detail.battle_skin = true
	add_child(_detail)
	_detail.set_anchors_and_offsets_preset(Control.PRESET_RIGHT_WIDE)
	_detail.offset_left = -376
	_detail.offset_right = -24
	_detail.offset_top = 88
	_detail.offset_bottom = -88
	_detail.closed.connect(_close_detail)
	_detail.hide()
	_actions = CardActionMenu.new()
	add_child(_actions)
	_actions.size = Vector2(300, 70)
	_actions.effect_preview_requested.connect(_show_decision)
	_actions.hide()
	canvas.resized.connect(_position_actions)
	canvas.layout_changed.connect(_position_actions)
	_modal_shade = ColorRect.new()
	add_child(_modal_shade)
	_modal_shade.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_modal_shade.color = Color(0.031, 0.075, 0.122, 0.55)
	_modal_shade.mouse_filter = Control.MOUSE_FILTER_STOP
	_modal_shade.hide()
	_decision = DecisionPanel.new()
	add_child(_decision)
	_decision.set_anchors_and_offsets_preset(Control.PRESET_CENTER)
	_decision.offset_left = -280
	_decision.offset_right = 280
	_decision.offset_top = -220
	_decision.offset_bottom = 220
	_decision.confirmed.connect(_decision_confirmed)
	_decision.cancelled.connect(_decision_cancelled)
	_decision.hide()
	resized.connect(_layout_panels)
	_layout_panels()

func _layout_panels() -> void:
	if _decision == null:
		return
	var extent: Vector2 = Vector2(minf(1240, size.x * 0.90), minf(650, size.y * 0.72))
	_decision.offset_left = -extent.x * 0.5
	_decision.offset_right = extent.x * 0.5
	_decision.offset_top = -extent.y * 0.5
	_decision.offset_bottom = extent.y * 0.5
	var detail_width: float = clampf(size.x * 0.26, 350, 500)
	var detail_height: float = detail_width * 0.51 / 0.75
	_detail.offset_left = -detail_width - 24
	_detail.offset_top = size.y * 0.16
	_detail.offset_bottom = _detail.offset_top + detail_height - size.y
	_position_actions()

func configure(service: FrontendService) -> void:
	canvas.configure(FieldShowcase.create(service.cards))
	if not service.error.is_empty():
		_notice.text = service.error

func _show_detail(card: CardPresentation) -> void:
	if canvas.privacy or _decision.visible:
		return
	_active_card = card
	canvas.selected_id = card.instance_id
	canvas.queue_redraw()
	_detail.configure(card)
	_detail.show()
	_position_actions()

func _position_actions() -> void:
	if _active_card == null or _actions == null or canvas.privacy or _decision.visible:
		return
	var bounds: Rect2 = canvas.card_bounds(_active_card)
	_actions.visible = bounds.has_area()
	if not bounds.has_area():
		return
	var point: Vector2 = canvas.global_position - global_position + Vector2(bounds.get_center().x - _actions.size.x * 0.5, bounds.position.y - _actions.size.y - 2)
	_actions.position = Vector2(clampf(point.x, 16, size.x - _actions.size.x - 16), maxf(70, point.y))
	_actions._effect.disabled = _active_card.hidden

func _close_detail() -> void:
	_detail.hide()
	_actions.hide()
	_active_card = null
	canvas.selected_id = ""
	canvas.queue_redraw()
	if canvas.visible and canvas.focus_mode != Control.FOCUS_NONE:
		canvas.grab_focus()

func dismiss_cancel() -> bool:
	# Esc first dismisses transient field UI. The root window uses this as a
	# fallback when Godot routes the key past the page's unhandled callback.
	if _decision.visible:
		if _decision.request != null and _decision.request.may_cancel:
			_decision._on_cancel(_decision.request.generation, _decision.request.revision, _decision.request.decision_id)
		return true
	if _detail.visible or _actions.visible or not canvas.selected_id.is_empty():
		_close_detail()
		return true
	return false

func _toggle_privacy() -> void:
	canvas.set_privacy(not canvas.privacy)
	_close_detail()
	_decision.hide()
	_modal_shade.hide()
	canvas.focus_mode = Control.FOCUS_NONE if canvas.privacy else Control.FOCUS_ALL
	canvas.mouse_filter = Control.MOUSE_FILTER_STOP
	_privacy_button.text = "揭开场地" if canvas.privacy else "交接遮挡"
	_hand_button.disabled = canvas.privacy

func _toggle_hand() -> void:
	if canvas.privacy or _decision.visible:
		return
	canvas.set_hand_collapsed(not canvas.hand_collapsed)
	_hand_button.text = "展开手牌" if canvas.hand_collapsed else "收起手牌"

func _placement_preview(_card_id: String, _host_id: String, ring_index: int) -> void:
	_notice.text = "选择了环位 %d · 展示页不会支付费用或改变卡牌位置" % (ring_index + 1)

func _show_decision() -> void:
	if canvas.privacy:
		return
	var request: DecisionPresentation = DecisionPresentation.new()
	request.generation = "9007199254740993"
	request.revision = "12"
	request.decision_id = "18446744073709551615"
	request.title = "效果检索 · 选择卡牌"
	request.description = "从候选卡牌中选择1张，点选可查看详情。"
	for card: CardPresentation in canvas.field.hand:
		var choice: DecisionChoice = DecisionChoice.new()
		choice.id = card.instance_id
		choice.label = card.title
		choice.card = card
		request.choices.append(choice)
	_decision.configure(request)
	_modal_shade.show()
	_decision.show()
	canvas.mouse_filter = Control.MOUSE_FILTER_IGNORE
	canvas.focus_mode = Control.FOCUS_NONE
	_detail.hide()
	_actions.hide()

func _decision_confirmed(_generation: String, _revision: String, _decision_id: String, _selected: Array[String]) -> void:
	_notice.text = "选择已确认 · 本展示未提交规则命令"
	_decision.hide()
	_modal_shade.hide()
	canvas.mouse_filter = Control.MOUSE_FILTER_STOP
	canvas.focus_mode = Control.FOCUS_ALL

func _decision_cancelled(_generation: String, _revision: String, _decision_id: String) -> void:
	_notice.text = "已取消 · 卡牌与费用保持原状"
	_decision.hide()
	_modal_shade.hide()
	canvas.mouse_filter = Control.MOUSE_FILTER_STOP
	canvas.focus_mode = Control.FOCUS_ALL

func _unhandled_key_input(event: InputEvent) -> void:
	if not event.is_action_pressed(&"ui_cancel"):
		return
	if dismiss_cancel():
		get_viewport().set_input_as_handled()

func _unhandled_input(event: InputEvent) -> void:
	if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_RIGHT:
		if not canvas.privacy and not _decision.visible:
			if canvas.selected_id.is_empty():
				_toggle_hand()
			else:
				_close_detail()
			get_viewport().set_input_as_handled()
