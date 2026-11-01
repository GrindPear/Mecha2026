# AUTO_TUNE
# Auto tuner: try gains on the real motor and find the best ones
#   1. Run AUTO_CONTROL (C program) with gains
#   2. Get the score of the CSV file with EVALUATOR.py (smaller is better)
#   3. Decide the next gains in tune() and repeat
#   4. Draw all runs in one HTML graph with PLOT_GRAPH.py
# Write your own tuning method only in tune() (like positionControl() in C).
# Keep AUTO_CONTROL, EVALUATOR.py, PLOT_GRAPH.py and this file in the same folder.
# Run: python3 AUTO_TUNE.py

import os
import subprocess                   # Run a terminal command from Python
import time
from pathlib import Path
import EVALUATOR as ev              # Evaluation program (EVALUATOR.py)

os.chdir(Path(__file__).parent)     # Work in this file's folder (AUTO_CONTROL, CSV files)

# ===== Settings for running the motor =====
MAX_TIME = 4        # Run time [s] (keep the same while tuning)
ENC_PER_REV = 216   # Encoder counts per 1 rev

csv_files = []      # CSV files of all runs (for the graph)

def run_motor(reference, gains):
    """Run AUTO_CONTROL once and return the CSV file name.
    reference: target position [rev], gains: [P], [P, I] or [P, I, D]"""
    command = ["./AUTO_CONTROL", str(reference), str(MAX_TIME), str(ENC_PER_REV)]
    for gain in gains:                      # Add gains at the end
        command.append(str(gain))
    print("Run:", " ".join(command))        # Same as typing it in the terminal

    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:              # AUTO_CONTROL failed
        print(result.stdout)
        exit()

    time.sleep(1)                           # Wait until the motor stops
    return result.stdout.split()[-1]        # Last word: "20261101_204818_P10.csv"


def test(reference, gains):
    """Run the motor to reference with gains and return the score (smaller is better)."""
    csv_file = run_motor(reference, gains)
    csv_files.append(csv_file)              # Keep it for the graph
    score = ev.get_score(csv_file)
    print(f"-> score = {score:.5f}\n")
    return score


# ===== Tuning method (write your own here) =====
#   Use : score = test(reference, gains)
#         reference: target position [rev] (can be different every run)
#         gains: [P], [P, I] or [P, I, D]
#   Return: best gains and best score
#   Any method is OK. Below is only a simple example.

def tune():
    """Find the best gains using test(reference, gains)."""
    # Example: move P by step, turn back with a half step if worse
    reference = 10      # Target position [rev] (same for all runs here)
    p = 10              # First P gain
    step = 5            # First change of P gain
    best_score = test(reference, [p])

    for i in range(2):  # Number of tries
        new_score = test(reference, [p + step])

        if new_score < best_score:      # Better: move there and keep going
            p = p + step
            best_score = new_score
        else:                           # Worse: turn back with a smaller step
            step = -step / 2

    return [p], best_score


# ===== Run tuning and show the result =====
best_gains, best_score = tune()

print("===== Result =====")
print(f"Best gains  : {best_gains}")
print(f"Best score  : {best_score:.5f}")
print(f"Motor runs  : {len(csv_files)}", flush=True)   # flush: print now, before the graph

# Graph of all runs: run PLOT_GRAPH.py with the CSV files
html_name = time.strftime("TUNE_%Y%m%d_%H%M%S")     # Answer to "Enter the HTML file name"
subprocess.run(["python3", "PLOT_GRAPH.py"] + csv_files, input=html_name, text=True)
