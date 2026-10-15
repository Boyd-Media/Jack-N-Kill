#!/usr/bin/env python3
"""Compile and execute production gameplay-rule tests without an Unreal SDK."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="g++")
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    compiler = shutil.which(args.compiler)
    if not compiler:
        parser.error(f"Compiler not available: {args.compiler}")
    root = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix="jnk-native-tests-") as directory:
        executable = Path(directory) / "gameplay_rules_tests"
        command = [compiler, "-std=c++20", "-Wall", "-Wextra", "-Wpedantic", "-Werror",
                   "-I", str(root / "Source/JackNKill/Public"),
                   str(root / "Tests/Native/GameplayRulesTests.cpp"), "-o", str(executable)]
        command += ["-O1", "-g", "-fno-omit-frame-pointer", "-fsanitize=address,undefined"] if args.sanitize else ["-O2"]
        subprocess.run(command, check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
