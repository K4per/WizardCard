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
	if not catalogue.load_catalog(FIXTURE):
		push_error(str(catalogue.last_error))
		quit(1)
		return
	var data: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(FIXTURE.path_join("cases.json")))
	catalogue.card_names = data.cards
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
