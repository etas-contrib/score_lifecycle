def _impl_run_examples(ctx):
    run_script = ctx.file._run_script

    launcher = ctx.actions.declare_file(ctx.label.name + "_launcher.sh")
    ctx.actions.write(
        output = launcher,
        content = "#!/bin/bash\nexec {run_script} \"$@\"\n".format(
            run_script = run_script.short_path,
        ),
        is_executable = True,
    )

    runfiles = ctx.runfiles(files = [run_script] + ctx.files.deps)
    for dep in ctx.attr.deps:
        runfiles = runfiles.merge(dep[DefaultInfo].default_runfiles)

    return DefaultInfo(
        executable = launcher,
        runfiles = runfiles,
    )

run_examples = rule(
    implementation = _impl_run_examples,
    executable = True,
    attrs = {
        "deps": attr.label_list(
            default = [":example_apps"],
        ),
        "_run_script": attr.label(
            default = Label("//examples:run.sh"),
            allow_single_file = True,
        ),
    },
)