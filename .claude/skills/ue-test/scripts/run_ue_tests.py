import subprocess
import json
import sys
import os

def run():
    # 1. Handle arguments: first arg is the project directory
    if len(sys.argv) < 2:
        print("Usage: python run_ue_tests.py <ProjectDirectory> [TestFilter]")
        sys.exit(1)
    
    project_dir = sys.argv[1]
    test_filter_arg = sys.argv[2] if len(sys.argv) > 2 else None

    # Automatically resolve the path to detect_ue.py relative to this script
    # Expected: .../skills/ue-test/scripts/run_ue_tests.py
    # Target:   .../skills/ue-build/scripts/detect_ue.py
    script_dir = os.path.dirname(os.path.abspath(__file__))
    detect_script = os.path.abspath(os.path.join(script_dir, "..", "..", "ue-build", "scripts", "detect_ue.py"))
    
    if not os.path.exists(detect_script):
        # Fallback for alternative installations
        print(f"Error: Could not find detect_ue.py at {detect_script}")
        sys.exit(1)

    try:
        # Run detection on provided project directory
        result = subprocess.check_output([sys.executable, detect_script, project_dir], stderr=subprocess.STDOUT)
        config = json.loads(result)
    except subprocess.CalledProcessError as e:
        print(f"Error executing detection script: {e.output.decode()}")
        sys.exit(1)
    except Exception as e:
        print(f"Unexpected error during detection: {e}")
        sys.exit(1)

    if "error" in config:
        print(f"Detection error: {config['error']}")
        sys.exit(1)

    editor_cmd = config.get('engine', {}).get('editor_cmd')
    uproject_path = config.get('uproject', {}).get('path')

    if not editor_cmd or not uproject_path:
        print("Error: Could not determine UnrealEditor-Cmd path or .uproject path.")
        sys.exit(1)

    # 2. Set filter
    # Default: Use project name from detection if no filter provided
    project_name = config.get('uproject', {}).get('name', "")
    test_filter = test_filter_arg if test_filter_arg else project_name
    
    # 3. Construct the verified command
    # -ExecCmds="Automation RunTests <filter>; Quit"
    # -stdout -FullStdOutLogOutput: Mandatory for terminal visibility
    # -unattended -nullrhi -nosplash -nopause: For Headless execution
    cmd = [
        editor_cmd,
        uproject_path,
        f"-ExecCmds=Automation RunTests {test_filter}; Quit",
        "-stdout",
        "-FullStdOutLogOutput",
        "-unattended",
        "-nullrhi",
        "-nosplash",
        "-nopause"
    ]

    print(f"--- UE Automation Test Runner ---")
    print(f"Project: {os.path.basename(uproject_path)}")
    print(f"Filter:  {test_filter if test_filter else 'All Project Tests'}")
    print(f"Engine:  {config.get('engine', {}).get('version', 'Unknown')}")
    print(f"---------------------------------")
    
    # 4. Execute and stream output
    try:
        env = os.environ.copy()
        env["PYTHONUNBUFFERED"] = "1"
        
        process = subprocess.Popen(
            cmd, 
            stdout=subprocess.PIPE, 
            stderr=subprocess.STDOUT, 
            text=True, 
            bufsize=1,
            env=env
        )
        
        tests_found = False
        test_results = []
        
        # Stream and process output
        while True:
            line = process.stdout.readline()
            if not line and process.poll() is not None:
                break
            if line:
                # Detect "No tests matched" error
                if "No automation tests matched" in line:
                    print(f"ERROR: No tests matched filter '{test_filter}'")
                    tests_found = False
                
                # Detect "Found X tests"
                if "Found" in line and "automation tests based on" in line:
                    print(line.strip())
                    tests_found = True
                
                # Show Test Start/End
                if "Test Started." in line or "Test Completed." in line:
                    # Clean up the UE log format for minimalist view
                    # Format: LogAutomationController: Display: Test Completed. Result={Success} Name={...}
                    parts = line.split("Display: ")
                    if len(parts) > 1:
                        print(parts[1].strip())
                
                # Capture Errors/Warnings from Automation
                if "LogAutomation: Error:" in line or "LogAutomation: Warning:" in line:
                    print(line.strip())

        process.wait()
        
        if not tests_found and process.returncode != 0:
            sys.exit(1)

        if process.returncode == 0:
            print(f"\nOVERALL STATUS: SUCCESS")
        else:
            print(f"\nOVERALL STATUS: FAILED (Exit Code: {process.returncode})")
            
        sys.exit(process.returncode)
        
    except Exception as e:
        print(f"Failed to execute test command: {e}")
        sys.exit(1)

if __name__ == "__main__":
    run()
