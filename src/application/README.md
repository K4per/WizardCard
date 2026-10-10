# Application and sessions

`Session` is the shared projection/submission interface. `MatchSession` is the
local adapter; `RoomSession` adds room lifecycle controls and `net::Peer` is the
TCP adapter. Pending/rejected commands never count as authority acceptance.
Network `tick` continues while a settings page or animation is active.

`Application` receives an optional `RoomFactory` from the composition root.
The SFML client supplies `net::createRoom`; CLI and local bridge do not need TCP.
The application library neither includes the concrete Peer nor links to the
network library. It owns generation/revision checks, AI/tutorial orchestration,
settings, deck drafts and transactional save/leave/restart handling. `match()`
is the retained legacy offline verification accessor; new presentation code
uses `viewFor` and `submit`.

Dependencies: core/replay/content/AI. Tests: application, decks, AI, session and
network cases, including polling while settings are open and identical replays.
