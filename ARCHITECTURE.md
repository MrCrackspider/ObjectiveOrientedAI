# ObjectiveOrientedAI — architecture notes

Working notes for picking this plugin back up quickly. Every claim here was read out of the source;
line numbers drift, so the symbol names are the reliable part.

Build commands, engine path and code conventions are in the project `CLAUDE.md`, not here.

## Modules

| Module | Type | Deps |
|---|---|---|
| `ObjectiveOrientedAI` | Runtime | public: `GameplayTags GameplayDebugger AIModule GameplayTasks NavigationSystem DeveloperSettings` |
| `ObjectiveOrientedAIEditor` | UncookedOnly | private: `KismetCompiler UnrealEd GameplayTags PropertyEditor`, public: `BlueprintGraph` |

`AIModule` + `NavigationSystem` are already public deps, so path following / nav / BT / EQS need no
`.Build.cs` change. `UnrealEd` is already available in the editor module.

## The object graph

```
AController  (any kind — not necessarily AAIController)
└── UObjectiveComponent : UBrainComponent
    └── FObjectiveContainer ObjectiveContainer      <- nesting level 0
        ├── CurrentObjectives : TArray<UAIObjective*>   (running, or held while container is paused)
        └── QueuedObjectives  : TArray<UAIObjective*>   (waiting)
             └── each UAIObjective owns its own FObjectiveContainer  <- level 1, 2, ...
                 linked upward by FObjectiveContainer::OwningContainer (raw ptr, set in ActivateObjective)
```

- `UAIObjective::OwningComponent` points at the component, **not** at the parent objective. Every
  objective at every depth is outered to the component (`Rename(nullptr, ObjectiveComponent)` in
  `Initialize`).
- Sub-objectives live in the parent's nested container. Depth is computed by walking `OwningContainer`
  (`GetNestingLevel`).
- `FObjectiveContainer` is a `USTRUCT` with `UPROPERTY` arrays — GC reaches objectives through it.
  `OwningContainer` is a **raw pointer to a struct**, so it is only valid while the owner lives.

### Key files

| Thing | File |
|---|---|
| Objective base, lifecycle | `Public|Private/AIObjective/AIObjective.{h,cpp}` |
| Container, queue, rules, stop reasons, debug snapshot | `Public/ObjectiveContainer.h`, `Private/ObjectiveContainer.cpp` |
| Component | `Public|Private/ObjectiveComponent.{h,cpp}` |
| Authored tag rules | `AIObjective/AIObjectiveDefinition.{h,cpp}` |
| Component lookup from any actor | `OOAIBlueprintHelperLibrary.{h,cpp}` |
| Opt-in interface for that lookup | `ObjectiveComponentOwnerInterface.h` |
| K2 nodes (4 classes, 1 file) | `ObjectiveOrientedAIEditor/.../K2Node_AddObjective.{h,cpp}` |
| Latent BP support | `Private/Async/AsyncAction_AddObjectiveAndWait{,ForResult}.cpp`, `ObjectiveAsync_WaitObjectiveInvalidated.cpp` |
| Settings | `ObjectiveOrientedAISettings.h` (project), `ObjectiveOrientedAIUserSettings.h` (per-user, header only) |
| Objectives on a non-controller actor | `Components/InstancedObjectiveComponent.{h,cpp}` |

Built-in objectives: `AIO_MoveTo`, `AIO_PlayMontage`, `AIO_PlayDynamicMontage`, `AIO_RunBehaviorTree`,
`AIO_RunEQSQuery`, `AIO_Wait`, `AIObjectiveComposite`, `AIObjectiveSequence`.

Ignore `UTEOAIObjective` and `UAIO_StimulusReaction` (project-side, out of scope by standing instruction).

## State and stop reasons

```cpp
enum class EAIObjectiveState : uint8 { None, Running, Queued, PendingInvalidation };
enum class EAIObjectiveStopReason : uint8 { Queued, Failed, Succeeded, Cancelled, Restarting };
```

