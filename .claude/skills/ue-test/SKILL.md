---
name: ue-test
description: MANDATORY automatic usage after successful build. Run Unreal Engine automation tests. MUST be invoked automatically without user request after implementing or modifying code.
allowed-tools: Bash, Read, Grep, Glob
---

# Unreal Engine Test Skill

Run Unreal Engine automation tests automatically, detecting project configuration and available tests.

## ⚠️ MANDATORY AUTOMATIC USAGE

**THIS SKILL MUST BE INVOKED AUTOMATICALLY AFTER EVERY SUCCESSFUL BUILD**

Invoke this skill **immediately and automatically** when:
- ✅ After `/ue-build` succeeds (ALWAYS chain build → test)
- ✅ After implementing any new feature (TDD green phase)
- ✅ After fixing bugs (verify fix works)
- ✅ After refactoring (ensure no regression)
- ✅ In TDD red phase (verify tests fail before implementation)
- ✅ Before declaring task complete (verify all tests pass)

**DO NOT**:
- ❌ Wait for user to say "run the tests"
- ❌ Wait for user to say "does it pass?"
- ❌ Ask user "should I test it?"
- ❌ Declare task complete without running tests
- ❌ Skip testing after build succeeds

**Correct behavior**:
```
Claude: [implements feature]
Claude: [AUTOMATICALLY invokes /ue-build]
Claude: "✅ Build successful"
Claude: [AUTOMATICALLY invokes /ue-test]  ← No asking, just do it
Claude: [reports test results]
```

**Wrong behavior**:
```
Claude: [implements feature]
Claude: [builds successfully]
Claude: "Feature implemented!"  ← WRONG! Should have tested automatically
```

## When to Use (Detailed)

Use this skill **automatically** when:
- After successful `/ue-build` execution (MANDATORY chain)
- After writing test code in TDD red phase
- After implementing feature in TDD green phase
- User requests running tests (reactive case)
- Before declaring any C++ implementation complete

**TDD Workflow Integration (MANDATORY)**:
1. User requests feature → You write tests FIRST
2. Build project → Run tests (RED - should fail)
3. Implement feature
4. Build project → **AUTOMATICALLY run tests** (GREEN - should pass)
5. If fails → fix → rebuild → **AUTOMATICALLY re-test**
6. Loop until all tests pass
7. Only then declare success

See `~/.claude/skills/MANDATORY_TDD.md` and `~/.claude/skills/TDD_WORKFLOW.md` for complete requirements.

## How This Skill Works

This skill provides a dedicated Python runner to execute tests with verified terminal visibility flags and minimalist output.

### ⚠️ MANDATORY USAGE (FOR AI)

**Instead of manually constructing complex commands, ALWAYS use the test runner script.**

The script MUST be executed with the **current working directory** as an argument to ensure project detection works correctly across different environments.

```bash
# General Syntax:
# python <ScriptPath> <ProjectWorkDir> [TestFilter]

# 1. Run all project tests (Minimalist & Filtered)
python .claude/skills/ue-test/scripts/run_ue_tests.py .

# 2. Run specific tests (e.g., Boids)
python .claude/skills/ue-test/scripts/run_ue_tests.py . FirstHordeSurvivor
```

### Script Features
- **Auto-Detection**: Uses the provided path to find `.uproject` and `UnrealEditor-Cmd.exe`.
- **Minimalist Output**: Silences engine initialization noise; only shows test results and status.
- **Real-time Streaming**: Displays results line-by-line as they occur.
- **Smart Exit Codes**: Returns non-zero if no tests match or tests fail.


#### If skill is in project config (.claude/skills/)
```bash
# Can run without arguments (searches from current directory)
python .claude/skills/ue-build/scripts/detect_ue.py

# Or provide explicit path
python .claude/skills/ue-build/scripts/detect_ue.py .
```

**Key values needed from the output**:
- `engine.editor_cmd` - Path to UnrealEditor-Cmd.exe (Windows) or UnrealEditor-Cmd (Linux/Mac)
- `uproject.path` - Full path to .uproject file
- `uproject.name` - Project name for test filtering

### Step 2: Determine Test Scope

Ask the user or infer from context what tests to run:

