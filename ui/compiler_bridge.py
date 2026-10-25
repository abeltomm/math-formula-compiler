"""Safe subprocess interface between Python and the C compiler executable.

The formula is sent on stdin as bytes and variables are passed as separate
argv items (no shell is ever involved), so formula text can never be
interpreted as a command. All results come from the compiler's JSON output.
"""
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


class CompilerError(Exception):
    """The compiler executable is missing, crashed, or returned unusable output."""


def find_compiler():
    exe = "math_compiler.exe" if os.name == "nt" else "math_compiler"
    candidates = [os.environ.get("MATH_COMPILER_EXE", ""), os.path.join(ROOT, "build", exe)]
    for path in candidates:
        if path and os.path.isfile(path):
            return path
    raise CompilerError(
        "Compiler executable not found. Build it first:\n"
        "  Windows:  build.bat   (or: mingw32-make)\n"
        "  Linux/macOS:  make\n"
        "Expected at: " + candidates[1])


def parse_variables(text):
    """Turn 'x=3, y=4.5' into ['x=3', 'y=4.5']. Raises ValueError on bad input."""
    items = []
    for part in re.split(r"[,;\s]+", text.strip()):
        if not part:
            continue
        if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*=[-+]?(\d+(\.\d+)?|\.\d+)([eE][-+]?\d+)?", part):
            raise ValueError("Bad variable assignment '%s' (use name=number, e.g. x=3, y=4.5)" % part)
        items.append(part)
    return items


def run_compiler(formula, evaluate=False, variables="", timeout=10):
    """Run the real C compiler on `formula` and return its JSON report as a dict."""
    exe = find_compiler()
    args = [exe, "--json"]
    if evaluate:
        args.append("--eval")
        for v in parse_variables(variables):
            args += ["--var", v]
    try:
        proc = subprocess.run(args, input=formula.encode("utf-8"), capture_output=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        raise CompilerError("The compiler did not finish within %d seconds." % timeout)
    except OSError as exc:
        raise CompilerError("Could not start the compiler: %s" % exc)
    if proc.returncode not in (0, 1, 3):
        raise CompilerError("The compiler exited abnormally (code %s).\n%s" %
                            (proc.returncode, proc.stderr.decode("utf-8", "replace")))
    try:
        return json.loads(proc.stdout.decode("utf-8", "replace"))
    except ValueError as exc:
        raise CompilerError("The compiler produced invalid output: %s" % exc)