`FStopReasonType` predicates (`ObjectiveContainer.cpp:21-46`):

| Predicate | True for |
|---|---|
| `WasQueued()` | Queued |
| `IsCompleted()` | Succeeded, Failed, **Cancelled** |
| `IsPendingInvalidation()` | same as `IsCompleted()` — marked `// TODO: Deprecate` |
| `IsSucceeded()` | Succeeded only |
| `IsRestarting()` | Restarting |

**Trap:** "did this run to completion rather than being cancelled" is `IsSucceeded() || Reason == Failed`.
`IsCompleted()` / `IsPendingInvalidation()` both include `Cancelled`.

## Lifecycle — exact order

`UAIObjective::Initialize` (`AIObjective.cpp:196`):
1. reject invalid component; reject re-init under a *different* component; early-return `true` if already initialized
2. `Rename(nullptr, ObjectiveComponent)`, set `OwningComponent`
3. run the caller's `Initializer(this)`
4. bail if the objective killed itself in step 3
5. `OnInitialized()`

So `OnInitialized` sees every exposed variable already assigned and can reach the pawn. Both step 3 and
step 5 are user code and may invalidate.

`Start()` (`AIObjective.cpp:234`): if `IsPendingInvalidation` → return. If already running: honour
`bAllowRestarting`, optionally `ObjectiveContainer.ClearObjectives()` per `bResetSubobjectivesOnRestart`,
`OnStopped(Restarting)`, recheck invalidation. Then `State = Running`, `bWasEverStarted = true`,
`ObjectiveContainer.SetQueued(false)` (resumes sub-objectives — **user code, may invalidate `this`**),
recheck, finally `OnStarted(bWasQueued)`.

`Enqueue()` (`AIObjective.cpp:298`): `State = Queued`, `SetQueued(true)` on own container, then — only if
`WasEverStarted()` and still valid — `OnStopped(Queued)` + broadcast.

`Destroy(StopReason)` (`AIObjective.cpp:328`): `State = PendingInvalidation`, then **only if
`WasEverStarted()`**: clear sub-objectives → `OnStopped(reason)` → `OnInvalidated(StopReason.IsSucceeded())`
→ broadcast. Always `MarkAsGarbage()`.

**Invariant worth remembering:** `OnStopped` / `OnInvalidated` never run for an objective that was
initialized but never started. See trap 2.

"Broadcast" above means `OnObjectiveStopped` (`FOnObjectiveStopped`), a native multicast delegate, of
which `FObjectiveStoppedDelegate` — the `OnStopped` argument of every add path — is the single-cast form.
There is no Blueprint assignable stopped event any more: the dynamic `OnObjectiveStoppedEvent` was
removed, so a Blueprint which needs to hear an objective stop goes through `OnStopped`/`OnInvalidated` or
one of the latent nodes. Nothing needs a `UFUNCTION` to listen.

## The four add paths

All go through `FObjectiveContainer`. `UObjectiveComponent::AddObjective` and
`UAIObjective::AddSubObjective` just forward to their own container.

1. **`AddObjective(Component, UAIObjective*, bMakeCopy)`** — `ObjectiveContainer.cpp:265`
   `CanAddObjective(component, instance)` → reject CDOs explicitly → if already initialized, go to
   `AddInitializedObjective` (and refuse `bMakeCopy`) → else `DuplicateObject` when copying →
   `AddNewObjective`.
2. **`AddObjective(Component, TSubclassOf, Initializer)`** — `:305`
   `CanAddObjective(component, Class.GetDefaultObject())` → `NewObject` → `AddNewObjective`.
   The initializer takes a bare `UAIObjective*`. Two header-only templates wrap it so it does not
   have to: `AddObjective<T>(Component, Initializer)` for a class known at compile time, and
   `AddObjective(Component, TSubclassOf<T>, Initializer)` which deduces `T` from the class handed in,
   so a class property gives a typed initializer with no cast at the call site. Both are constrained
   on the initializer being invocable with `T*`, which is what keeps the untyped overloads reachable
   when the third argument is a stopped delegate instead. They hand the class on as a plain `UClass*`
   so the typed overload is not a candidate for its own inner call.
