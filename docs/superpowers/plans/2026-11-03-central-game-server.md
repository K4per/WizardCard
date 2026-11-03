# Central Game Server Implementation Plan

> **For agentic workers:** Use executing-plans inline, task by task, then one fresh whole-branch code review. Continue through push and deployment already authorized by the user. Checkbox items track deliverables, not permission gates.

**Goal:** Ship and deploy a Linux central authority for 20 two-player WizardCard rooms and the corresponding Godot product flow.

**Architecture:** Existing C++17 MatchSession remains authoritative. Central room coordination is socket-independent; Asio/OpenSSL runtime owns networking and bounded workers; Godot supplies its native verified TLS transport to ClientRoom. An independent persistence executor writes incremental records and final format-5 replays.

**Tech Stack:** C++17, CMake >=3.28, Asio 1.36.0 pinned commit, OpenSSL >=3, Godot 4.7.2, Python test orchestration.

**Spec:** docs/superpowers/specs/2026-11-03-central-game-server.md

## Global Constraints

- Formal production content: rules/cards 2.6.0, mechanics 10, hash 41eac6fc263abd5e; historical tests stay historical.
- No transport dependency in wizard_application; core-only remains standard-library-only; Godot client does not gain OpenSSL DLL requirements.
- Protocol 5 central, protocol 4 P2P diagnostic; tokens never persisted by client or logged.
- 60-second reconnect, abort without winner, abort unfinished matches on restart.
- Remote branches remain master/dev; user authorized push to dev and deployment via ssh WizardGameServer.

## Review Focus

- Lost Welcome or RoomJoined must not orphan a usable room or allocate duplicate seats; test in task 4.
- Deadline crossings while storage or technical pause is active must not resume an expired match; test in tasks 4/5.
- Partial journal writes and retries must not duplicate records or block unrelated heartbeats; test in task 5.
- Full replay transfer, abort status and forced leave must not reveal unfinished private state or trap UI; test in tasks 5/6.
- Slow/malformed clients and DNS/certificate failures must remain bounded and fail visibly; test in tasks 2/7/8.

### Task 1: Design and build boundaries

- [ ] Save spec, plan, audit and ADR; record old rules documentation supersession.
- [ ] Verify isolated baseline via cmake --build --preset headless and ctest --preset headless --parallel 4.
- [ ] Add optional WIZARD_BUILD_SERVER dependency setup and server-debug/server-release presets as their first consumers are added in tasks 2/7.
- [ ] Verify core-only configure/build and dependency DAG remain valid; commit docs/build boundaries.

### Task 2: Central protocol and public room contracts

Produces RoomConnectOptions, explicit lifecycle state and central Hello/Welcome/CreateRoom/JoinRoom/Resume envelopes plus bounded framing; uses existing game payload codecs.

- [ ] Add failing central protocol tests for compatibility, privacy and fragmented/malformed input; run and observe missing behavior.
- [ ] Implement protocol 5, room options/status and transport seam, keeping P2P wire 4 intact.
- [ ] Run central unit tests and existing codec regression; commit.

### Task 3: Authoritative room registry

Produces socket-independent registry/room authority with per-seat tokens, deck validation, config/readiness, server seed/clock/auto advancement, private projections/queries, bounded room lifetime.

- [ ] Add failing two-remote-seat, owner privilege, cross-room privacy and terminal-match tests.
- [ ] Implement minimal central room authority and queue-facing interfaces.
- [ ] Run central unit tests and existing rules regressions; commit.

### Task 4: Retries and reconnect

Consumes authority from task 3. Produces pre-entry token recovery, per-seat request deduplication, independent disconnect deadlines and terminal resume semantics.

- [ ] Add failing lost entry response, duplicate/different payload, actor impersonation, 59/60 second and simultaneous loss tests.
- [ ] Implement recovery, frozen clocks, waiting-room owner/guest departure and abort metadata.
- [ ] Run central unit tests with controlled monotonic time; commit.

### Task 5: Incremental persistence

Consumes MatchSession and room lifecycle. Produces command/event recording cursors, offset-safe append journal, independent storageBlocked flag, final replay and restart abort scan.

- [ ] Add failing tests for automatic/timeout capture, partial append retry, disk failure/recovery, cumulative-event budget and crash-prefix recovery.
- [ ] Implement bounded asynchronous journal executor and immutable terminal replay generation/validation.
- [ ] Run persistence/unit suite; ensure other rooms advance under a delayed writer; commit.

### Task 6: Godot central client and product flow

Consumes protocol/room contracts. Produces ClientRoom, Application factory migration, Godot native TLS transport and room/match lifecycle UI.

- [ ] Add failing injected central client/application tests and update frontend behavior tests for central rooms/abort/recovery.
- [ ] Implement nonblocking client with queued retry/query/events, asynchronous DNS and verified TLS using original hostname; extend bridge profile.
- [ ] Replace LAN product entry with server/code/create/join; migrate application injection tests while retaining direct P2P regression.
- [ ] Verify Godot import/frontend tests and two-client end-to-end match; commit.

### Task 7: Linux runtime and deployment

Produces wizard_server, native TLS probe, validated config, structured logs, health/readiness, bounded I/O/game/disk execution, graceful stop, container and user-service artifacts.

- [ ] Add failing real TLS, bad CA/hostname, backpressure, handshake/capacity and shutdown tests.
- [ ] Implement Asio/OpenSSL runtime and probe, server-only dependencies and install packaging of formal content.
- [ ] Add container/Compose/user service/config and deployment guide; test rootless local launch and health probe; commit.

### Task 8: CI, capacity, review, push and deployment

- [ ] Add central_smoke orchestration and capacity mode (20 rooms/40 clients, 10 minutes, 2 vCPU/4 GiB baseline, mixed reconnect/slow peers).
- [ ] Run original headless suite, P2P smoke against both card pools, central unit/TLS/Godot integration, catalogue/DAG/core-only checks and capacity report.
- [ ] Build Linux Release and Windows Godot CI jobs; report local versus CI verification accurately.
- [ ] Fresh whole-branch code review; fix important findings with failing regression tests and rerun relevant full suite.
- [ ] Fast-forward local dev, push origin dev, verify remote SHA.
- [ ] Deploy release to WizardGameServer non-destructively; verify TLS, readiness, real remote match and restart-abort semantics. Record release SHA, certificate trust instructions and rollback command.
- [ ] Audit every spec deliverable against authoritative evidence before marking the goal complete.
