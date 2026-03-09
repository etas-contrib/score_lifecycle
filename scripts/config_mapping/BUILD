
load("@score_lifecycle_pip//:requirements.bzl", "requirement")
load("@score_tooling//:defs.bzl", "score_py_pytest")

py_binary(
    name = "lifecycle_config",
    srcs = ["lifecycle_config.py"],
    deps = [requirement("jsonschema")],
    visibility = ["//visibility:public"],
)

filegroup(
  name = "integration_test_files",
  srcs = glob(["tests/**/*/*.json"]),
)

score_py_pytest(
    name = "lifecycle_config_tests",
    srcs = [
        "unit_tests.py",
        "integration_tests.py",
    ],
    args = [
    ],
    data = [
        ":lifecycle_config.py",
        "//src/launch_manager_daemon/config/config_schema:s-core_launch_manager.schema.json",
        ":integration_test_files"
    ],
    tags = ["manual"],
    deps = [requirement("pytest"), requirement("jsonschema")],

)