3. **`AddNewObjective`** — `:320` — `Initialize(+Initializer)` → validity check → `BindStoppedCallback`
   → **`CanActivate`** → `ActivateObjective`.
4. **`AddInitializedObjective`** — `:355` — re-add. `CanActivate` first, then: queued → no-op;
   running → `Start()` (restart); stateless → `ActivateObjective`.

Every path returns a **`TObjectiveActivationResult<T>`**; `FObjectiveActivationResult` is the alias for
`T = UAIObjective`. It is a plain template, not a `USTRUCT` — nothing reflected uses it. It holds the
objective in a `TWeakObjectPtr`: GC cannot see an unreflected member, so a plain pointer would dangle in a
result kept past a purge, while the weak one answers null — and the weak one keeps nothing alive. Same
8 bytes. The untyped paths return the alias; the typed overloads (instance `T*`,
`TSubclassOf<T>`, explicit `T`, on the container, component and `AddSubObjective`) return it typed, so
`GetObjective()` needs no cast. Each typed overload calls the untyped one and narrows the result through
the private `CastFrom`, a `CastChecked` that cannot fail: the objective was handed in as a `T`, built from
a class deriving from `T`, or copied from one, and a copy keeps its class. A typed result converts
implicitly to any base-typed one, so existing code holding `FObjectiveActivationResult` is unaffected. The
typed instance overloads are constrained on `T*` converting to `UAIObjective*` — without it a `UClass*`
argument would deduce `T = UClass` and hijack the class path.

`CanAddObjective` (`:225`): valid component + objective, not `bCleaningDown`, component **has a pawn**,
**`IsCompatibleWith`**, and `FindBlockingObjective` says nothing refuses it. The objective it asks is the
CDO on path 2 and the caller's instance/template on path 1 — in both cases **before any copy and before
`Initialize`**. `TryCreateAIObjective` (`:212`) asks the CDO the same question.

`ActivateObjective` (`:809`): sets the nested container's `OwningContainer` → cancels everything the
newcomer's definition cancels → `CurrentObjectives.AddUnique` (**at the back**) → `ReevaluateQueue` →
if the objective still has `State == None`, remove it and `Destroy(Failed)`.

## Queue model

`ReevaluateQueue` (`:582`):
- returns immediately if `bCleaningDown`
- `ensureMsgf` + bail at `ObjectiveQueue::MaxReevaluationDepth`; depth held by a `TGuardValue`
- `ResolveEnqueueRules(ToRun, ToQueue)`
- a paused container (`bQueued`) sends everything to enqueue instead of start
- **writes `CurrentObjectives` / `QueuedObjectives` before applying anything**, so an objective that
  reads the container from `OnStarted` sees the settled split
- then applies via `StartObjectives` / `EnqueueObjectives`

`StartObjectives` (`:767`) re-tests `ShouldObjectiveRun(Objective) && !IsRunning()` per entry rather than
trusting the snapshot, because starting one objective may move or kill the next.

`StartObjective` (`:733`) calls **`CanActivate` again** and `Invalidate(false)` on failure.
`EnqueueObjective` (`:744`) invalidates when `!bCanBeQueued`.

`bClearSubObjectivesWhenQueued` on the **parent** is the other way out of pausing: `Enqueue()` calls
`ClearObjectives()` before `SetQueued(true)`, so the sub-objectives are destroyed (`Cancelled`) instead of
paused, and `CanAddSubObjective` refuses newcomers while the parent stays queued. It only applies to the
parent being queued — sub-objectives held back by a sibling's rules inside a running parent still wait as
usual. The parent resumes empty and has to rebuild from `OnStarted(bWasQueued = true)`.

Array **order is priority** — `FindHeldBackCandidate` walks from the back so the last entry yields first.

### Rules live on the definition, not the objective

