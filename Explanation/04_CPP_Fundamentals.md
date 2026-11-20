# 04 · C++ Fundamentals: NavReadyPCG

**Rule:** only the language ideas this project's files actually rely on, each with a tiny example first and then where it appears in our code. This file grows with every implementation step.

**State (Phase 4):** the project has two C++ files of one meaningful line each, plus C# build rules, JSON descriptors and INI configuration. So this first version covers how those pieces are read. C++ classes, pointers, templates and the rest arrive with S1–S5.

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
