# Alpha v2.6.3 已确认设计

用户于2026-11-10确认并要求实施。完整原始需求随本文件同目录保存在 alpha-v263-requirements.md，以下裁定优先于原文中的待确认项。

## 方案
沿用原有C++权威内核、查看者投影、Godot场景和打包工具逐层升级；不新增第二套UI规则。新状态和命令需要显式升级复盘格式，旧卡池保留隔离回归。

## 已确认裁定

```json
{
  "version": "2.6.3",
  "cards": 90,
  "retire": [
    "v260-formation-0014",
    "v260-formation-0015"
  ],
  "oldDeckMigration": "backup then remove retired cards; require player to fill gaps; never auto-select replacements",
  "setupDelay": {
    "timing": "only subsequent settings until end of current turn",
    "source": "all sources; exclude relocation, return-to-ring, control transfer"
  },
  "recipeFaceDown": "any currently controlled own analysis-zone face-down spell including analytic, word, talisman, special source, borrowed ownership, no flip-wait requirement",
  "castingSlots": {
    "rounding": "floor",
    "overflow": "controller chooses exact excess cards to ash after whole current chain resolves based on final capacity; block actions/progression until complete",
    "placement": "automatic",
    "stacking": "each active effect halves sequentially with floor after each; independent expiration"
  },
  "engeas": "first face-up setting or flip of analytic each global turn automatically reduces remaining analysis by1 clamped at0 and consumes once-per-formation opportunity; remove legacy extra load",
  "hostRestrictions": "face-down bypasses; check face-up setting/flip; chaos counts other face-up cards only",
  "fireEligibility": "must include fixed fire damage in card effects; selectable damage not sufficient; same predicate for sulfur hosting and recipe",
  "server": {
    "host": "aq.rainplay.cn",
    "port": 23513,
    "deploy": "upgrade same production server, backup and no live matches before switch, public two-client acceptance",
    "trust": "embed existing public cert; updates in release; hostname/expiry/chain checks; no private key",
    "advanced": "collapsed custom host/port/certificate plus reset to official defaults"
  },
  "tutorial": {
    "access": "all five available",
    "progress": "local chapter completion only; replay from chapter start; no midchapter save",
    "initial": "purpose-built fixed scenarios, sufficient resources, legal formations; chapter2 separates free initial selection and paid recipe; use real authoritative rules"
  },
  "layout": {
    "center": "two casting strips, one per player",
    "hand": "own hand independent bottom; opponent backs/count top",
    "baseline": "1920x1080",
    "minimum": "1280x720",
    "acceptance": [
      "1280x720",
      "1600x900",
      "1920x1080"
    ],
    "other": "scale to higher resolutions; retain key info in non16:9"
  },
  "response": {
    "default": "AUTO renamed 可用时点",
    "ignore": "until player manually changes it within current match; persistent visible state; never skip mandatory decisions"
  },
  "rarity": "preserve existing values for blank sheet cells",
  "rank": "face-up setting and flip require host level >= spell rank; ambush placement bypasses",
  "rules": {
    "rank": "face-up setting and flip require host level >= spell rank; ambush placement bypasses",
    "persistent": "after chain persistent spells return to a legal analysis ring; no legal ring => ash; ordinary resolved spells remain casting until end cleanup",
    "overload": "strictly greater than cap; 2x newly excessive load; equality no damage",
    "timers": "after successful activation, decrement once at owner's End after phase triggers, not opponent End",
    "deck": "draw/inspect no shuffle; search/shuffle-in per rules; explicit top/bottom overrides shuffle; keep original relative order for remaining Otherworldly Perception/Augury cards",
    "mainCastDocs": "complete legal action and speed/response restrictions from kernel and confirmed decisions",
    "baseLoadCapacity": 2,
    "opening": "RPS winner goes first; ties repeat; simultaneous hidden submission until both confirmed; then shuffle and draw5 each; then private initial formation selections revealed together; all normal AI/hotseat/central modes; remove normal-room fixed-first override; tutorial chapter2 includes true opening",
    "recipeReady": "retain current Ready or Active eligibility for completed-analysis recipe material; do not conflate with analysis-time modification exclusions",
    "rpsClock": "No match clock deduction during any RPS round; resume existing clock behavior at initial formation selection; network freeze/reconnect/abort unchanged"
  },
  "executionDefaults": {
    "branch": "codex/alpha-v263 in isolated workspace based on latest origin/dev; integrate and push origin/dev without force",
    "implementation": "native execution in current chat with independent review",
    "mechanicsRevision": 12,
    "protocol": 5,
    "stateFormat": "wizardcard-state-v8",
    "replayFormat": 6,
    "digestSchema": 4,
    "overflowOrder": "finish chain and derived effects, perform existing persistent-spell returns, then stabilize mandatory capacity cleanup; recheck after resulting effects",
    "sourceDocuments": "rename requirements to Alpha v2.6.3 更新需求.md and backfill decisions; store versioned offline rulebook in repo",
    "openingInterface": "add authoritative opening stage/round/private gesture state and SubmitOpeningGesture command; update legal query, viewer filtering, AI/hotseat privacy, codec/network/replay; normal clients may not override RPS winner",
    "acceptance": "A01-A12 plus A13 RPS opening, privacy, ties, reconnection, no clock charge, draw-before-formation order"
  }
}
```

## 交付范围
全部A01–A12及A13猜拳；源码推送origin/dev，便携包交付到主仓库dist，同版正式服部署、公网双端及确定性复盘验收。