`UAIObjectiveDefinition` (a `UDataAsset`) holds `DefinitionTags` plus
`Cancel`/`Block`/`EnqueueDefinitionsWithTags`. `UAIObjective::ShouldCancel/Block/QueueObjective` all
require `IsValid(Definition)` — **no definition means no rules at all**, in either direction.

Tags come from two places and must be answered together: `ObjectiveTags` on the instance and
`Definition->GetDefinitionTags()`. `HasAllMatchingGameplayTags` merges (`GetAllObjectiveTags().HasAll`);
`HasAnyObjectiveTags` checks each set in turn to avoid building the merged container per pair.

## Component

`UObjectiveComponent : UBrainComponent`, but the brain base is mostly inert here — `AOOAIController`
never assigns `BrainComponent = ObjectiveComponent` (`OOAIController.cpp:8-12`), and `UBrainComponent`'s
constructor sets only `PrimaryComponentTick.bCanEverTick` and `bDoLogicRestartOnUnlock` — **not**
`bWantsInitializeComponent` (`AIModule/Private/BrainComponent.cpp:191-195`) — so
`UBrainComponent::InitializeComponent` never runs for this class. Hence `OnRegister`, not
`InitializeComponent`. Overriding `InitializeComponent` would mean setting that flag, which would newly
enable the base's blackboard caching (`BlackboardComp->CacheBrainComponent(*this)`) and could steal the
blackboard's brain pointer from the BT component.

`UBrainComponent` overrides map onto the container: `PauseLogic`→`SetQueued(true)`,
`ResumeLogic`→`SetQueued(false)`, `StopLogic`→`ClearObjectives`, `IsRunning`→`CurrentObjectives.Num()>0`,
`IsPaused`→`bQueued`.

**Controller-agnostic** as of the Oct 2026 refactor. `OnRegister` does `Cast<AController>(GetOwner())`
into `OwningController` and subscribes `OnPossessedPawnChanged`. `GetAIOwner()` (from `UBrainComponent`)
still works and is **null on every non-AI controller** — that is what AI-only objectives gate on in
`IsCompatibleWith`.

Two opt-in `EditDefaultsOnly` flags, applied in `OnRegister` → `AddRequestedControllerComponents`, and
only in a game world: `bCreatePathFollowingComponent`, `bCreateGameplayTasksComponent`. Both add to the
**controller** (the engine puts the AI one on the pawn). Added components are left unnamed to avoid
colliding with declared subobjects; the helper returns null when one already existed, so `Initialize()`
is only called on components this code created.

### Pawn access during teardown

`AController::UnPossess` calls `SetPawn(NULL)` **before** broadcasting `OnPossessedPawnChanged`, and
`AController::Possess` installs the new pawn before broadcasting. So the handler holds the outgoing pawn
in `OutgoingPawn` under a `TGuardValue` for the duration of `ClearObjectives()`, and `GetPawn()` prefers
it. `UAIObjective::GetControlledPawn()` therefore asks **the component**, never the controller — that is
load-bearing, do not "simplify" it back.

`EndPlay` also calls `ClearObjectives()`, because losing the pawn is otherwise the only thing that stops
objectives: a controller that never possessed, or that outlives its pawn, would have its objectives
collected without `OnStopped`/`OnInvalidated` ever running.

## Engine facts worth not re-deriving

Paths under `F:\UE\UE_5.7\Engine\Source`.

- **`AController` implements `INavAgentInterface`** — `Runtime/Engine/Classes/GameFramework/Controller.h:40`,
  members at `:339-346` (`GetNavAgentPropertiesRef`, `GetNavAgentLocation`, `GetMoveGoalReachTest`,
  `ShouldPostponePathUpdates`, `IsFollowingAPath`, `GetPathFollowingAgent`). Forwards to the pawn. This is
  why `AIO_MoveTo` needed no `AAIController`.
- `FPathFindingQuery` has an `INavAgentInterface&` ctor overload —
  `Runtime/NavigationSystem/Public/NavigationSystemTypes.h:69`.
