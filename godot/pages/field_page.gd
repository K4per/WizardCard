class_name FieldPage
extends Control

signal navigation_requested(page: StringName)

var canvas: FieldCanvas
var _detail: CardDetail
var _decision: DecisionPanel
var _privacy_button: Button
var _notice: Label
var _modal_shade: ColorRect

func _ready() -> void:
	var margin: MarginContainer = UiFactory.margin(self, 16)
	var column: VBoxContainer = VBoxContainer.new()
	column.add_theme_constant_override(&"separation", 8)
	margin.add_child(column)
	var header: HBoxContainer = UiFactory.heading(column, "场地预览", navigation_requested.emit.bind(&"menu"))
	var status: Label = UiFactory.label("对手 40  ·  魔素 1       主要阶段       我方 40  ·  魔素 1", 16)
	status.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	status.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	header.add_child(status)
	canvas = FieldCanvas.new()
	canvas.size_flags_vertical = Control.SIZE_EXPAND_FILL
	column.add_child(canvas)
	canvas.card_selected.connect(_show_detail)
	canvas.placement_requested.connect(_placement_preview)
	var footer: HBoxContainer = HBoxContainer.new()
	column.add_child(footer)
	_notice = UiFactory.label("展示数据 · 点击卡牌查看详情；拖动只展示落点意图", 16, true)
	footer.add_child(_notice)
	_privacy_button = UiFactory.button("交接遮挡", _toggle_privacy)
	footer.add_child(_privacy_button)
	footer.add_child(UiFactory.button("选择材料示例", _show_decision))
	_detail = CardDetail.new()
	add_child(_detail)
	_detail.set_anchors_and_offsets_preset(Control.PRESET_RIGHT_WIDE)
	_detail.offset_left = -376
	_detail.offset_right = -24
	_detail.offset_top = 88
	_detail.offset_bottom = -88
	_detail.closed.connect(_detail.hide)
	_detail.hide()
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

func configure(service: FrontendService) -> void:
	canvas.configure(FieldShowcase.create(service.cards))
	if not service.error.is_empty():
		_notice.text = service.error

func _show_detail(card: CardPresentation) -> void:
	if canvas.privacy:
		return
	_detail.configure(card)
	_detail.show()
	_decision.hide()

func _toggle_privacy() -> void:
	canvas.set_privacy(not canvas.privacy)
	_detail.hide()
	_decision.hide()
	_modal_shade.hide()
	canvas.focus_mode = Control.FOCUS_NONE if canvas.privacy else Control.FOCUS_ALL
	canvas.mouse_filter = Control.MOUSE_FILTER_STOP
	_privacy_button.text = "揭开场地" if canvas.privacy else "交接遮挡"

func _placement_preview(_card_id: String, _host_id: String, ring_index: int) -> void:
	_notice.text = "选择了环位 %d · 展示页不会支付费用或改变卡牌位置" % (ring_index + 1)

func _show_decision() -> void:
	if canvas.privacy:
		return
	var request: DecisionPresentation = DecisionPresentation.new()
	request.generation = "9007199254740993"
	request.revision = "12"
	request.decision_id = "18446744073709551615"
	request.title = "材料选择 · 控件示例"
	request.description = "选择一项以查看确认状态；取消不会消耗任何卡牌。"
	for card: CardPresentation in canvas.field.hand:
		var choice: DecisionChoice = DecisionChoice.new()
		choice.id = card.instance_id
		choice.label = card.title
		request.choices.append(choice)
	_decision.configure(request)
	_modal_shade.show()
	_decision.show()
	canvas.mouse_filter = Control.MOUSE_FILTER_IGNORE
	canvas.focus_mode = Control.FOCUS_NONE
	_detail.hide()

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
	if _decision.visible:
		_decision_cancelled("", "", "")
		get_viewport().set_input_as_handled()
	elif _detail.visible:
		_detail.hide()
		get_viewport().set_input_as_handled()
