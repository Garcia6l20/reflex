import subprocess
from pathlib import Path

if __name__ == "__main__":
    import sys

    tests = sys.argv[1:]
    if not tests:
        print("No tests to run")
        sys.exit(0)
    print(f"Running tests: {', '.join(tests)}")

    pass_count = 0
    fail_count = 0

    for test in tests:
        print(f"⏳ {test}...", end=" ", flush=True)
        try:
            result = subprocess.run(test, capture_output=True, text=True)
        except Exception as e:
            print(f"\r❌\n  - Error running test {test}: {e}")
            sys.exit(1)
        if result.returncode != 0:
            print("\r❌")
            if result.stdout:
                print(result.stdout)
            if result.stderr:
                print(result.stderr)
            print(f"Test {test} failed with exit code {result.returncode}")
            fail_count += 1
        else:
            pass_count += 1
            print("\r✔️")
    if fail_count > 0:
        print(
            f"❌ Some tests failed: 💀 {fail_count} failed, 🚀 {pass_count} passed ({pass_count / (pass_count + fail_count) * 100:.1f}%)"
        )
        sys.exit(1)
    else:
        print(f"🚀 All {pass_count} tests passed !")
    sys.exit(0)
