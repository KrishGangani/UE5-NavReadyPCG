# 04 · C++ Fundamentals: NavReadyPCG

**Rule:** only the language ideas this project's files actually rely on, each with a tiny example first and then where it appears in our code. This file grows with every implementation step.

**State (S1):** sections 1–6 cover the Phase 4 files (module entry points, C# build rules, descriptors, INI). Sections 7–15 cover the C++ the S1 world spike uses.

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
- **In our code:** every tunable of the S1 spike lives in `namespace NRBench` at the top of `NRBenchmarkSubsystem.cpp`; the graph builder is `namespace NRGraphFactory`.

## 8. References as output parameters

```cpp
bool TryDivide(int A, int B, int& OutResult) { if (B == 0) return false; OutResult = A / B; return true; }
int R; if (TryDivide(7, 2, R)) { /* R == 3 */ }
```
- A **reference** (`int&`) is another name for an existing variable: writing to it writes the caller's variable. It can't be null, unlike a pointer.
- Unreal names such parameters `Out…`. A reference **to a pointer** (`UPCGVolumeSamplerSettings*&`) lets a function hand back a pointer.
- **In our code:** `AddNodeOfType<T>(T*& OutSettings)`, `CheckArenaPath(FString& OutDetail)`, `GetNavigation(World, UNavigationSystemV1*& OutNavSys)`.

## 9. `const` member functions and `const` locals

```cpp
struct FCell { FBox Box; FBox GetBounds() const { return Box; } };  // promises not to modify the object
const FBox Bounds = Cell.GetBounds();                               // this local never changes
```
- A `const` member function can be called on a `const` object and can't change its members. The compiler enforces it.
- **In our code:** `GetCellBounds() const`, `GetGeneratedComponents(...) const`, `CheckArenaPath(...) const`, `AllChecksPassed() const`.

## 10. `enum class`

```cpp
enum class EVerdict : uint8 { Pending, Pass, Fail };
EVerdict V = EVerdict::Pending;     // must be qualified; doesn't convert to int silently
```
- Named states instead of magic integers (the first S1 draft used `0 / 1 / -1`). `: uint8` fixes the storage size at one byte.
- **In our code:** the three spike checks' verdicts.

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
- **In our code:** `FinishChecks` turns every still-pending verdict into a failure with one loop over `{ &NavVerdict, &GenerationVerdict, &CarveVerdict }`.

## 13. `static` local variables

```cpp
const FName& LabelName() { static const FName Name(TEXT("PerQualityLevelFloat")); return Name; }
```
- A `static` local is created the first time the line runs and kept for the program's lifetime. Creating an `FName` from text looks it up in a global table, so doing it once is cheaper than every call.
- **In our code:** `SetRadiiMember`'s `PerQualityLevelFloatName`.

## 14. Short-circuit `&&` and `||`

```cpp
if (Ptr && Ptr->IsValid()) { … }            // the right side runs only if Ptr isn't null
if (!BuildArena(W) || !SpawnTestCell(W)) {…} // SpawnTestCell runs only if BuildArena succeeded
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
