#!/usr/bin/env python3

import argparse
import json
from typing import Dict, Any

score_defaults = {
    "deployment_config": {
        "startup_timeout": 0.5,
        "shutdown_timeout": 0.5,
        "uid": 1000,
        "gid" : 1000,
        "supplementary_group_ids": [],
        "security_policy": "",
        "environmental_variables": {},
        "process_arguments": [],
        "scheduling_policy": "SCHED_OTHER",
        "scheduling_priority": 0,
        "bin_dir": "/opt",
        "working_directory": "/tmp",
        "restarts_during_startup": 0,
        "resource_limits": {}
    },
    "component_properties": {
    },
    "run_target": {
    }
}


def load_json_file(file_path: str) -> Dict[str, Any]:
    """Load and parse a JSON file."""
    with open(file_path, 'r') as file:
        return json.load(file)


def preprocess_defaults(config):
    """
    This function takes the input configuration and fills in any missing fields with default values.
    The resulting file with have no "defaults" entry anymore, but looks like if the user had specified all the fields explicitly.
    """
    # TODO
    return config

def gen_health_monitor_config(output_dir, config):
    """
    This function generates the health monitor configuration file based on the input configuration.
    Input:
        output_dir: The directory where the generated files should be saved
        config: The preprocessed configuration in the new format, with all defaults applied 

    Output:
    - A file named "hm_demo.json" containing the health monitor daemon configuration
    - A optional file named "hmcore.json" containing the watchdog configuration
    - For each supervised process, a file named "<process>_<process_group>.json"
    """
    with open(f"{output_dir}/hm_demo.json", "w") as hm_file:
        hm_config = {}
        json.dump(hm_config, hm_file, indent=4)

def gen_launch_manager_config(output_dir, config):
    """
    This function generates the launch manager configuration file based on the input configuration.
    Input:
        output_dir: The directory where the generated files should be saved
        config: The preprocessed configuration in the new format, with all defaults applied 

    Output:
    - A file named "lm_demo.json" containing the launch manager configuration
    """
    
    with open(f"{output_dir}/lm_demo.json", "w") as lm_file:
        lm_config = {}
        json.dump(lm_config, lm_file, indent=4)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("filename", help="The launch_manager configuration file")
    parser.add_argument("--output-dir", "-o", default=".", help="Output directory for generated files")
    args = parser.parse_args()

    input_config = load_json_file(args.filename)
    preprocessed_config = preprocess_defaults(input_config)
    gen_health_monitor_config(args.output_dir, preprocessed_config)
    gen_launch_manager_config(args.output_dir, preprocessed_config)
    
    return 0

if __name__ == "__main__":
    exit(main())