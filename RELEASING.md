# Releasing CETL

Merge the reviewed changes and version bump in `include/cetl/cetl.hpp` into `main`.
Choose patch versions for compatible fixes, minor versions for compatible additions,
and major versions for incompatible changes.

```sh
python3 scripts/release.py
```

The script fetches `main`, reads the header version, verifies CI and the release
bundle, then creates the tag, publishes the release, and deploys documentation.
Failed verification leaves the version available for a corrected commit.

The normal command requires a version newer than every published stable release.
If interrupted, use the printed recovery command, which pins the version and commit.
Recovery reuses matching tags, drafts, and uploaded assets. Conflicting tags or asset
contents are refused.
