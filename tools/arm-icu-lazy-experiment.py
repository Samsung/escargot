#!/usr/bin/env python3

import importlib.util
from pathlib import Path


if __name__ == '__main__':
    spec = importlib.util.spec_from_file_location(
        'arm_engine_experiment', Path(__file__).with_name('arm-v8-lite-experiment.py'))
    experiment = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(experiment)
    experiment.main(mode='both', engine_names=('escargot',))
