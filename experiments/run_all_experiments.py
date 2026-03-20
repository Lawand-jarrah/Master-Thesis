import yaml
from commons import *
import subprocess

def run_command(command):
    process = subprocess.Popen(command, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    stdout, stderr = process.communicate()
    
    if process.returncode != 0:
        print(f"Command failed with error: {stderr.decode()}")
        return False
    else:
        return True

def main():
    commands1 = [
        "python3 experiment_datasets.py",
        "python3 experiment_datasets_xcmp.py",
    ]

    commands2 = [
        "python3 visualization.py",
    ]

    for command in commands1:
        if not run_command(command):
            return
    print("All experiments done!")
    
    for command in commands2:
        if not run_command(command):
            return
    print("All visualizations done!")

if __name__ == "__main__":
    main()
