load("@pip_score_venv_test//:requirements.bzl", "requirement")
load("@rules_python//python:defs.bzl", "py_library")

py_library(
    name = "fixtures",
    srcs = [
        "control_interface/control_interface.py",
        "control_interface/linux_interface.py",
        "control_interface/ssh.py",
        "control_interface/fixture.py",
        "file_interface/file_interface.py",
        "file_interface/fixture.py",
        "file_interface/sftp_interface.py",
        "target/fixture.py",
        "utils/setup_test.py",
        "utils/check_for_failures.py",
        "__init__.py",
    ],
    imports = ["../../.."],
    visibility = ["//tests:__subpackages__"],
    deps = [
        requirement("pytest"),
        "@score_itf//itf/core/qemu",
        "@score_itf//itf/core/com:ssh",
        "@score_itf//itf/core/utils/process",
        ":conf"
    ],
    tags = [
        "local",
    ]
)


py_library(
    name="conf",
    srcs = [ "conftest.py" ],
    visibility = ["//tests:__subpackages__"],
)