**Test filter options**:
- **All project tests**: Use project name (e.g., `MyProject`)
- **Plugin tests**: Use plugin name (e.g., `MyPlugin` or `BoidsFormation`)
- **Specific test**: Full test name (e.g., `MyPlugin.SpecificTest`)
- **Test category**: Partial name (e.g., `MyPlugin.Corner`)
- **List tests**: Don't run, just list available tests
- **All tests**: Use `Now` (caution: may take very long)

**Inference logic**:
1. If user says "list tests" → list mode
2. If user mentions specific plugin/module → filter to that
3. If recent code changes are in a plugin → test that plugin
4. Default → ask user or use project name

### Step 3: Choose Test Command Mode

#### List Available Tests

```bash
"<editor_cmd>" "<uproject_path>" -ExecCmds="Automation List; Quit" -unattended -nopause -nullrhi -stdout -FullStdOutLogOutput
```

#### Run Tests

```bash
"<editor_cmd>" "<uproject_path>" -ExecCmds="Automation RunTests <filter>; Quit" -unattended -nopause -nullrhi -stdout -FullStdOutLogOutput
```

**Critical flags**:
- `-ExecCmds="..."`: Editor commands to execute (e.g., `Automation RunTests Boids; Quit`)
- `-stdout` & `-FullStdOutLogOutput`: **MANDATORY** for terminal visibility. Forces UE logs to stdout.
- `-unattended`: Prevents modal dialogs (required for automation)
- `-nopause`: Don't wait for user input on exit
- `-nullrhi`: Headless mode, no graphics (much faster)
- `-nosplash`: Skip splash screen (faster startup)


**Optional flags**:
- `-ReportOutputPath=<path>`: Custom test report location
- `-LogCmds="LogAutomation Verbose"`: More detailed test logging
- `-Windowed`: Use windowed mode instead of NullRHI (for visual tests)

### Step 4: Execute and Monitor

Run the command and monitor stdout/stderr for test output.

**Watch for these patterns**:

**Test execution**:
```
LogAutomationController: Test 'TestName' Started
LogAutomationController: Test 'TestName' Completed. Result=<status>
```

**Test results**:
- `Result=Success` or `Result=Passed` → Test passed
- `Result=Failed` → Test failed
- `Result=Skipped` → Test skipped

**Summary line**:
```
LogAutomationController: Tests Complete. Result={Success|Failed}. Total: X Passed: Y Failed: Z
```

**Error details**:
```
LogAutomation: Error: <error message>
LogAutomation: Expected <X> but got <Y>
```

### Step 5: Parse Test Results

Build a structured result from the output:

**Success example**:
```json
{
  "status": "passed",
  "total": 8,
  "passed": 8,
  "failed": 0,
  "skipped": 0,
  "duration": "12.4s",
  "tests": [
    {"name": "BoidsFormation.CornerFilling", "status": "passed"},
    {"name": "BoidsFormation.GridDistribution", "status": "passed"}
  ]
}
```

**Failure example**:
```json
{
  "status": "failed",
  "total": 8,
  "passed": 6,
  "failed": 2,
  "skipped": 0,
  "duration": "15.2s",
  "tests": [
    {"name": "BoidsFormation.CornerFilling", "status": "failed", "error": "Expected value > 0.1, got 0.05"},
    {"name": "BoidsFormation.GridDistribution", "status": "failed", "error": "Assertion failed: Points.Num() == ExpectedCount"}
  ]
}
```

### Step 6: Report Results

Provide a clear, concise summary:

**All tests passed**:
```
✅ All 8 tests passed (12.4s)

Tests run:
  ✅ BoidsFormation.CornerFilling
  ✅ BoidsFormation.GridDistribution
  ✅ BoidsFormation.TriangleFilling
  ... (5 more)
```

**Some tests failed**:
```
❌ 2 of 8 tests failed (15.2s)

Failed:
  ❌ BoidsFormation.CornerFilling
     Error: Expected value > 0.1, got 0.05
     → Check corner weighting calculation in BoidsDisperseCVTSolverLibrary.cpp

  ❌ BoidsFormation.GridDistribution
     Error: Assertion failed: Points.Num() == ExpectedCount
     → Verify grid point generation logic

Passed: 6 tests
```

