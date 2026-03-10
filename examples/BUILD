load("//:defs.bzl", "launch_manager_config")
load(":run_examples.bzl", "run_examples")

launch_manager_config(
    name = "example_config",
    config = "//examples/config:lifecycle_demo_config",
)

filegroup(
    name = "example_apps",
    srcs = [
        "//examples:example_config",
        "//examples/control_application:control_daemon",
        "//examples/control_application:lmcontrol",
        "//examples/cpp_lifecycle_app",
        "//examples/cpp_supervised_app",
        "//examples/rust_supervised_app",
    ],
)

filegroup(
    name = "lm_binaries",
    srcs = [
        "//src/control_client_lib",
        "//src/launch_manager_daemon:launch_manager",
        "//src/launch_manager_daemon/lifecycle_client_lib:lifecycle_client",
        "//src/launch_manager_daemon/process_state_client_lib:process_state_client",
    ],
)

run_examples(
    name = "run_examples",
    deps = [
        ":example_apps",
        ":lm_binaries",
    ],
)
