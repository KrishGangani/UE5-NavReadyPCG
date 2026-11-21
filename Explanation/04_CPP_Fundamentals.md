# 04 · C++ Fundamentals: NavReadyPCG

**Rule:** only the language ideas this project's files actually rely on, each with a tiny example first and then where it appears in our code. This file grows with every implementation step.

**State (S3):** sections 1–6 cover the Phase 4 files (module entry points, C# build rules, descriptors, INI). Sections 7–15 were written for the S1 world spike; their examples now point at the S2 code, with the S1-only code noted. Sections 16–22 cover what S2 adds, 23–27 what S3 adds.

---

## 1. Translation units and `#include`

```cpp
// Greeting.h
void SayHello();

// Main.cpp
#include "Greeting.h"   // the preprocessor pastes the whole text of Greeting.h here
int main() { SayHello(); }
```
- The compiler compiles **one `.cpp` file at a time**, together with everything it `#include`s. That bundle is a *translation unit*.
- `#include "X.h"` is plain text pasting, done by the **preprocessor** before compilation. Nothing is "imported" the way C# `using` or Python `import` works.
- Where the compiler looks for `"Modules/ModuleManager.h"` is decided by the **include paths**. In Unreal, UnrealBuildTool builds those paths from each module's dependency list in `Build.cs` (`06` §2). That is why a missing dependency shows up as "cannot open include file".
- **In our code:** both module `.cpp` files start with `#include "Modules/ModuleManager.h"`. That header lives in the `Core` module, which both `Build.cs` files list as a public dependency.

## 2. Macros

```cpp
#define SQUARE(x) ((x) * (x))
int Area = SQUARE(3 + 1);   // the preprocessor rewrites this to ((3 + 1) * (3 + 1)) = 16
```
- A macro is a **text substitution** done by the preprocessor. It has no type checking and no scope; it can generate whole functions, variables and classes.
- That power is why Unreal uses macros for boilerplate that must look different in different build types. `IMPLEMENT_MODULE` expands into an exported entry-point function in a DLL (editor) build, and into a static registration object in a single-executable (packaged) build. Plain C++ functions can't change shape like that.
- The parentheses in `SQUARE` matter: without them, `SQUARE(3 + 1)` would become `3 + 1 * 3 + 1` = 7. Unreal's macros are written defensively for the same reason.
- **In our code:** `IMPLEMENT_PRIMARY_GAME_MODULE(...)` and `IMPLEMENT_MODULE(...)` (`05` A4, A6; `06` §1).

## 3. Comments

- `// text` runs to the end of the line. Every file in this project starts with a one-line comment saying what the file is for, so a reader opening it cold knows where they are.

## 4. Why there is no header for the module classes

- A `.h` file exists so that **other** translation units can see declarations. Our modules use the engine's ready-made module classes (`FDefaultGameModuleImpl`, `FDefaultModuleImpl`) and declare nothing of their own, so no other file needs to include anything from them.
- Epic's template ships an empty `<Module>.h` that nothing includes. An empty header adds a file to read and nothing else, so it is left out. The empty `Public\` folders keep a `.gitkeep` placeholder (git doesn't track empty folders); they fill up in S1–S5.

## 5. Reading the C# build files (`*.Target.cs`, `*.Build.cs`)

These files are **C#**, compiled by UnrealBuildTool itself, not by the C++ compiler. Four pieces of C# are enough to read them:

```csharp
using UnrealBuildTool;                                  // makes the names in that namespace usable without a prefix

public class NavReadyPCG : ModuleRules                   // a class deriving from (inheriting) ModuleRules
{
	public NavReadyPCG(ReadOnlyTargetRules Target)      // the constructor: runs when UBT creates the object
		: base(Target)                                   // first run the base class's constructor with the same argument
	{
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "Engine" });   // append an array of strings to a list
	}
}
```
- `: base(Target)` is C#'s version of a C++ member-initialiser call to the parent constructor.
- `new string[] { … }` creates an array in place; `AddRange` appends all of it to an existing list.
- UBT finds these classes **by name**: `<ModuleName>` for modules, `<TargetName>Target` for targets (`06` §2–3).

## 6. Reading the descriptors (`.uproject`, `.uplugin`) and the INI files