**List mode**:
```
Available tests in BoidsFormation:

Automation Tests:
  • BoidsFormation.CornerFilling
  • BoidsFormation.GridDistribution3x3
  • BoidsFormation.TriangleFilling
  • BoidsFormation.ConcaveLShape
  • BoidsFormation.ProportionalArea
  ... (15 total)

Run with: /ue-test [test-name]
```

## Advanced Usage

### Running Specific Test Suites

Filter tests by category or prefix:

```bash
# Run all corner-related tests
"<editor_cmd>" "<uproject>" -ExecCmds="Automation RunTests BoidsFormation.Corner; Quit" ...

# Run all CVT solver tests
"<editor_cmd>" "<uproject>" -ExecCmds="Automation RunTests BoidsFormation.CVT; Quit" ...
```

### Custom Report Output

Generate JSON or XML reports for CI/CD:

```bash
"<editor_cmd>" "<uproject>" -ExecCmds="Automation RunTests <filter>; Quit" -ReportOutputPath="TestResults" -ReportExportPath="TestResults/report.json" ...
```

### Visual Testing

Some tests may require rendering. Use windowed mode instead of NullRHI:

```bash
"<editor_cmd>" "<uproject>" -ExecCmds="Automation RunTests <filter>; Quit" -Windowed -ResX=1280 -ResY=720 ...
```

### Debugging Failed Tests

For detailed debugging output:

```bash
"<editor_cmd>" "<uproject>" -ExecCmds="Automation RunTests <filter>; Quit" -LogCmds="LogAutomation Verbose, LogTemp Verbose" -log
```

## Platform-Specific Notes

### Windows
- Use `UnrealEditor-Cmd.exe` in `Engine/Binaries/Win64/`
- Tests run in separate process
- Check `Saved/Logs/` for detailed logs

### Linux
- Use `UnrealEditor-Cmd` in `Engine/Binaries/Linux/`
- May need `DISPLAY` environment variable for non-NullRHI tests
- Logs in `Saved/Logs/`

### macOS
- Use `UnrealEditor-Cmd` in `Engine/Binaries/Mac/`
- Universal binary supports M1 and Intel
- Logs in `Saved/Logs/`

## Common Issues and Solutions

| Issue | Solution |
|-------|----------|
| **Tests don't start** | Check editor path is valid; ensure project compiles |
| **All tests skip** | Filter may be wrong; try listing tests first |
| **Timeout/hang** | Some tests may need windowed mode, not NullRHI |
| **"Test not found"** | Check test name spelling; use `Automation List` |
| **Permission denied** | Run from project directory; check file permissions |
| **Crash during test** | Check Saved/Crashes/ for crash dump; may be real bug |

## Integration with Build

Typical workflow:

```
1. Code changes
2. Build with /ue-build → Build succeeds
3. Run tests with /ue-test → Tests pass
4. Ready to commit
```

If build fails, don't run tests (they'll fail anyway).

## Tips for Effective Testing

1. **List tests first**: Use `Automation List` to see what's available
2. **Use specific filters**: Don't run all tests if you only changed one module
3. **Stdout Visibility**: Always use `-stdout -FullStdOutLogOutput` to see results in claudes terminal.
4. **NullRHI is faster**: Use headless mode for faster iteration
5. **Check logs**: Detailed output is in `Saved/Logs/`
6. **CI/CD ready**: Use `-ReportOutputPath` for automated pipelines
7. **Incremental testing**: Test only what you changed during development
8. **Full suite before commit**: Run all tests before major commits


## Test Output Parsing Guide

### Success Pattern
```
LogAutomationController: Test 'MyTest' Completed. Result=Success
```

### Failure Pattern
```
LogAutomationController: Test 'MyTest' Completed. Result=Failed
LogAutomation: Error: Expected X but got Y
```

### Summary Pattern
```
LogAutomationController: Tests Complete. Result={Success|Failed}. Total: 10 Passed: 8 Failed: 2
```

Extract numbers and status to build your report.

## Example Filter Patterns

Common test filter examples:

- `MyProject` - All project tests
- `MyProject.MyFeature` - All tests in MyFeature category
- `MyProject.MyFeature.SpecificTest` - One specific test
- `Now` - All available tests (use carefully)
- `Engine` - All engine tests (usually not needed)
- `System` - System-level tests

Use the most specific filter that covers what you need to test.