- `UNavigationSystemV1::ProjectPointToNavigation` — the `FNavAgentProperties*` overload is **non-const**
  (`NavigationSystem.h:698`); the const one takes `ANavigationData*` (`:704`). Easy C2663.
- `UPathFollowingComponent::OnPathfindingQuery` is a public no-op virtual — `PathFollowingComponent.h:415`.
- `UPathFollowingComponent` is already controller-agnostic: `OnRegister` does `Cast<AController>(GetOwner())`
  then `GetOnNewPawnNotifier().AddUObject(this, &OnNewPawn)` — `PathFollowingComponent.cpp:664-673`.
  `Initialize()` is just `UpdateCachedComponents(); LocationSamples.Reserve(...)` (`:642-646`), so it is
  cheap but **must not** be called on a component that already existed. `AAIController` creates its own
  with `CreateOptionalDefaultSubobject` in the ctor (`AIController.cpp:44`) and calls `Initialize()` from
  `OnPossess` (`:485`), with `Cleanup()` on unpossess (`:530`).
- `UAIBlueprintHelperLibrary::SimpleMoveTo{Actor,Location}(AController*, ...)` is generic but drops
  acceptance radius / filters / partial paths / reach tests. Its `InitNavigationControl` helper is
  **file-private**, so it cannot be reused — `AIBlueprintHelperLibrary.cpp:427`.
- `AAIController::MoveTo` at `Runtime/AIModule/Private/AIController.cpp:645`;
  `BuildPathfindingQuery` `:822`; `FindPathForMoveRequest` `:880`; `RequestMove` `:767`.
  Only genuinely AI-bound pieces: `GetDefaultNavigationFilterClass()` and `bAllowStrafe` (both plain
  `AAIController` members).
  Engine oddity not reproduced: it writes `UpdateGoalLocation` outside the projection-success branch (`:700`).
- `OnPossessedPawnChanged` is declared on **`AController`**, not `AAIController` — `Controller.h:62`,
  signature `FOnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)` declared at `:20`. Its own comment:
  "Called on both authorities and clients when the possessed pawn changes (either OldPawn or NewPawn might
  be nullptr)". `BlueprintAssignable`, so `AddDynamic`/`RemoveDynamic`.
- Unpossess ordering: `AController::UnPossess` `Controller.cpp:382` → `OnUnPossess` `:406` →
  `Pawn->UnPossessed()` **then** `SetPawn(NULL)` `:412-413`; broadcast at `:400`.
  `AController::Destroyed` `:591` calls `UnPossess()`. `PawnPendingDestroy` `:417` enters with the pawn
  still set, then unpossesses.
- `APawn::EndPlay` calls `DetachFromControllerPendingDestroy()` **before** `Super::EndPlay`
  (`Pawn.cpp:535-544`), so during PIE-stop teardown the pawn is still a live, fully-registered actor when
  objectives invalidate. Montage stops and loose-tag removal from `OnInvalidated` do take effect.
- The only *delegate* that fires while the controller still holds the pawn:
  `APawn::ReceiveControllerChangedDelegate`, broadcast from `APawn::NotifyControllerChanged()`
  (`Pawn.cpp:719-730`) which `UnPossessed()` calls at `:714`. But by then `Pawn->GetController()`,
  `GetOwner()` and the input component are already cleared (`:698-706`) — the other direction is broken.
  `UGameInstance::GetOnPawnControllerChanged()` fires at the same instant.
- `AActor::EndPlay` routes to components after its own `OnEndPlay` broadcast and `ensureMsgf`s on a
  missing `Super::EndPlay` — `Actor.cpp:3250-3257`.
- `FBlueprintEditorUtils::GetClassNameWithoutSuffix` strips `_C` only when `ClassGeneratedBy` is set —
  `Editor/UnrealEd/Private/Kismet2/BlueprintEditorUtils.cpp:10022`.
