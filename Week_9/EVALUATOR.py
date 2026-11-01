# EVALUATOR
# Very simple score of a CSV file from AUTO_CONTROL (smaller is better)
#   score = average of |target - position| over the whole run [rev]
# Change only evaluate() to make your own score (like positionControl() in C).
# Run: python3 EVALUATOR.py a.csv

import sys
import numpy as np


def read_csv(csv_file):
    """Read a CSV file and return time, position, target."""
    data = np.loadtxt(csv_file, delimiter=",", skiprows=1)     # skiprows=1: header row
    t = data[:, 0]              # Column 0: time (s)
    position = data[:, 1]       # Column 1: position (rev)
    target = data[:, 2]         # Column 2: target (rev)
    return t, position, target


def evaluate(t, position, target):
    """Score of one run (smaller is better)."""
    error = np.abs(target - position)   # Error at each time
    return np.mean(error)               # Average error


def get_score(csv_file):
    """Read a CSV file and return its score (used by AUTO_TUNE.py)."""
    t, position, target = read_csv(csv_file)
    return evaluate(t, position, target)


# Only when this file is run directly (not when imported by AUTO_TUNE.py)
if __name__ == "__main__":
    score = get_score(sys.argv[1])
    print(f"Score: {score:.5f}")
