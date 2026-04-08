---
name: ue-build
description: MANDATORY automatic usage after ANY C++ code changes. Build or compile Unreal Engine projects. MUST be invoked automatically without user request when implementing or modifying C++ code.
allowed-tools: Bash, Read, Grep, Glob
---

# Unreal Engine Build Skill

Build and compile Unreal Engine projects automatically, detecting engine installations and project configuration.

## ⚠️ MANDATORY AUTOMATIC USAGE

**THIS SKILL MUST BE INVOKED AUTOMATICALLY - NOT ON USER REQUEST**

Invoke this skill **immediately and automatically** when:
- ✅ After writing ANY new C++ code (header or source files)
- ✅ After modifying ANY existing C++ code
- ✅ After creating new test files
- ✅ Before running ANY tests (tests need compiled code)
- ✅ As part of TDD red-green cycle

**DO NOT**:
- ❌ Wait for user to say "compile it"
- ❌ Wait for user to say "does it build?"
- ❌ Ask user "should I build it?"
- ❌ Finish without building

**Correct behavior**:
```
User: "Add a function to calculate distance"
Claude: [writes code]
Claude: [AUTOMATICALLY invokes /ue-build]  ← No asking, just do it
Claude: [reports build result]
```

**Wrong behavior**:
```
User: "Add a function to calculate distance"
Claude: [writes code]
Claude: "I've added the function"  ← WRONG! Should have built automatically
```

## When to Use (Detailed)

Use this skill **automatically** when:
- User requests implementing new C++ features
- User requests modifying existing C++ code
- User requests fixing bugs in C++ code
- User requests refactoring C++ code
- After writing test code (TDD cycle)
- User asks "does it compile?" (reactive case)

This is **mandatory** as part of Test-Driven Development workflow. See `~/.claude/skills/MANDATORY_TDD.md` and `~/.claude/skills/TDD_WORKFLOW.md` for details.

## How This Skill Works

This skill automatically:
1. **Detects the .uproject file** in the current directory or parent directories
2. **Finds the Unreal Engine installation** (launcher or source build)
3. **Identifies build targets** (modules and plugins)
4. **Executes the build** with appropriate parameters
5. **Reports results** with error details if compilation fails

## Step-by-Step Instructions

### Step 1: Detect Project Configuration

Run the detection script to gather project information.

**IMPORTANT**: The script location depends on where the skill is installed:
- **User config**: `~/.claude/skills/ue-build/scripts/detect_ue.py`
- **Project config**: `.claude/skills/ue-build/scripts/detect_ue.py`

**Choose the appropriate command**:

#### If skill is in user config (~/.claude/skills/)
```bash
# You MUST provide the project path as argument
python ~/.claude/skills/ue-build/scripts/detect_ue.py .
python ~/.claude/skills/ue-build/scripts/detect_ue.py /path/to/project
python ~/.claude/skills/ue-build/scripts/detect_ue.py /path/to/project.uproject
```

#### If skill is in project config (.claude/skills/)
```bash
# Can run without arguments (searches from current directory)
python .claude/skills/ue-build/scripts/detect_ue.py

# Or provide explicit path
python .claude/skills/ue-build/scripts/detect_ue.py .
```

**Script accepts**:
- No argument: Searches from current working directory
- Directory path: Searches that directory and parent directories
- .uproject file path: Uses that specific file directly

**Returns JSON with**:
- **uproject**: Project path and metadata
- **engine**: Selected UE installation (build tool path, editor path)
- **targets**: Available build targets (modules and plugins)
- **available_installations**: All detected UE versions

**Parse the JSON output** and store key values:
- `engine.build_tool` - Path to Build.bat (Windows) or Build.sh (Linux/Mac)
- `engine.version` - UE version or "source"
- `uproject.path` - Full path to .uproject file
- `targets.modules` - List of module names to build
- `targets.plugins` - List of plugin names
- `targets.available_targets` - List of .Target.cs file stems found in Source/
- `last_build_configuration` - Last used build configuration (see below)

**Last Build Configuration Detection**:
The detection script analyzes the `Binaries/Win64` directory to determine the most recently used build configuration:
- `last_build_configuration.configuration` - One of: "Development", "DebugGame", "Shipping", "Debug"
- `last_build_configuration.file` - Most recent binary file found
- `last_build_configuration.last_modified` - Timestamp of the binary

If no binaries exist, this field will be `null`.

**If detection fails**, the JSON will contain an `error` field with details.

### Step 2: Determine Build Target

**CRITICAL**: Build targets MUST correspond to `.Target.cs` files in the Source/ directory. Plugin modules cannot be built directly as standalone targets.

**Valid Build Targets**:
- Targets defined by `.Target.cs` files (e.g., `MyProject.Target.cs` → use "MyProject")
- Common patterns: `ProjectName`, `ProjectNameEditor`, `ProjectNameServer`, `ProjectNameClient`
- Check `targets.available_targets` from detection output for all valid targets

**Target Selection Logic**:
1. **If user specifies a target explicitly**: Verify it exists in `targets.available_targets` first
2. **If recent code changes are in a plugin directory**:
   - **DO NOT** try to build the plugin name directly (this will fail)
   - Build the main project target instead (e.g., "ProjectName" or "ProjectNameEditor")
   - The plugin will be compiled as part of the project build
