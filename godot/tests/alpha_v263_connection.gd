extends SceneTree

var failures: int = 0

func _initialize() -> void:
	_run.call_deferred()

func check(value: bool, message: String) -> void:
	if not value:
		failures += 1
		push_error("V263_CONNECTION: " + message)

func _run() -> void:
	var page: RoomsPage = load("res://pages/rooms_page.tscn").instantiate() as RoomsPage
	root.add_child(page)
	await process_frame
	check(page._address.text == "aq.rainplay.cn" and int(page._port.value) == 23513, "normal entry defaults to production")
	var certificate: X509Certificate = X509Certificate.new()
	check(page._trusted_ca.text.begins_with("res://") and certificate.load(page._trusted_ca.text) == OK, "public trust certificate is bundled offline")
	check(not page._address.is_visible_in_tree() and not page._host_first.visible, "advanced address and fixed first player are hidden")
	check(page.has_method("_restore_production"), "production defaults can be restored")
	if page.has_method("_restore_production"):
		page._address.text = "invalid.example"
		page._port.value = 12345
		page._trusted_ca.text = "wrong.crt"
		page.call("_restore_production")
		check(page._address.text == "aq.rainplay.cn" and int(page._port.value) == 23513 and certificate.load(page._trusted_ca.text) == OK, "restore resets endpoint and trusted public certificate")
	page.queue_free()
	await process_frame
	print("Alpha v2.6.3 connection failures: ", failures)
	quit(0 if failures == 0 else 1)
