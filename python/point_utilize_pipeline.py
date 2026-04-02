# write parameters.xml file, gather output of simulator and save FLOPS into file for each hosts' power distribution
import numpy as np
from pathlib import Path
from scenarious.point_run import point_run
from analyzers.merge_timeline import merge_timeline

experiment_dir = Path(__file__).parent.parent

proj_name_1 = "RakeSearchtype1e15@home"
proj_name_2 = "RakeSearchtype2e13@home"

LOG_DIR = "exp/test/point_runs/utilize_tester"

REPEATING_PER_CONFIG = 1

FLOPS = np.arange(10, 200, 250, dtype=int)

PROP = np.arange(6_000_000_000_000, 6_000_000_000_100, 50_000_000_000, dtype=int)

MIN_QUORUM = 1
SUCCESS_PERCENTAGE = 98
CANONICAL_PERCENTAGE = 84

key_to_look_at = ['Messages received',
                  'Work requests received',
                  'Results created',
                  'Results sent',
                  'Results cancelled',
                  'Results received',
                  'Results analyzed',
                  'Results success',
                  'Results failed',
                  'Results too late',
                  'Results valid',
                  'Workunits total',
                  'Workunits completed',
                  'Workunits not completed',
                  'Workunits valid',
                  'Workunits error',
                  'Throughput',]

print(f"LOG_DIR:{LOG_DIR}")

point_run(experiment_dir, LOG_DIR, FLOPS, PROP)
merge_timeline(experiment_dir/LOG_DIR, FLOPS, PROP, output_file_name="merged_10x.csv")

print("----------FINISH----------")
