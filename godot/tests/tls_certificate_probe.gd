extends SceneTree

class ProbeService extends FrontendService:
	var endpoint: Dictionary
	var opened: bool = false
	var reason: String = ""
	func network_transport_state() -> Dictionary:
		return {"ok": true, "data": endpoint}
	func network_transport_opened() -> Dictionary:
		opened = true
		return {"ok": true}
	func network_transport_drain() -> Dictionary:
		return {"ok": true}
	func network_transport_lost(message: String) -> Dictionary:
		reason = message
		return {"ok": true}

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var arguments: PackedStringArray = OS.get_cmdline_user_args()
	var cases: Array = JSON.parse_string(FileAccess.get_file_as_string(arguments[0])) as Array
	var service: ProbeService = ProbeService.new()
	service.network_active = true
	var transport: CentralTlsTransport = CentralTlsTransport.new()
	var failures: int = 0
	for test: Dictionary in cases:
		service.opened = false
		service.reason = ""
		service.endpoint = {"active": true, "external": true, "open": true,
			"hostname": test["host"], "port": test["port"], "trustedCa": test["ca"]}
		var deadline: int = Time.get_ticks_msec() + 15000
		while not service.opened and service.reason.is_empty() and Time.get_ticks_msec() < deadline:
			transport.pump(service)
			await create_timer(0.01).timeout
		var passed: bool = service.opened == bool(test["accepted"]) and (service.opened or not service.reason.is_empty())
		if not bool(test["accepted"]):
			passed = passed and service.reason.contains(str(test.get("reason", "证书校验失败")))
		if not passed:
			failures += 1
		print("TLS_CERTIFICATE: ", test["name"], " passed=", passed, " accepted=", service.opened, " reason=", service.reason)
		transport.stop()
	transport.stop()
	quit(0 if failures == 0 else 1)
