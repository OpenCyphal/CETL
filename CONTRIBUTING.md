# Proposing a New Type

If you want to add to CETL you should start on the [forum](https://forum.opencyphal.org/c/app/cetl/22). Once we have
tacit approval the type should be developed using a [standard Github workflow](https://docs.github.com/en/get-started/quickstart/contributing-to-projects).

> Please don't write code on the forum. The forum post should be used to build consensus that a given type should be
developed and contributed to CETL before you go wasting your time writing the code. Once you have that level of support
it's time to move to Github.

For some types, however, we may want to stage introduction but make the type available to maintainers and
early-adopters. To facilitate git submodule access to CETL and to avoid noise we use feature branches upstream to
incubate such types. As such, we may request that your PR to `main` be redirected to a feature branch we setup in the
form of `preview/{your incubating feature name}`.

# Running CETLVaSt locally

```
./build-tools/bin/verify.py --online
```

need help?

```
./build-tools/bin/verify.py -h
```

# Developer Environment

## ![visual-studio code](.vscode/vscode-alt.svg#gh-dark-mode-only) ![visual-studio code](.vscode/vscode.svg#gh-light-mode-only)
We support the vscode IDE using
[cmake](https://github.com/microsoft/vscode-cmake-tools/blob/main/docs/README.md) and
[development containers](https://containers.dev/). Simply clone CETL, open the
repo in vscode and do "reopen in container" to automatically pull and relaunch
vscode in the provided devcontainer.

## Command-Line Workflow

If you don't want to use vscode you can pull our [Toolshed devcontainer](https://github.com/OpenCyphal/docker_toolchains/pkgs/container/toolshed)
and manually run it.

### TLDR
```
docker pull ghcr.io/opencyphal/toolshed:ts22.4.3
git clone {this repo}
cd CETL
docker run --rm -it -v ${PWD}:/repo ghcr.io/opencyphal/toolshed:ts22.4.3
./build-tools/bin/verify.py -vv --online configure
cd build
ninja release
```

### Step-by-Step

1. Pull the OpenCyphal dev-container used for CETL:
```
docker pull ghcr.io/opencyphal/toolshed:ts22.4.3
```
2. Clone CETL, cd into the repo, and launch an interactive terminal session of
the dev container. This command will mount the current directory (`${PWD}`) in
the container as `/repo` so any changes you make while in the container will
be made directly in your git repo. In fact, you can use your text editor of
choice on the host machine and just use the interactive console session for
building.
```
git clone {this repo}
cd CETL
docker run --rm -it -v ${PWD}:/repo ghcr.io/opencyphal/toolshed:ts22.4.x
```
3. run the verify script to configure cetlvast. By including `-vv` you can see
the exact cmake commands verify.py is executing and you can use `--dry-run`
if you really hate my python script so much that you want to do all the typing
yourself (It's not like I spent a ton of time documenting all of these options
for you. No no. It's fine. Don't try to apologize now...):
```
./build-tools/bin/verify.py -vv configure
```
4. Finally, you can cd into the top-level cmake directory you just configured
and run ninja directly.
```
cd build
ninja help
ninja release
```

### Format the sources

Clang-Format may format the sources differently depending on the version used.
To ensure that the formatting matches the expectations of the CI suite,
invoke Clang-Format of the correct version from the container (be sure to use the correct image tag):

```
docker run --rm -v ${PWD}:/repo ghcr.io/opencyphal/toolshed:ts22.4.3 ./build-tools/bin/verify.py build-danger-danger-cetlvast-clang-format-in-place
```

# Hashtag-based CI triggering

Normally, the CI will only run on pull requests, releases, and perhaps some other special occasions.
Often, however, you will want to run it on your branch before proposing the changes to ensure all checks are
green and test coverage is adequate.
To do that, add a hashtag with the name of the workflow you need to run to the head commit;
for example, making a commit with a message like `Add feature such and such #verification #docs #sonar`
will force the CI to execute jobs named `verification`, `docs`, and `sonar`.

Note that if the job you requested is dependent on other jobs that are not triggered, it will not run; 
for example, if `sonar` requires `docs`, pushing a commit with `#sonar` alone will not make it run.

# IDE-specific notes

## CLion

Ensure that the memory limit for clangd is set to at least 12 GiB;
a lower limit will cause the IDE to kill clangd frequently, causing the indexing context to be lost.
To change the limit, open the Registry (Shift+Shift -> type `Registry`) and adjust
`clion.clangd.max.memory`.
