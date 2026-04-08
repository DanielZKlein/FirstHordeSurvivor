#!/usr/bin/env python3
"""
Detect Unreal Engine installation paths and project configuration.
This script helps Claude find UE binaries regardless of installation type.

Usage:
    python detect_ue.py [project_path]

Arguments:
    project_path: Optional path to .uproject file or directory containing .uproject
                  If not provided, searches from current working directory
"""

import json
import os
import sys
import platform
from pathlib import Path


def find_uproject_file(start_dir=None):
    """Find the .uproject file in the given directory or parent directories."""
    if start_dir is None:
        start_dir = os.getcwd()

    start_path = Path(start_dir).resolve()

    # If it's a .uproject file directly
    if start_path.is_file() and start_path.suffix == ".uproject":
        return start_path

    # If it's a directory, search it and parent directories
    if start_path.is_dir():
        current = start_path
        # Search current directory and up to 5 parent directories
        for _ in range(6):
            for uproject in current.glob("*.uproject"):
                return uproject

            parent = current.parent
            if parent == current:  # Reached root
                break
            current = parent

    return None


def read_uproject(uproject_path):
    """Read and parse .uproject file."""
    try:
        with open(uproject_path, 'r', encoding='utf-8') as f:
            return json.load(f)
    except Exception as e:
        return {"error": f"Failed to read .uproject: {e}"}


def detect_ue_from_uproject(uproject_data):
    """Detect UE version from .uproject metadata."""
    engine_association = uproject_data.get("EngineAssociation", "")

    # Format examples:
    # "5.4" - Launcher version
    # "C:/UnrealEngine" - Custom/source build path
    # "{GUID}" - Custom engine registered in registry

    return engine_association


def find_ue_launcher_installations():
    """Find UE installations from Epic Games Launcher."""
    installations = []

    if platform.system() == "Windows":
        # Common launcher installation paths
        program_files_paths = [
            Path(os.environ.get("ProgramW6432", "C:/Program Files")),
            Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")),
            Path(os.environ.get("ProgramFiles", "C:/Program Files"))
        ]

        for pf in program_files_paths:
            epic_games = pf / "Epic Games"
            if epic_games.exists():
                for ue_dir in epic_games.iterdir():
                    if ue_dir.is_dir() and ue_dir.name.startswith("UE_"):
                        version = ue_dir.name.replace("UE_", "")
                        build_bat = ue_dir / "Engine" / "Build" / "BatchFiles" / "Build.bat"
                        editor_cmd = ue_dir / "Engine" / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe"

                        if build_bat.exists():
                            installations.append({
                                "version": version,
                                "path": str(ue_dir),
                                "type": "launcher",
                                "build_tool": str(build_bat),
                                "editor_cmd": str(editor_cmd) if editor_cmd.exists() else None
                            })

    elif platform.system() == "Darwin":  # macOS
        ue_library = Path.home() / "Library" / "Application Support" / "Epic" / "UnrealEngine"
        # TODO: Add macOS detection logic

    elif platform.system() == "Linux":
        # TODO: Add Linux detection logic
        pass

    return installations


def find_ue_from_registry(guid):
    """Find UE installation from Windows registry using GUID."""
    if platform.system() != "Windows":
        return None

    try:
        import winreg
    except ImportError:
        return None

    # Registry paths to check
    registry_paths = [
        (winreg.HKEY_CURRENT_USER, r"Software\Epic Games\Unreal Engine\Builds"),
        (winreg.HKEY_LOCAL_MACHINE, r"Software\Epic Games\Unreal Engine\Builds"),
        (winreg.HKEY_LOCAL_MACHINE, r"Software\Wow6432Node\Epic Games\Unreal Engine\Builds"),
    ]

    for root_key, subkey_path in registry_paths:
        try:
            with winreg.OpenKey(root_key, subkey_path) as key:
                ue_path, _ = winreg.QueryValueEx(key, guid)
                ue_path = Path(ue_path)

                if ue_path.exists() and ue_path.is_dir():
                    build_bat = ue_path / "Engine" / "Build" / "BatchFiles" / "Build.bat"
                    editor_cmd = ue_path / "Engine" / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe"

                    if build_bat.exists():
                        # Try to detect version from Build.version
                        version = "custom"
                        version_file = ue_path / "Engine" / "Build" / "Build.version"
                        if version_file.exists():
                            try:
                                import json
                                with open(version_file, 'r') as f:
                                    version_data = json.load(f)
                                    major = version_data.get("MajorVersion", 5)
                                    minor = version_data.get("MinorVersion", 0)
                                    version = f"{major}.{minor}"
                            except:
                                pass

                        return {
                            "version": version,
                            "path": str(ue_path),
                            "type": "custom",
                            "build_tool": str(build_bat),
                            "editor_cmd": str(editor_cmd) if editor_cmd.exists() else None,
                            "guid": guid
                        }
        except (OSError, FileNotFoundError):
            continue

    return None


