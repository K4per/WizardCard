class_name KernelContract
extends RefCounted

# Generated from core contracts. Run tools/generate_godot_contract.py after changes.
const SCHEMA: int = 2
enum CommandKind { SetFormation=0, RemoveFormation=1, StartAnalysis=2, PrepareCast=3, PlayAction=4, PreloadWord=5, AttachSeal=6, RemoveSeal=7, Abandon=8, Advance=9, Choose=10, Surrender=11, Respond=12, PassResponse=13, AdvancePhase=14, SetAmbush=15, FlipAmbush=16, ActivateSpell=17, SelectInitialFormation=18, Timeout=19, ActivateAbility=20 }
enum ChoiceKind { Pass, Card, Player, Load, Mode, Payment, Link, Trigger, Legacy }
enum ChoiceMode { Confirm=1, SevenDamage=2, SevenLoad=3, DelayAnalysis=4, CycloneDamage=5, Draw=6, StormSearch=7, StormPoints=8, SupportingSearch=9, DamageTypeBase=100, HeartDraw=200, HeartRecover=201 }
enum DecisionKind { Response, CastOrder, TriggerOrder, Discard, ClearLoad, Overflow, DestroyOwnFormation, EffectDiscard, EffectPrepare, Concentration, DestroyAmbush, SearchDeck, ReturnSpell, EffectChoice, DamageChoice, ClearAnyLoad, StormSearch, StormHeartChoice, StormHeartRecover, Payment }
enum ActionField { Operation, Source, Host, Replacement, Material, Target, Discard, Link, Ability, Ring, Choice, Confirm }