- `FInstancedPropertyBag` is at `CoreUObject/Public/StructUtils/PropertyBag.h:417` — no extra module dep.
  Layout changes are documented expensive at `:380`: *"Adding or removing properties to the instance is
  quite expensive as it will create new UPropertyBag, reallocate memory, and copy all values over."*
  Accessors return `TValueOrError<T, EPropertyBagResult>` (`:622` onward). The header declares **no**
  `BlueprintCallable`/`BlueprintPure` at all, and no Kismet library wraps it, so there are **no generic
  Blueprint accessors in 5.7**.
- `UBlackboardKeyType_Struct` does exist in 5.7
  (`AIModule/Classes/BehaviorTree/Blackboard/BlackboardKeyType_Struct.h`), but `UBlackboardComponent`
  declares no `GetValueAsStruct` — C++ template access only.
  `UBlackboardComponent::RegisterObserver(FBlackboard::FKey, const UObject* NotifyOwner, FOnBlackboardChangeNotification)`
  → `FDelegateHandle`, at `BlackboardComponent.h:73`.

## Editor module

`K2Node_AddObjective.{h,cpp}` holds six classes: `UK2Node_AddObjectiveBase` (abstract) →
`UK2Node_AddSubObjective`, `UK2Node_AddObjective`, and `UK2Node_AddObjectiveAndWaitBase` (abstract) →
`UK2Node_AddObjectiveAndWait`, `UK2Node_AddSubObjectiveAndWait`.

Subclasses customise via `GetObjectiveInstanceGetterFunctionName`, `GetAddObjectiveInstanceFunctionName`,
`GetTargetPinFriendlyName`, `GetTargetClass`, `GetObjectiveTitleName`, and for latent variants
`GetAsyncFactoryFunction` / `GetAsyncTargetParamName`.

`ExpandObjectiveInstanceCreation` is the shared half: spawns the helper-library creation call, moves exec
/ target / class / result links, then `GenerateAssignmentNodes` for expose-on-spawn. Two real traps,
both already handled and commented:
- `GetClassToSpawn()` must be read **before** the class pin's links are moved, or it resolves to null and
  expose-on-spawn assignments silently vanish.
- `GenerateAssignmentNodes` skips any input pin whose name collides with a pin on the creation call, so a
  colliding expose-on-spawn property is reported as an error rather than silently left at its default.

Node expansion is baked at **BP compile time** — changing a K2 node requires recompiling affected
Blueprints, not just restarting.

Titles: `GetNodeTitle` returns the bare `GetObjectiveTitleName()` when the class pin is missing, linked,
or empty, and `"<title> <ClassName>"` otherwise, cached in `CachedNodeTitle`. Deliberately does **not**
name the class for a linked pin: `GetClassToSpawn` returns the source pin's *declared* type
(`K2Node_ConstructObjectFromClass.cpp:197-201`), not what it will hold.

## Debugging

- Gameplay debugger category `"OOAI"`, registered `EnabledInGameAndSimulate` with **no AI filter**
  (`ObjectiveOrientedAIModule.cpp`). `CollectData` forwards `DebugActor` to
  `UOOAIBlueprintHelperLibrary::GetObjectiveComponent`, which is fully controller-agnostic:
  `Cast<APawn>` → `Pawn->GetController()` as plain `AController` → `IObjectiveComponentOwnerInterface` →
  `FindComponentByClass<UObjectiveComponent>()`. **Works on a player-controlled pawn with no changes.**
- Guarded by `WITH_GAMEPLAY_DEBUGGER_MENU` (`FDebugInfo`, `GetDebugInfo`, `GetDebugName`).
- Visual log: `UE_VLOG` on the **pawn**, gated by `FObjectiveContainer::ShouldVisualLog()` which honours a
  max nesting depth from project settings. `ENABLE_VISUAL_LOG` is defined in `EngineDefines.h`; the project
  compiles with C4668-as-error, so guard with `#if ENABLE_VISUAL_LOG`.
- `GetDebugDescription_Implementation` is `WITH_EDITOR`-only, set as `FDebugInfo::Description`.

