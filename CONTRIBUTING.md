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
cd cetlvast
cmake --list-presets=workflow
```

From this list, choose one and do:

```
cmake --workflow --preset one-of-the-presets-from-the-list
```

## Build Configurations

The following build configuration types are available for each configuration in the build (We use Ninja Multi-Config which generates build targets for all types in each cmake configuration).

| Type          | Description                                                                                           |
|---------------|-------------------------------------------------------------------------------------------------------|
| **Release**   | Highly Optimized, with exceptions enabled, and rtti.                                                  |
| **ReleaseEP** | (Embedded Profile) Optimized for embedded with exceptions and rtti disabled.                          |
| **Debug**     | Lightly optimized, with exceptions, rtti, and debug assert enabled.                                   |
| **DebugEP**   | Similar to Release EP (Embedded Profile) but with no optimizations and debug asserts enabled.         |
| **Coverage**  | Debug-like build only used to generate and report coverage data for CETLVaSt.                         |

See the CETLVaSt [CMakePresetsVendorTemplate.json](cetlvast/CMakePresetsVendorTemplate.json) for where these types are specified as well as all build parameters used to configure the verification build.

### CMakePresets.json and CMakePresetsVendorTemplate.json

We use [TCPM](https://pypi.org/project/tcpm/) to manage our extensive list of presets. This python tool will utilize configuration in the `CMakePresetsVendorTemplate.json` file to regenerate `CMakePresets.json` in place. The transformation is idempotent so it's safe to run the tool multiple times and any configuration manually added to `CMakePresets.json`, like our "manual-" presets, is preserved. If you change anything in `CMakePresetsVendorTemplate.json` simply install TCPM from pypi and do:

```bash
cd cetlvast
tcpm
```

then submit a PR for the updated files.

# Developer Environment

![visual-studio code](.vscode/vscode-alt.svg#gh-dark-mode-only) ![visual-studio code](.vscode/vscode.svg#gh-light-mode-only)

We support the vscode IDE using
[cmake](https://github.com/microsoft/vscode-cmake-tools/blob/main/docs/README.md) and
[development containers](https://containers.dev/). Simply clone CETL, open the
repo in vscode and do "reopen in container" to automatically pull and relaunch
vscode in the provided devcontainer.

## Command-Line Workflow

If you don't want to use vscode you can pull our [Toolshed devcontainer](https://github.com/OpenCyphal/docker_toolchains/pkgs/container/toolshed)
and manually run it.

### TLDR

See [devcontainer.json](.devcontainer/toolshed/devcontainer.json) for the current value of x for `ts24.4.x`.

```
docker pull ghcr.io/opencyphal/toolshed:ts24.4.x
git clone {this repo}
cd CETL
docker run --rm -it -v ${PWD}:/repo ghcr.io/opencyphal/toolshed:ts24.4.x
cd cetlvast
cmake --workflow --preset workflow-clang-native-cpp-14-online
```

### Step-by-Step

1. Pull the OpenCyphal dev-container used for CETL:
```
docker pull ghcr.io/opencyphal/toolshed:ts24.4.x
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
docker run --rm -it -v ${PWD}:/repo ghcr.io/opencyphal/toolshed:ts24.4.x
```
3. See available configurations
```
cd cetlvast
cmake --list-presets=configure
```
4. Select a configuration and run the configuration for it. Use an "online" variant the first time to pull external dependencies.
```
cmake --preset configure-gcc-native-cpp-14-online
```
5. List available build targets
```
cmake --list-presets=build | grep gcc-native-cpp-14-online
```
6. Build a configuration
```
cmake --build --preset build-Release-gcc-native-cpp-14-online
```

### Format the sources

Clang-Format may format the sources differently depending on the version used.
To ensure that the formatting matches the expectations of the CI suite,
invoke Clang-Format of the correct version from the container (be sure to use the correct image tag):

```
docker run --rm -v ${PWD}:/repo ghcr.io/opencyphal/toolshed:ts24.4.x /bin/sh -c 'cd cetlvast && cmake --preset configure-clang-native-cpp-17-offline && cd build && ninja danger-danger-cetlvast-clang-format-in-place'
```

# `issue/*` and hashtag-based CI triggering

Normally, the CI will only run on pull requests (PR), releases, and perhaps some other special occasions on `main` branch.
Often, however, you will want to run it on your branch before proposing the changes to ensure all checks are
green and test coverage is adequate - to do that:
- either target your PR to any `issue/NN_LABEL` branch, where `NN` is the issue number and `LABEL` is a small title giving context (like `issue/83_any`)
- or add a hashtag with the name of the workflow you need to run to the head commit;
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
The IDE needs at least 20 GiB of memory overall to open the project correctly.