def find_ue_source_build(engine_association):
    """Find UE source build from custom path or GUID."""
    if not engine_association:
        return None

    # Handle GUID-based engine associations (e.g., "{12345678-1234-1234-1234-123456789012}")
    if engine_association.startswith("{") and engine_association.endswith("}"):
        return find_ue_from_registry(engine_association)

    # Handle path-based engine associations (e.g., "C:/UnrealEngine")
    ue_path = Path(engine_association)
    if ue_path.exists() and ue_path.is_dir():
        if platform.system() == "Windows":
            build_bat = ue_path / "Engine" / "Build" / "BatchFiles" / "Build.bat"
            editor_cmd = ue_path / "Engine" / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe"
        else:
            build_bat = ue_path / "Engine" / "Build" / "BatchFiles" / "Build.sh"
            editor_cmd = ue_path / "Engine" / "Binaries" / platform.system() / "UnrealEditor-Cmd"

        if build_bat.exists():
            # Try to detect version
            version = "source"
            version_file = ue_path / "Engine" / "Build" / "Build.version"
            if version_file.exists():
                try:
                    import json
                    with open(version_file, 'r') as f:
                        version_data = json.load(f)
                        major = version_data.get("MajorVersion", 5)
                        minor = version_data.get("MinorVersion", 0)
                        version = f"{major}.{minor}"
                except:
                    pass

            return {
                "version": version,
                "path": str(ue_path),
                "type": "source",
                "build_tool": str(build_bat),
                "editor_cmd": str(editor_cmd) if editor_cmd.exists() else None
            }

    return None


def detect_last_build_configuration(uproject_path):
    """Detect the last build configuration used by examining binaries."""
    project_dir = Path(uproject_path).parent
    binaries_dir = project_dir / "Binaries" / "Win64"

    if not binaries_dir.exists():
        return None

    # Look for .dll or .target files with configuration suffixes
    # Note: Development builds often don't have a suffix, just the module name
    config_patterns = {
        "DebugGame": ["*-Win64-DebugGame.dll", "*-Win64-DebugGame.target"],
        "Shipping": ["*-Win64-Shipping.dll", "*-Win64-Shipping.target"],
        "Debug": ["*-Win64-Debug.dll", "*-Win64-Debug.target"],
        "Development": ["*-Win64-Development.dll", "*-Win64-Development.target"]
    }

    # Find most recent binary for each configuration
    most_recent = {}
    for config, patterns in config_patterns.items():
        for pattern in patterns:
            for file_path in binaries_dir.glob(pattern):
                if file_path.is_file():
                    mtime = file_path.stat().st_mtime
                    if config not in most_recent or mtime > most_recent[config]["mtime"]:
                        most_recent[config] = {"mtime": mtime, "file": file_path.name}

    # Also check for Development binaries without suffix (UnrealEditor-ProjectName.dll)
    # These are typically Development builds
    project_name = Path(uproject_path).stem
    dev_patterns = [
        f"UnrealEditor-{project_name}.dll",
        f"{project_name}Editor.dll"
    ]

    for pattern in dev_patterns:
        for file_path in binaries_dir.glob(pattern):
            if file_path.is_file():
                # Only count as Development if no explicit config suffix
                if not any(suffix in file_path.name for suffix in ["-DebugGame", "-Debug", "-Shipping", "-Development"]):
                    mtime = file_path.stat().st_mtime
                    if "Development" not in most_recent or mtime > most_recent["Development"]["mtime"]:
                        most_recent["Development"] = {"mtime": mtime, "file": file_path.name}

    # Return the configuration with the most recent binary
    if most_recent:
        latest_config = max(most_recent.items(), key=lambda x: x[1]["mtime"])
        return {
            "configuration": latest_config[0],
            "last_modified": latest_config[1]["mtime"],
            "file": latest_config[1]["file"]
        }

    return None


