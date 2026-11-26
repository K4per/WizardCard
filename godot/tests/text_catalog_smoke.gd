extends SceneTree

const FIXTURE := "res://tests/fixtures/text_catalog"

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var catalogue := root.get_node_or_null("Text")
	if catalogue == null:
		push_error("Text autoload missing")
		quit(1)
		return
	var menu: Control = load("res://pages/menu_page.tscn").instantiate()
	root.add_child(menu)
	if menu.get_node("%SinglePlayer").text != "单人游戏" or GameText.label("terms.damage.fire") != "火焰":
		push_error("Scene labels and card model terms must use the production catalog")
		quit(1)
		return
	menu.queue_free()
	if not catalogue.load_catalog(FIXTURE):
		push_error(str(catalogue.last_error))
		quit(1)
		return
	var data: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(FIXTURE.path_join("cases.json")))
	catalogue.card_names = data.cards
	var keyed := {"phaseNameKey": "terms.phase.casting", "phaseName": "Legacy phase"}
	if GameText.term(keyed, "phaseName") != "施法阶段" or GameText.term({"phaseName": "Legacy phase"}, "phaseName") != "Legacy phase":
		push_error("New projections use local term keys and older projections keep their captions")
		quit(1)
		return
	var ref := {"key": "ui.test.escaped", "args": {"mana": 5}}
	var option_row := {"value": "1", "label": "legacy option", "labelRef": ref}
	var options := ActionOptions.parse({"schema": KernelContract.SCHEMA, "options": [option_row], "costQuote": "legacy cost", "costQuoteRef": ref, "field": 0})
	if options.options.front().label != "{5}" or options.cost_quote != "{5}" or option_row.label != "legacy option":
		push_error("Option and cost references must render without rewriting the received message")
		quit(1)
		return
	var old := ActionOptions.parse({"schema": KernelContract.SCHEMA, "options": [{"value": "1", "label": "legacy option"}], "costQuote": "legacy cost", "field": 0})
	if old.options.front().label != "legacy option" or old.cost_quote != "legacy cost":
		push_error("Older query responses must retain both captions")
		quit(1)
		return
	var table := preload("res://ui/battle_table.gd").new()
	var caption := table._event_caption({"kind": "phase_enter", "text": "legacy event", "textRef": ref}, {})
	table.free()
	if caption != "{5}" or MatchProjection.action_label({}, option_row) != "{5}" or GameText.field({"error": "legacy error", "errorRef": ref}, "error") != "{5}":
		push_error("Event, action and rejection references must take precedence over older captions")
		quit(1)
		return
	var diagnostics: Array[String] = []
	catalogue.diagnostic = func(message: String) -> void: diagnostics.append(message)
	for row: Dictionary in data.cases:
		catalogue.locale = row.locale
		if catalogue.render({"key": row.key, "args": row.args}) != row.expected:
			push_error("Text rendering mismatch in " + str(row.name))
			quit(1)
			return
	if diagnostics.size() != 1 or not diagnostics.front().contains("ui.test.unknown"):
		push_error("Missing text did not emit its diagnostic")
		quit(1)
		return
	quit(0)
