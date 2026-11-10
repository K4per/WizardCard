extends Node

var _app: WizardFrontend

func _ready() -> void:
	_run.call_deferred()

func _run() -> void:
	await get_tree().process_frame
	_app.navigate(&"guide")
	await get_tree().process_frame
	var page: GuidePage = _app.page as GuidePage
	var passed: bool = _app.service.error.is_empty() and page.tabs.get_tab_count() == 3
	passed = passed and page.chapter_buttons.size() == 5 and page._library._grid.get_child_count() == 90
	page.tabs.current_tab = 1
	for index: int in range(page._rule_chapters.item_count):
		page._read_rule(index)
		passed = passed and page._rule_text.text.length() > 30
	page.tabs.current_tab = 2
	var certificate: X509Certificate = X509Certificate.new()
	passed = passed and certificate.load(RoomsPage.PRODUCTION_CA) == OK
	var report: Dictionary = {"passed": passed, "chapters": page.chapter_buttons.size(),
		"ruleSections": page._rule_chapters.item_count, "cards": page._library._grid.get_child_count(),
		"bundledCertificate": true, "networkUsed": _app.service.network_active}
	var arguments: PackedStringArray = OS.get_cmdline_user_args()
	for index: int in range(arguments.size() - 1):
		if arguments[index] == "--report":
			var file: FileAccess = FileAccess.open(arguments[index + 1], FileAccess.WRITE)
			if file == null:
				passed = false
			else:
				file.store_string(JSON.stringify(report))
				file.close()
	print("OFFLINE_GUIDE: ", JSON.stringify(report))
	get_tree().quit(0 if passed else 1)
