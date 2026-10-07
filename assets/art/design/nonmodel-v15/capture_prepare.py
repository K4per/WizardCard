"""Create a separate art review capture; keep the original regression script intact."""
from pathlib import Path
P=Path(__file__).resolve().parents[4]
source=(P/'godot/tests/capture_match_25d.gd').read_text(encoding='utf-8')
source=source.replace('\tawait snap("opening-1280x720")','''\tawait snap("opening-1280x720")
\t# Review fixture: hide cards without changing the authoritative match.
\tfor node: WizardCard3D in client.match_scene.board.cards.values():
\t\tnode.visible = false
\tclient.match_scene.hand.hide()
\tfor zone: WizardZone3D in client.match_scene.board.zones.values():
\t\tzone.configure(client.skin, 0)
\tfor dimensions: Vector2i in [Vector2i(1280, 720), Vector2i(1600, 1000), Vector2i(1920, 1080)]:
\t\tviewport.size = dimensions
\t\tawait snap("empty-%dx%d" % [dimensions.x, dimensions.y])
\tclient.match_scene.hand.show()
\tclient.refresh_match()''')
source=source.replace('\tclient.match_scene.browse_zone(0, 3)','''\t# HUD review at all three supported resolutions with real replay cards.
\tclient.inspected = client._find_card(client.hand_order[0]) if not client.hand_order.is_empty() else {}
\tclient.match_scene._log_pinned = true
\tclient.refresh_match()
\tclient.match_scene.reveal_details()
\tfor dimensions: Vector2i in [Vector2i(1280, 720), Vector2i(1600, 1000), Vector2i(1920, 1080)]:
\t\tviewport.size = dimensions
\t\tawait snap("busy-hud-%dx%d" % [dimensions.x, dimensions.y])
\tfor node: WizardCard3D in client.match_scene.board.cards.values():
\t\tnode.material.shading_mode = BaseMaterial3D.SHADING_MODE_PER_PIXEL
\tawait snap("card-face-lit-comparison")
\tfor node: WizardCard3D in client.match_scene.board.cards.values():
\t\tnode.material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
\tawait snap("card-face-unlit-comparison")
\tclient.match_scene.browse_zone(0, 3)''')
source=source.replace('range(180)', 'range(30)')
source=source.replace('MATCH_25D_CAPTURE', 'ART_NONMODEL_CAPTURE')
source=source.replace('static replay step 107, not full gameplay or native DPI acceptance','static art review, 30 samples; not full gameplay or native DPI acceptance')
(P/'godot/tests/capture_art_nonmodel.gd').write_text(source,encoding='utf-8')
