# Releasing CETL

Merge the reviewed changes and version bump into `main`. Choose patch versions for
compatible fixes, minor versions for compatible additions, and major versions for
incompatible changes. Documentation/test-only changes do not require a version bump.

```sh
python3 scripts/release.py --dry-run
```

Run the printed **Release this checked commit** command to publish exactly the
version and commit that passed preflight. The script fetches remote `main`, reads
the header version, checks the tools/access/workflow/Pages setup, waits for full CI,
verifies the archives, publishes the release, and verifies the deployed documentation.
Failures report the corrective action. No manual test, checksum, or site-version
check is required.

For a prerelease, supply its version explicitly:

```sh
python3 scripts/release.py --version 2.0.0-rc.1 --dry-run
```

Use `--ref COMMIT` to select a particular merged commit and `--notes-file PATH` for
reviewed release notes; otherwise GitHub generates the notes. See `--help` for
forks, alternate remotes, and wait limits.

## Recovery

Run the printed **Resume command** after fixing the reported problem. It retains
matching tags/assets, finishes missing uploads, and retries documentation deployment.
Published releases are verified from their actual assets, so expired CI artifacts
do not require manual recovery. Conflicting tags or asset contents are refused.

For a transient CI failure, run the full-run retry command printed in the error,
then resume. A source fix needs a new reviewed commit and a new version/tag.
