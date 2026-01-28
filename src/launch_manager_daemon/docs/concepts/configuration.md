# Launch Manager Daemon Configuration Concept

## Version

The version of the configuration schema should be included in every configuration file.
When loading the configuration file at runtime, the launch_manager will check that it matches the supported schema version.

## Usage of Defaults

We should have a mechanism to define default values to avoid repetition of common properties.

### Proposal for first version: Default component and default sandbox definition

In the `defaults` section of file, the user can define default values for component and sandbox properties.
All keys have the identical names as used for the configuration of a concrete component / sandbox.

```json
"defaults_": {
    "sandbox": {
        "startup_timeout": 0.5,
        "shutdown_timeout": 0.5,
        "uid" : 1000,
        "gid" : 1000,
        "supplementary_group_ids": [500, 600, 700],
        "environmental_variables": {
            "LD_LIBRARY_PATH": "/opt/lib",
            "EMPTY_ENV_VAR": ""
        },
        "process_arguments": [],
        "scheduling_policy": "SCHED_OTHER",
        "scheduling_priority": "0"
        ...
    },
    "component": {
        "is_native_application": false,
        "is_supervised": true,
        "is_self_terminating": false,
        "is_state_manager": false,
        "depends_on": []
    },
    "run_target": {
        "transition_timeout": 2
    }
}
```

Any setting not defined as part of a component or sandbox definition will automatically use the default value.
Overwriting Behavior:
* Primitive values (like `uid`) simply get overwritten if configured
* Dictionaries (like `environmental_variables`) get merged. Keys can be overwritten by configuring the same keys.
* Lists (like `supplementary_group_ids`) get overwritten. In the future we may add further properties like `supplementary_group_ids_extend` to add to the default list instead of overwriting - if this use case gets necessary.

### Proposal for future extension: Defining different component and sandbox templates

As per our discussion there may be a use case to have different kinds of components and different kinds of sandboxes with their own defaults.
The approach above can be extended in this direction, by defining multiple sandboxes and multiple components in the "defaults" section.
Component and sandbox definition could then refer to one of the predefined alternatives.

## Dependencies between components

Each dependency is configured as a json object, as we foresee that further properties might be added to the dependency in the future (i.e. support for kinds of dependencies).

Initially (when mapping the new configuration to the existing software), the component states are identical to the process state:
* `Running`: The process finished initialization (or is a self-terminating processes)
* `Terminated`: The process terminated

```json
"components": {
    "test_app2": {
        "depends_on": {
            "test_app4": {
                "required_state": "Running",
            }
        }
    }
}
```

## Assignment to RunTargets

For every dependency, the required state of the component is implicitly "Running".

```json
"run_targets": {
    "run_target_01": {
        "description": "Example description of a Run Target number 01.",
        "includes": ["test_app1"]
    }
    ...
}
```

## Timeout Values

All timeout values will be in seconds.

## Configuration Example

![Example Graph](../images/example_graph.png)

```json
{
    "schema_version": 1,
    "defaults": {
        "sandbox": {
            "startup_timeout": 0.5,
            "shutdown_timeout": 0.5,
            "uid" : 1000,
            "gid" : 1000,
            "supplementary_group_ids": [500, 600, 700],
            "security_policy": "",
            "environmental_variables": {
                "LD_LIBRARY_PATH": "/opt/lib",
                "GLOBAL_ENV_VAR": "abc",
                "EMPTY_GLOBAL_ENV_VAR": ""
            },
            "process_arguments": [],
            "scheduling_policy": "SCHED_OTHER",
            "scheduling_priority": "0",
            "working_directory": "/tmp",
            "resource_limits": {}
        },
        "component": {
            "is_native_application": false,
            "is_supervised": true,
            "is_self_terminating": false,
            "is_state_manager": false,
            "depends_on": []
        }
    },
    "components": {
        "setup_filesystem_sh": {
            "is_native_application": true,
            "is_supervised": false,
            "is_self_terminating": true,
            "sandbox": {
                "executable_path" : "/bin/sh",
                "process_arguments": ["-c", "/opt/scripts/setup_filesystem.sh"],
            }
        },
        "dlt-daemon": {
            "is_native_application": true,
            "is_supervised": false,
            "depends_on": {
                "setup_filesystem_sh": {
                    "required_state": "Terminated"
                }
            },
            "sandbox": {
                "executable_path" : "/opt/apps/dlt-daemon/dltd",
            }
        },
        "someip-daemon": {
            "sandbox": {
                "executable_path" : "/opt/apps/someip/someipd",
            }
        },
        "test_app1": {
            "depends_on": {
                "dlt-daemon": {
                    "required_state": "Running"
                },
                "someip-daemon": {
                    "required_state": "Running"
                }
            },
            "sandbox": {
                "executable_path" : "/opt/apps/test_app1/test_app1",
            }
        },
        "state_manager": {
            "is_state_manager": true,
            "depends_on": {
                "setup_filesystem_sh": {
                    "required_state": "Terminated"
                }
            },
            "sandbox": {
                "executable_path" : "/opt/apps/state_manager/sm",
            }
        }
    },
    "run_targets": {
        "Minimal": {
            "description": "Minimal functionality of the system",
            "includes": ["state_manager"]
        },
        "Full": {
            "description": "Everything running",
            "includes": ["Minimal", "test_app1"]
        },
        "Off": {
            "description": "Nothing is running"
        }
    },
    "initial_run_target": "Minimal",
    "health_monitoring" : {
        "evaluation_cycle_ms": 500
    },
    "watchdogs": {
        "simple_watchdog": {
            "device_file_path": "/dev/watchdog",
            "max_timeout_ms": 2000,
            "deactivate_on_shutdown": true,
            "require_magic_close": false
        }
    }
}
```

## Open Topics

### RecoveryAction

In the initial version (mapping to the config to the existing software) the only supported RecoveryAction will be to switch to a predefined recovery RunTarget in case of any error.

How to define the SwitchRunTarget RecoveryAction without entangling the component definition with the RunTarget definition?

### Execution dependencies: Component State vs. Process State

For the initial version, it will be easiest to have component state identical to process state as this is already supported by the existing code.

### How to configure the initial RunTarget

Should we reserve a name for the initial RunTarget or make this configurable (see `initial_run_target`)?

### Defining json schema

After agreement on the rough structure, we will define a json schema depicting all possible configuration options.