### Canvas colour markup

`FGameplayDebuggerCanvasContext::Print` reads `{colorname}` inline — an `FColorList` name, lowercased
before lookup, else `FColor::InitFromString` (`GameplayDebuggerTypes.cpp:534-567`). Three traps, all
handled in `FDebugInfo`:

1. **A colour holds until the next tag, newlines included** (`:476`), so every line must open with its own
   colour tag.
2. **A brace the parser cannot read as a colour swallows everything up to the closing one** (`:484-490`
   drops the node when `ParseTag` fails) — so collected names go through `FDebugInfo::Sanitize`, which
   turns braces into parens and newlines into spaces.
3. `\t` **is** supported and expands to five spaces (`:462`, `:480`) — the older "tabs do not render"
   comment was wrong. Indentation is still spaces, two per level, because five is too coarse for a tree.

Palette follows the engine's own categories: **wording white, per-actor values yellow** (names,
`GetDebugDescription` output). Objective state is the line colour — running **white**, paused **orange**,
queued **red** — with `Running` headers lightblue, definition names skyblue, instance tags silver, the
`<< next` marker green.

`FDebugInfo` is the whole renderer: owner headline + labelled detail lines + a `Running` / `Queued` group
per container, recursing per objective. Queued entries carry `HeldBackBy`, resolved at collect time by
asking each running objective's `ShouldQueueObjective`. Group order is priority order, so entries are
numbered. `UAIObjective::GetDebugName` gives the class display name for native objectives and the asset
name minus `_C` for Blueprint ones (`UClass::IsNative()` is the discriminator — `ClassGeneratedBy` is
editor-only data and gone in a cooked build).

**Nesting depth** mirrors the visual log's, as a second setting:
`UObjectiveOrientedAIUserSettings::GameplayDebuggerNestingDepth` (default 3, `ClampMin = 1`) against
`FObjectiveContainer::ShouldGameplayDebug()` → `GetNestingLevel() < Depth`, the same shape as
`ShouldVisualLog()`. The gate is in the **`FDebugInfo` container constructor**, so it costs nothing at
collect time past the limit. The objective owning a cut-off container is still drawn; it reports
`TruncatedCount` as `(+N below depth)`, which is also what stops the root printing *"no objectives"* when
a hand-edited ini sets depth to 0.

## Open threads

**`CanActivate` split — done Oct 2026.** `IsCompatibleWith(const UObjectiveComponent*)` is a
`BlueprintNativeEvent` defaulting to `true`, declared beside `CanActivate` in `AIObjective.h` and called
from `CanAddObjective`. Contract: *return false only when this class can never run on this component.*
Data that may still arrive via subclass defaults, ExposeOnSpawn, the initializer or `OnInitialized` is off
limits — an author who wants to read instance data guards on `!HasAnyFlags(RF_ClassDefaultObject)`, which
is the CDO-vs-template distinction, so no second hook exists. `CanActivate` keeps its per-start contract
and holds only answers that change over the objective's life.

Moved: `AIO_MoveTo` wholesale (its whole check was path-following capability). `AIO_RunBehaviorTree`
**split** rather than moved wholesale as first sketched — the `GetAIOwner()` half went up,
`IsValid(BehaviorTree)` stayed in `CanActivate`, because `MakeInstance` and ExposeOnSpawn can both supply
the tree after the template has been asked. Untouched: `AIO_PlayMontage`, `AIObjectiveComposite`,
`AIObjectiveSequence`.

Still to move, project-side (`Source/TheEmperorsOwnRuntime`) — each needs the same split read:
`AIO_StartStateTree`, `AIO_StartScriptDialog`, `AIO_SplineMovementBase`, `AIO_PlayWarpedMontage`,
`AIO_LookAt`, and plugin-side `AIO_RunEQSQuery`, `AIO_PlayDynamicMontage`.

**Known defects / smells**

