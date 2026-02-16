#!/usr/bin/env python3

import subprocess
import shutil
from pathlib import Path
import filecmp
from lifecycle_config import preprocess_defaults
import json

script_dir = Path(__file__).parent
tests_dir = script_dir / "tests"
lifecycle_script = script_dir / "lifecycle_config.py"

def run(input_file : Path, test_name : str):
    """
    Execute the mapping script with the given input file and compare the generated output with the expected output.
    Input:
    - input_file: The path to the input JSON file for the mapping script
    - test_name: The name of the test case, which corresponds to a subdirectory in the "tests" directory containing the expected output
    """
    actual_output_dir = tests_dir / test_name / "actual_output"
    expected_output_dir = tests_dir / test_name / "expected_output"

    # Clean and create actual output directory
    if actual_output_dir.exists():
        shutil.rmtree(actual_output_dir)
    actual_output_dir.mkdir(parents=True)

    # Execute lifecycle_config.py
    cmd = [
        "python3", 
        str(lifecycle_script),
        str(input_file),
        "-o", str(actual_output_dir)
    ]

    try:
        result = subprocess.run(cmd, check=True, capture_output=True, text=True)
        print(f"Command executed successfully: {' '.join(cmd)}")
        print(f"Output: {result.stdout}")
    except subprocess.CalledProcessError as e:
        print(f"Command failed: {' '.join(cmd)}")
        print(f"Error: {e.stderr}")
        raise

    if not compare_directories(actual_output_dir, expected_output_dir):
        raise AssertionError("Actual output does not match expected output.")

def compare_directories(dir1: Path, dir2: Path) -> bool:
    """
    Compare two directories recursively. Return True if they are the same, False otherwise.
    """
    dcmp = filecmp.dircmp(dir1, dir2)
    
    if dcmp.left_only or dcmp.right_only or dcmp.diff_files:
        print(f"Directories differ: {dir1} vs {dir2}")
        print(f"Only in {dir1}: {dcmp.left_only}")
        print(f"Only in {dir2}: {dcmp.right_only}")
        print(f"Different files: {dcmp.diff_files}")
        return False
    
    for common_dir in dcmp.common_dirs:
        if not compare_directories(dir1 / common_dir, dir2 / common_dir):
            return False
            
    return True

def test_basic():
    test_name = "basic_test"
    input_file = tests_dir / test_name / "input" / "lm_config.json"

    run(input_file, test_name="basic_test")


def test_preprocessing_basic():
    """
    Basic smoketest for the preprocess_defaults function, to ensure that defaults are being applied and overridden correctly.
    """

    global_defaults = json.loads('''
    {
        "deployment_config": {
            "ready_timeout": 0.5,
            "shutdown_timeout": 0.5,
            "environmental_variables" : {
                "DEFAULT1": "default_value1",
                "DEFAULT2": "default_value2"
            },
            "sandbox": {
                "uid": 0,
                "supplementary_group_ids": [100]
            }
        },
        "component_properties": {
            "application_profile": {
                "application_type": "REPORTING",
                "is_self_terminating": false
            }
        },
        "alive_supervision": {
            "evaluation_cycle": 0.5
        },
        "watchdogs": {}
    }''')

    config = json.loads('''{
        "defaults": {
            "deployment_config": {
                "shutdown_timeout": 1.0,
                "environmental_variables" : {
                    "DEFAULT2": "overridden_value2",
                    "DEFAULT3": "default_value3",
                    "DEFAULT4": "default_value4"
                },
                "recovery_action": {
                    "restart": {
                        "number_of_attempts": 1,
                        "delay_before_restart": 0.5
                    }
                }
            },
            "component_properties": {

            }
        },
        "components": {
            "test_comp": {
                "description": "Test component",
                "component_properties": {

                },
                "deployment_config": {
                    "environmental_variables": {
                        "DEFAULT3": "overridden_value3"
                    },
                    "sandbox": {
                        "uid": 0,
                        "gid": 1,
                        "supplementary_group_ids": [101]
                    },
                    "recovery_action": {
                        "switch_run_target": {
                            "run_target": "Off"
                        }
                    }
                }
            }
        },
        "run_targets": {},
        "alive_supervision": {
            "evaluation_cycle": 0.1
        },
        "watchdogs": {
            "simple_watchdog": {
                "device_file_path": "/dev/watchdog",
                "max_timeout": 2,
                "deactivate_on_shutdown": true,
                "require_magic_close": false
            }
        }
    }''')

    preprocessed_config = preprocess_defaults(global_defaults, config)

    expected_config=json.loads('''{
        "components": {
            "test_comp": {
                "description": "Test component",
                "component_properties": {
                    "application_profile": {
                        "application_type": "REPORTING",
                        "is_self_terminating": false
                    }
                },
                "deployment_config": {
                    "ready_timeout": 0.5,
                    "shutdown_timeout": 1.0,
                    "environmental_variables" : {
                        "DEFAULT1": "default_value1",
                        "DEFAULT2": "overridden_value2",
                        "DEFAULT3": "overridden_value3",
                        "DEFAULT4": "default_value4"
                    },
                    "sandbox": {
                        "uid": 0,
                        "gid":1,
                        "supplementary_group_ids": [101]
                    },
                    "recovery_action": {
                        "switch_run_target": {
                            "run_target": "Off"
                        }
                    }
                }
            }
        },
        "run_targets": {},
        "alive_supervision": {
            "evaluation_cycle": 0.1
        },
        "watchdogs": {
            "simple_watchdog": {
                "device_file_path": "/dev/watchdog",
                "max_timeout": 2,
                "deactivate_on_shutdown": true,
                "require_magic_close": false
            }
        }
    }''')

    print("Dumping preprocessed configuration:")
    print(json.dumps(preprocessed_config, indent=4))

    assert preprocessed_config == expected_config, "Preprocessed config does not match expected config."
