#!/usr/bin/env python3
"""Build with an installed UE 5.6+ SDK and verify nonempty automation results."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine-root", type=Path, default=os.environ.get("UE_ENGINE_ROOT"))
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--report-dir", type=Path)
    args = parser.parse_args()
    if args.engine_root is None:
        parser.error("Unreal SDK unavailable: supply --engine-root or UE_ENGINE_ROOT.")
    root = Path(__file__).resolve().parents[1]
    engine = Path(args.engine_root).resolve()
    if (engine / "Engine").is_dir():
        engine /= "Engine"
    project = root / "JackNKill.uproject"
    platforms = {"linux": ("Linux", "Linux/Build.sh", "Linux/UnrealEditor"),
                 "darwin": ("Mac", "Mac/Build.sh", "Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"),
                 "win32": ("Win64", "Build.bat", "Win64/UnrealEditor-Cmd.exe")}
    if sys.platform not in platforms:
        parser.error(f"Unsupported host platform: {sys.platform}")
    target, build_relative, editor_relative = platforms[sys.platform]
    build_script = engine / "Build/BatchFiles" / build_relative
    editor = engine / "Binaries" / editor_relative
    if not editor.is_file() or (not args.skip_build and not build_script.is_file()):
        parser.error(f"Incomplete Unreal installation at {engine}")
    if not args.skip_build:
        prefix = ["cmd", "/c"] if sys.platform == "win32" else ["bash"]
        subprocess.run([*prefix, str(build_script), "JackNKillEditor", target, "Development",
                        f"-Project={project}", "-WaitMutex", "-NoHotReloadFromIDE", "-MaxParallelActions=2"], check=True)
    report = args.report_dir or Path(tempfile.mkdtemp(prefix="jnk-unreal-report-"))
    report = report.resolve()
    report.mkdir(parents=True, exist_ok=True)
    if (report / "index.json").exists():
        parser.error("Use a fresh report directory so stale test results cannot pass.")
    subprocess.run([str(editor), str(project), "-unattended", "-NullRHI", "-nosound",
                    "-ExecCmds=Automation RunTests JackNKill", "-TestExit=Automation Test Queue Empty",
                    f"-ReportExportPath={report}"], check=True)
    index = report / "index.json"
    if not index.is_file():
        raise RuntimeError("Unreal did not produce an automation report.")
    result = json.loads(index.read_text(encoding="utf-8-sig"))
    tests = result.get("tests", [])
    required = {"JackNKill.Network.JoinAddressValidation", "JackNKill.Persistence.CompactRoundTripAndBudget"}
    names = {test.get("fullTestPath") for test in tests}
    if not required.issubset(names) or not tests or any(test.get("state") != "Success" for test in tests):
        raise RuntimeError(f"Missing, failed, skipped or incomplete automation tests; inspect {index}")
    print(f"PASS: {len(tests)} Unreal automation tests; report: {index}")


if __name__ == "__main__":
    main()