def get_build_targets(uproject_path, uproject_data):
    """Extract build targets from .uproject and scan for available .Target.cs files."""
    targets = []

    # Main modules
    modules = uproject_data.get("Modules", [])
    for module in modules:
        module_name = module.get("Name", "")
        if module_name:
            targets.append({
                "name": module_name,
                "type": module.get("Type", "Runtime"),
                "loading_phase": module.get("LoadingPhase", "Default")
            })

    # Plugins
    plugins = uproject_data.get("Plugins", [])
    plugin_names = [p.get("Name") for p in plugins if p.get("Enabled", True)]

    # Scan for .Target.cs files in Source directory to find available build targets
    project_dir = Path(uproject_path).parent
    source_dir = project_dir / "Source"
    available_targets = []

    if source_dir.exists():
        for target_file in source_dir.glob("*.Target.cs"):
            target_name = target_file.stem.replace(".Target", "")
            if target_name not in [t["name"] for t in targets]:
                available_targets.append(target_name)

    return {
        "modules": targets,
        "plugins": plugin_names,
        "available_targets": available_targets
    }


def main():
    """Main detection logic."""
    # Parse command-line arguments
    project_path = None
    if len(sys.argv) > 1:
        project_path = sys.argv[1]

    # Find .uproject file
    uproject_path = find_uproject_file(project_path)

    if not uproject_path:
        error_msg = {
            "error": "No .uproject file found"
        }
        if project_path:
            error_msg["searched_path"] = str(Path(project_path).resolve())
        else:
            error_msg["searched_path"] = os.getcwd()
        error_msg["help"] = "Usage: python detect_ue.py [project_path]"
        print(json.dumps(error_msg, indent=2))
        return 1

    # Read .uproject
    uproject_data = read_uproject(uproject_path)
    if "error" in uproject_data:
        print(json.dumps(uproject_data))
        return 1

    # Detect engine version
    engine_association = detect_ue_from_uproject(uproject_data)

    # Find UE installations
    launcher_installations = find_ue_launcher_installations()
    source_installation = find_ue_source_build(engine_association)

    # Determine which installation to use based on EngineAssociation priority:
    # 1. GUID (registry lookup) - custom/source build
    # 2. Path - direct path to engine
    # 3. Version string - launcher installation
    # 4. Fallback to latest launcher version
    selected_installation = None

    if engine_association:
        # Priority 1: GUID-based (custom engine from registry)
        if engine_association.startswith("{") and source_installation:
            selected_installation = source_installation

        # Priority 2: Path-based (direct engine path)
        elif not engine_association.startswith("{") and "/" in engine_association or "\\" in engine_association:
            if source_installation:
                selected_installation = source_installation

        # Priority 3: Version string (launcher installation)
        else:
            for install in launcher_installations:
                if install["version"] == engine_association:
                    selected_installation = install
                    break

    # Fallback to latest launcher version if no match found
    if not selected_installation and launcher_installations:
        selected_installation = launcher_installations[-1]

    # Get build targets
    targets = get_build_targets(uproject_path, uproject_data)

    # Detect last build configuration
    last_build_config = detect_last_build_configuration(uproject_path)

    # Prepare output
    result = {
        "uproject": {
            "path": str(uproject_path),
            "name": uproject_path.stem,
            "directory": str(uproject_path.parent),
            "engine_association": engine_association
        },
        "targets": targets,
        "engine": selected_installation,
        "available_installations": launcher_installations,
        "last_build_configuration": last_build_config,
        "platform": platform.system()
    }

    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