3. **If changes are in project Source/**: Build the primary project target
4. **Default behavior**: Use the first target from `targets.available_targets`, typically the project name

**Example Target Selection**:
```
Project: BoidsDemo.uproject
Available targets from .Target.cs files:
  - BoidsDemo (from BoidsDemo.Target.cs)
  - BoidsDemoEditor (from BoidsDemoEditor.Target.cs)

❌ WRONG: "BoidsFormation" (plugin module, no .Target.cs)
✅ CORRECT: "BoidsDemo" or "BoidsDemoEditor"
```

### Step 3: Choose Build Configuration

**Available configurations**:
- **Development** - Standard development build, good balance
- **DebugGame** - Debugging with some optimizations, for deep debugging
- **Shipping** - Fully optimized, for final release
- **Debug** - Full debug, very slow, rarely used

**Configuration Selection Priority**:
1. **User explicitly specifies** a configuration → use that
2. **Last build configuration detected** from `last_build_configuration.configuration` → use the same to avoid full rebuild
3. **Default fallback** → use "Development"

**Why reuse last configuration?**
Changing configurations (e.g., DebugGame → Development) forces a complete rebuild of all modules, which can take significantly longer. Reusing the same configuration enables incremental builds that only recompile changed files.

**Example logic**:
```python
if user_specified_config:
    config = user_specified_config
elif last_build_config and last_build_config["configuration"]:
    config = last_build_config["configuration"]
    # Inform user: "Using last build configuration: DebugGame"
else:
    config = "Development"
```

### Step 4: Execute Build Command

#### Windows

```bash
"<engine.build_tool>" <Target> Win64 <Configuration> -Project="<uproject.path>" -WaitMutex -FromMsBuild
```

#### Linux/Mac

```bash
"<engine.build_tool>" <Target> Linux <Configuration> -Project="<uproject.path>" -WaitMutex
```

**Critical flags**:
- `-WaitMutex`: Prevents conflicting parallel builds
- `-FromMsBuild`: Better output formatting on Windows
- `-Project=`: Full path to .uproject file

**Example**:
```bash
"C:/Program Files/Epic Games/UE_5.4/Engine/Build/BatchFiles/Build.bat" MyPlugin Win64 Development -Project="C:/Projects/MyGame/MyGame.uproject" -WaitMutex -FromMsBuild
```

### Step 5: Monitor Build Output

Watch for these patterns in the output:

**Success indicators**:
- `BUILD SUCCEEDED` or `Build succeeded`
- Exit code 0
- No error messages

**Error indicators**:
- `error C2065:` - Undeclared identifier
- `error C2039:` - Member not found
- `error LNK2019:` - Unresolved external symbol (linker error)
- `error:` - General error pattern
- Exit code non-zero

**Warning indicators** (note but don't fail):
- `warning C4996:` - Deprecated API
- `warning:` - General warnings

### Step 6: Parse and Report Results

Provide a concise, actionable summary:

**On Success**:
```
✅ Build succeeded (Development configuration)
Target: MyPlugin
Time: 45.2 seconds
0 errors, 3 warnings
```

**On Failure**:
```
❌ Build failed (2 errors)

Errors:
1. BoidsDisperseLibrary.cpp(142): error C2065: 'UndeclaredVariable' undeclared identifier
2. BoidsFormation.cpp(89): error LNK2019: unresolved external symbol "class UBoidsManager"

Suggest:
- Check BoidsDisperseLibrary.cpp:142
- Verify UBoidsManager is included and linked
```

**Include**:
- Status (success/failure)
- Error count and details with file:line references
- Warning count (summary only)
- Build time
- Suggested next steps for failures

## Advanced Usage

### Clean Rebuild

For a clean rebuild, delete intermediate files first:

```bash
# Windows
rmdir /s /q Intermediate Binaries
# Linux/Mac
rm -rf Intermediate/ Binaries/
```

Then run the build normally.

### Building Multiple Targets

If the user wants to build multiple targets, run them **sequentially** (not parallel):

```bash
# Build plugin first
"<build_tool>" PluginName Win64 Development -Project="<uproject>" -WaitMutex -FromMsBuild

# Then build main project
"<build_tool>" ProjectName Win64 Development -Project="<uproject>" -WaitMutex -FromMsBuild
```

### Handling Source Builds

If `engine.type == "source"`:
- The build process is the same
- Build times may be longer
- More detailed output is available
- Source modifications affect the engine itself

## Common Issues and Solutions

| Issue | Solution |
|-------|----------|
| **Mutex timeout** | Another build is running; wait or kill UBT process |
| **Missing .uproject** | Navigate to project root directory |
| **Engine not found** | Check EngineAssociation in .uproject; verify UE installation |
| **Target not found** | Verify target has a .Target.cs file in Source/; use `available_targets` from detection |
| **Plugin build failure** | Don't build plugins directly; build the main project target instead |
| **Linker errors (LNK2019)** | Check dependencies in .Build.cs files |
| **Permission denied** | Run as administrator or check file locks |

## Platform-Specific Notes

### Windows
- Use `Build.bat` in `Engine/Build/BatchFiles/`
- Requires Visual Studio or Build Tools
- Output is colorized in newer UE versions

### Linux
- Use `Build.sh` in `Engine/Build/BatchFiles/`
- Requires clang toolchain
- May need `mono` for older UE versions

### macOS
- Use `Build.sh` in `Engine/Build/BatchFiles/`
- Requires Xcode command-line tools
- Universal binaries for M1/Intel

## Tips for Effective Building

1. **Always detect first**: Run `detect_ue.py` to get accurate paths
2. **Check git status**: See what files changed to infer the right target
3. **Start with Development**: Fastest for iteration
4. **Read the error**: UE provides file:line information
5. **Incremental builds**: Only rebuild what changed (UBT handles this)
6. **Clean when stuck**: Delete Intermediate/ for stubborn issues

## Integration with Testing

After a successful build, you can chain testing:

```
Build → Success → Run tests with /ue-test
Build → Failure → Report errors, don't test
```

See the `/ue-test` skill for running automation tests after build.
