import sys
import subprocess
Import("env")

def before_upload(source, target, env):
    print("\n=======================================================")
    print("   AUTOMATICALLY ERASING FLASH BEFORE UPLOAD")
    print("=======================================================\n")
    # This invokes platformio's built-in erase target cleanly using the current python env
    subprocess.run([sys.executable, "-m", "platformio", "run", "--target", "erase"])

env.AddPreAction("upload", before_upload)
