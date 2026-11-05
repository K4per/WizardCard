extends SceneTree

class RejectingService extends FrontendService:
	var lost: int = 0
	var received: Variant

	func network_transport_message(message: Variant) -> Dictionary:
		received = message
		return {"ok": false, "error": "rejected server message"}

	func network_transport_lost(_reason: String) -> Dictionary:
		lost += 1
		return {"ok": true}

func _initialize() -> void:
	var service: RejectingService = RejectingService.new()
	var transport: CentralTlsTransport = CentralTlsTransport.new()
	transport._service = service
	transport._phase = "open"
	var wire_message: String = '{"type":"ActionOptions","options":{"complete":{"type":18,"card":42}}}'
	var payload: PackedByteArray = wire_message.to_utf8_buffer()
	transport._receive = PackedByteArray([0, 0, 0, payload.size()])
	transport._receive.append_array(payload)
	var decoded: bool = transport._decode_frames()
	var passed: bool = not decoded and transport._phase == "idle" and service.lost == 1
	var exact_payload: bool = service.received is String and service.received == wire_message
	if not passed:
		push_error("CENTRAL_TRANSPORT: message rejection must stop the TLS receive loop")
	if not exact_payload:
		push_error("CENTRAL_TRANSPORT: pass raw JSON to native decoding without converting protocol integers to floats")
	print("CENTRAL_TRANSPORT: rejected message stops receive=", passed)
	print("CENTRAL_TRANSPORT: exact JSON payload=", exact_payload)
	quit(0 if passed and exact_payload else 1)