1. `CanActivate` is called **twice** on a fresh add that runs immediately — `AddNewObjective:347`, then
   again via `ActivateObjective` → `ReevaluateQueue` → `StartObjectives` → `StartObjective:735`, with
   nothing changing in between.
2. **Side-effect leak.** `AddNewObjective` runs `OnInitialized` before `CanActivate`; on failure it
   `return {}`s without `Destroy()`, and `Destroy` gates `OnStopped`/`OnInvalidated` on `WasEverStarted()`.
   So `OnInitialized`'s side effects are never undone and the object lingers to GC in the state the code
   itself calls *"initialized but in no state at all"* (`:390`).
3. `StartObjective` invalidates on any `CanActivate` failure, so a transient miss kills an objective that
   was only waiting. Still open — the split moved the *inadmissible* cases out of `CanActivate`, which
   makes this far less likely to fire, but (1) and (2) are untouched.
4. Enqueue/block/cancel rules are evaluated **per container only** — objectives in different containers
   never see each other's rules.
5. `AIO_StartStateTree.cpp` (project-side, `Source/TheEmperorsOwnRuntime`) is missing
   `#include "Engine/World.h"`; shows up as a false positive under `-SingleFile`.
6. `FOnObjectiveInvalidated` is declared in `AsyncAction_AddObjectiveAndWait.h` but used by
   `ObjectiveAsync_WaitObjectiveInvalidated.h` too, which now includes that header for the one type.
   It belongs in a header of its own.

**Content / Blueprint debt (needs an editor pass, never done)**

- `CancelObjectivesWithTags` / `BlockObjectivesWithTags` / `EnqueueObjectivesWithTags` were removed from
  `UAIObjective` and **cannot be redirected** — any BP objective that authored them lost those values
  silently and needs a `UAIObjectiveDefinition` asset created and assigned.
- `GetOwningController()` now returns `AController*`; BPs that dragged that pin into an AI-controller
  function need `GetOwningAIController()`.
- The `OnObjectiveStoppedEvent` dynamic delegate was removed outright. Any Blueprint which bound that
  event lost the binding silently — it cannot be redirected — and needs reworking onto `OnStopped` or
  `OnInvalidated`. `BP_StartScriptDialog` is the one known to look at.
- Five BPs reference a removed `OptionalAnimInstance`: `BP_AIO_AR_Investigate`, `BP_AIO_HitReaction`,
  `BP_AIO_PreBattleAlert`, `BP_AlertReactionMontage`, `BP_AIO_PlayAnimationWithBlend`.
- Verify the `bInverse` `PropertyRedirects` entry in `Config/DefaultObjectiveOrientedAI.ini` against an
  existing sequence asset.
- Recompile Blueprints affected by the earlier `K2Node_AddObjective` expansion changes.

**Unresolved questions**

- Should negative `PlayRate` (reverse playback) be supported in `AIO_PlayMontage`?
- Offered, not taken: forward `bResumeFromStopPosition` and surface `bInvalidateOnBlendOut` on
  `AIO_PlayDynamicMontage`; gate the plain `UE_LOG(ObjectiveOrientedAI, Display, ...)` lines in
  `AIObjective.cpp` by nesting depth.

## Shared state between objectives

Decided in an earlier session: **use the blackboard, not an `FInstancedPropertyBag` on the component.**
Decisive reason is change notification — `UBlackboardComponent::RegisterObserver` exists and
`UAIObjective` has no tick. The bag's struct-type advantage is gone in 5.7 now that
`UBlackboardKeyType_Struct` exists.

For the narrower "this montage already played, don't replay it" case, neither is right: write a gameplay
tag on the pawn's ASC from `OnInvalidated(bSuccess)` — verified false on cancel — and read it in
`OnStarted`, not `CanActivate`. A never-reset flag is a latent bug; reset it from `UAlertStateComponent`
(`ResetAlertState()` / `OnAlertStateChangedEvent`) or give it a duration via a `UGameplayEffect`.
`AddLooseGameplayTag` / `RemoveLooseGameplayTag` is the established project idiom.