- **JSON** (`.uproject`, `.uplugin`): `{ }` is an object of `"key": value` pairs, `[ ]` is a list. Strings are in double quotes; `true`/`false` are booleans. A trailing comma after the last item is an error in strict JSON, so the files avoid it.
- **INI** (`Config\Default*.ini`): `[Section]` names a class (for example `[/Script/Engine.InputSettings]`) or a settings group (`[SystemSettings]`); `Key=Value` sets a property; `;` starts a comment. For **array** properties, `+Key=Value` adds an element and `-Key=Value` removes one. That is why `DefaultEngine.ini` has `-D3D12TargetedShaderFormats=PCD3D_SM5` followed by `+D3D12TargetedShaderFormats=PCD3D_SM6`: remove SM5 from the list the engine's base config provides, then add SM6.
- **Layering:** the engine reads `Engine\Config\Base*.ini` first, then the project's `Default*.ini` on top. A project file only needs the keys it changes. Our files also restate a few defaults on purpose (`05` A7).

---

## Added in step S1

## 7. Namespaces and named constants

```cpp
namespace NRBench
{
	constexpr double ArenaHalfSize = 10000.0;        // known at compile time
	const FVector ArenaCenter(100000.0, 0.0, 0.0);  // FVector isn't constexpr-constructible here, so plain const
}
double Half = NRBench::ArenaHalfSize;
```
- A **namespace** groups names so they can't clash with others (`NRBench::ArenaHalfSize` vs any other `ArenaHalfSize`).
- **`constexpr`** means "computed at compile time"; **`const`** means "never changes after initialisation". Both replace magic numbers with a name and a comment saying why the value is what it is.
- **In our code:** every tunable of the director lives in `namespace NRBench` at the top of `NRBenchmarkSubsystem.cpp` (the arena centre above is the S1 value; `04` §20 shows S2's); the graph builder is `namespace NRGraphFactory`, the hash `namespace NavReadyContentHash`.

## 8. References as output parameters

```cpp
bool TryDivide(int A, int B, int& OutResult) { if (B == 0) return false; OutResult = A / B; return true; }
int R; if (TryDivide(7, 2, R)) { /* R == 3 */ }
```
- A **reference** (`int&`) is another name for an existing variable: writing to it writes the caller's variable. It can't be null, unlike a pointer.
- Unreal names such parameters `Out…`. A reference **to a pointer** (`UPCGVolumeSamplerSettings*&`) lets a function hand back a pointer.
- **In our code:** `AddNodeOfType<T>(T*& OutSettings)`, `GetNavigation(World, UNavigationSystemV1*& OutNavSys)`, `GetGeneratedComponents(TArray<…>& OutComponents)`, and the engine's `GetInstanceTransform(Index, FTransform& Out, bWorldSpace)` (S1 also had `CheckArenaPath(FString& OutDetail)`).

## 9. `const` member functions and `const` locals

```cpp
struct FCell { FBox Box; FBox GetBounds() const { return Box; } };  // promises not to modify the object
const FBox Bounds = Cell.GetBounds();                               // this local never changes
```
- A `const` member function can be called on a `const` object and can't change its members. The compiler enforces it.
- **In our code:** `GetCellBounds() const`, `GetGeneratedComponents(...) const`, `IsArenaNavigable() const`, `AreTilesAlignedWithCells() const`, `CellsGenerated() const`.

## 10. `enum class`

```cpp
enum class EPhase : uint8 { Idle, WaitingForNavmesh, Flying, Settling, Done };
EPhase Phase = EPhase::Idle;        // must be qualified; doesn't convert to int silently
```
- Named states instead of magic integers (the first S1 draft used `0 / 1 / -1` for its check verdicts). `: uint8` fixes the storage size at one byte.
- **In our code:** the director's run phases (S2); in S1, the spike checks' verdicts.

## 11. Lambdas

```cpp
TArray<int32> Numbers = {1, 2, 3, 4};
Numbers.RemoveAll([](int32 N) { return N % 2 == 0; });   // keeps 1 and 3
```
- A **lambda** is an unnamed function written in place. `[]` lists what it captures from the surrounding scope (nothing here), `(int32 N)` its parameters.
- **In our code:** `ANRCellActor::GetGeneratedComponents` removes components without PCG's tag with `RemoveAll` and a lambda.

## 12. Range-based `for`, including over a braced list

```cpp
for (int32* Value : { &A, &B, &C }) { *Value = 0; }
```
- `for (Element : Container)` visits every element. A braced list `{ … }` becomes a temporary `std::initializer_list`, handy for "do the same to these three variables".
- **In our code:** `for (const TCHAR* Path : NRBench::PropMeshPaths)` (a C array) loads the meshes; `HashComponents` loops over its sorted components. (S1's `FinishChecks` looped over the braced list `{ &NavVerdict, &GenerationVerdict, &CarveVerdict }`.)

## 13. `static` local variables

```cpp
const FName& LabelName() { static const FName Name(TEXT("PerQualityLevelFloat")); return Name; }
```
- A `static` local is created the first time the line runs and kept for the program's lifetime. Creating an `FName` from text looks it up in a global table, so doing it once is cheaper than every call.
- **In our code:** `SetRadiiMember`'s `PerQualityLevelFloatName`.

## 14. Short-circuit `&&` and `||`

```cpp
if (Ptr && Ptr->IsValid()) { … }            // the right side runs only if Ptr isn't null
if (!BuildArena(W) || !SpawnCells(W)) {…}    // SpawnCells runs only if BuildArena succeeded
```
- `&&` stops at the first `false`, `||` at the first `true`. That makes "check, then use" safe in one expression, and orders steps that depend on each other.
- **In our code:** the set-up in `OnWorldBeginPlay`, `bRadiiSet` in `Configure`, the null checks before every navigation query.

## 15. Forward declarations vs includes

```cpp
class UPCGGraph;                       // "a class with this name exists": enough for pointers and references
void Use(UPCGGraphInterface* Graph);
Use(MyGraph);                          // error unless the compiler has seen that UPCGGraph derives from UPCGGraphInterface
```
- A **forward declaration** keeps headers light: anything that only stores or passes a pointer doesn't need the full class.
- Anything that needs the class's **layout or inheritance** (calling a member, converting to a base class) needs its header.
- **In our code:** headers forward-declare `UPCGComponent`, `UPCGGraph`, `ACameraActor`; `NRCellActor.cpp` includes `PCGGraph.h` because `SetGraphLocal(UPCGGraphInterface*)` needs the `UPCGGraph` → `UPCGGraphInterface` conversion (S1's first build failed with `error C2664` without it).

## Added in step S2

## 16. Fixed-size arrays, `sizeof`, and padding

```cpp
int64 Values[10] = { /* ten quantized numbers */ };   // 10 x 8 bytes = 80 bytes, no gaps
FCrc::MemCrc32(Values, sizeof(Values));                 // sizeof gives the array's size in bytes: 80

struct FMixed { uint32 A; int64 B; };                   // 4 bytes, then 4 bytes of PADDING, then 8 bytes
```
- A **C array** (`int64 Values[10]`) is a fixed number of elements stored back to back. Its name converts to a pointer to the first element; `sizeof` on the array (not on a pointer) gives the total bytes.
- **Padding:** the compiler aligns each member to its natural boundary, so `FMixed` has 4 unused bytes between `A` and `B`. Their content is undefined, so hashing a struct's raw memory can give a different hash for equal values. An array of one type has no padding.
- **In our code:** `HashInstance` puts all ten quantized values into one `int64` array and hashes its memory.

## 17. Pointers into an array, and the conditional operator

```cpp
int64* Quat = &Values[3];          // points at element 3; Quat[0] is Values[3], Quat[3] is Values[6]
const int64 Deciding = Quat[3] != 0 ? Quat[3] : (Quat[0] != 0 ? Quat[0] : Quat[1]);
```
- `&Values[3]` is the address of one element; indexing from it walks forward, so `Quat[i]` is `Values[3 + i]`. It must stay inside the array.
- `Condition ? A : B` is an **expression** that yields `A` or `B`. Nested, it reads as "the first non-zero of …". Brackets make the nesting visible.
- **In our code:** choosing the quaternion's sign (W, else X, Y, Z) in `HashInstance`.

## 18. `static_cast` and rounding to an integer

```cpp
const int64 Steps = static_cast<int64>(FMath::RoundToDouble(Value / Step));   // 12.34 cm / 0.1 -> 123
```
- `static_cast<T>(x)` is the checked, explicit conversion between related types (here `double` to `int64`). It truncates, so the value is rounded first.
- `int64` holds about ±9.2 × 10^18, far more than any world position in 0.1 cm steps (a 1,000 km world is 10^9 steps).
- **In our code:** `NavReadyContentHash::Quantize`.

## 19. Defaulted comparison (C++20)

```cpp
struct FNavReadyCellHash
{
	uint32 ContentHash = 0; uint32 OrderedHash = 0; int32 NumInstances = 0;
	bool operator==(const FNavReadyCellHash& Other) const = default;   // compares every member, in order
};
```
- `= default` asks the compiler to write the operator: member-wise `==`. C++20 also derives `!=` from it. Unreal 5.8 compiles as C++20: it is both the default and the minimum (`Programs/UnrealBuildTool/System/CppCompileEnvironment.cs:61, 71`).
- **In our code:** `FNavReadyCellHash`; the S5 tests compare whole cell hashes with it.

## 20. Constants built from constants

```cpp
constexpr double NavTileSize = 988.0;
constexpr double CellSize = 3.0 * NavTileSize;        // still a compile-time constant
const FVector ArenaCenter(101.0 * NavTileSize, NavTileSize, 0.0);
```
- A `constexpr` value can be used in other `constexpr` expressions, so one named number can drive a layout. Changing it changes every dependent value consistently.
- **In our code:** the tile size fixes the cell size and the arena's position; two run-time checks (`AreTilesAlignedWithCells`, the lattice check in `SpawnCells`) catch a constant that no longer matches the engine or the graph.

## 21. Views: `TConstArrayView` and `FStringView`

```cpp
void Hash(TConstArrayView<uint32> Values);    // accepts a TArray<uint32>, a C array, or part of one, without copying
TArray<uint32> A = {1, 2, 3}; Hash(A);
```
- A **view** is a pointer plus a length: it refers to someone else's elements and owns nothing. It is cheap to pass by value. The caller must keep the data alive while the view is used.
- `TConstArrayView<const UInstancedStaticMeshComponent*>` accepts a `TArray<UInstancedStaticMeshComponent*>`: adding `const` is allowed, the reverse isn't (`Containers/ArrayView.h:28`).
- `FStringView` is the same for text. It is **not** null-terminated, so functions that read until a null character can't take its data.
- **In our code:** every `NavReadyContentHash` function takes views.

## 22. Default member initialisers and plain structs inside a class

```cpp
class UNRBenchmarkSubsystem
{
	struct FCellRecord { FIntPoint Grid = FIntPoint::ZeroValue; double GenerationDone = -1.0; };   // -1 = not yet
	TArray<FCellRecord> Records;
};
```
- `= value` on a member gives it that value in every new object, so no constructor is needed.
- A struct declared **inside** a class is a nested type: private to the class if declared in a `private:` section.
- **In our code:** `FCellRecord` (-1 marks "not happened yet") and `FRockFieldParams`.

## Added in step S3

## 23. Parallel arrays, and removing with a swap

```cpp
TArray<FNavReadyCell> Cells;                              // Cells[i] belongs to Components[i]
TArray<TObjectPtr<UNavReadyCellComponent>> Components;
Cells.RemoveAtSwap(Index); Components.RemoveAtSwap(Index); // the last element moves into the gap, in both
```
- Two arrays with the same index instead of one array of structs: the per-frame loop touches only the plain data in `Cells`; the UObject pointers sit in a `UPROPERTY` array so the garbage collector sees them.
- `RemoveAtSwap` is O(1) because it doesn't shift the rest, but it changes the order. Both arrays must always be changed together.
- **In our code:** `UNavReadySubsystem`'s registry.

## 24. Never change a container while something iterates it

```cpp
bBroadcasting = true;
for (int32 i = 0; i < Pending.Num(); ++i) { const auto Change = Pending[i]; Change.Key->Event.Broadcast(…); }
bBroadcasting = false;
```
- A listener can do anything: destroy a cell (which unregisters it) or cause a new state change (which appends to `Pending`). So the loop goes **by index** (it sees appended items), **copies** the element before calling out (the array may reallocate), and removals during the loop only **empty the slot**; `Compact()` removes empty slots afterwards.
- **In our code:** `FlushBroadcasts`, `UnregisterCell`, `Compact`.

## 25. Detecting a rising edge

```cpp
const bool bWasDirty = bDirty;
bDirty = /* this frame's value */;
if (bDirty && !bWasDirty) { ++Episodes; }   // counted once per episode, not once per frame
```
- **In our code:** `FNavReadyCell::ObserveWhileReady`.

## 26. `static_cast` down a class hierarchy, and `static_assert`

```cpp
const ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(NavData);                          // checked: nullptr if not
const auto* Generator = static_cast<const FRecastNavMeshGenerator*>(NavMesh->GetGenerator()); // unchecked
static_assert(WITH_RECAST, "NavReadyPCGCore needs Recast navigation.");                 // checked at compile time
```
- `Cast<T>` (Unreal) checks the UObject's class at run time. A generator isn't a UObject, so the downcast is a plain `static_cast`, which checks nothing: it is correct only because the navigation data was first checked to be a Recast navmesh, whose generator is always a Recast generator.
- `static_assert(condition, message)` stops the **build** if the condition is false.
- **In our code:** the probe in `UNavReadySubsystem::Tick`.

## 27. `TPair` and `TWeakObjectPtr`

```cpp
TArray<TPair<TWeakObjectPtr<UNavReadyCellComponent>, ENavReadyCellState>> Pending;
if (UNavReadyCellComponent* Cell = Pending[i].Key.Get()) { … }   // nullptr if the cell was destroyed meanwhile
```
- `TPair<A, B>` holds two values (`.Key`, `.Value`). `TWeakObjectPtr` refers to a UObject without keeping it alive; `Get()` returns `nullptr` once it is gone.
- **In our code:** the queued state changes.
