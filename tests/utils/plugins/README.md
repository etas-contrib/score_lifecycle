# LocalTarget

An ITF plugin to allow for testing locally.
This implements the `itf.core.target.Target` & `itf.core.process.AsyncProcess`.

This will deploy the files into the directory `/opt/score` on your host machine.
So before running please create those directories with permissions so that your
user can write to those directories.
