# Releasing CETL

Releases are cut from a reviewed commit merged into `main`, using
`python3 scripts/release.py`. The script runs on a maintainer's machine using their
Git and GitHub credentials. It does not bypass review or bump versions automatically.

## Version and tag contract

`CETL_VERSION_MAJOR`, `CETL_VERSION_MINOR`, and `CETL_VERSION_PATCH` in
`include/cetl/cetl.hpp` are the authoritative numeric version. Each must occur
exactly once as a decimal integer without leading zeros. CETLVaSt derives its
default CMake version from these macros. Release documentation uses the validated
tag version, including any prerelease suffix.

- Stable releases use `vMAJOR.MINOR.PATCH`, for example `v2.0.0`.
- Prereleases use `vMAJOR.MINOR.PATCH-alpha.N`, `-beta.N`, or `-rc.N`, with `N >= 1`.
  Their numeric core must match the header. GitHub's prerelease flag must match the
  presence of the suffix. Prereleases never replace the stable documentation site.
- Patch releases fix library code without breaking compatibility; minor releases
  add compatible functionality; incompatible public changes require a major
  release. See the public-name compatibility rules in `CONTRIBUTING.md`.
- Documentation/test-only changes do not require a library version bump or a
  new library release. Ordinary development commits need no matching tag.
- A release tag and its published assets are immutable. Fix a defective release
  with a new version, rather than moving the tag or replacing its assets.
- New release versions must advance beyond every published stable version.
  Only the latest published stable version may replace the Pages site; resuming
  an older release retains the newer documentation.
- Releases currently come from `main` only. A maintenance branch policy must be
  reviewed and implemented before releasing from other branches.

Prepare the version bump and relevant code/documentation changes in a PR, describe
compatibility changes, and merge through normal review. Do not tag an unmerged
branch. No version is bumped just by installing the release tooling.

## Maintainer prerequisites

Use Python 3.10 or newer, Git, and a current [GitHub CLI](https://cli.github.com/).
The Python scripts use only the standard library. Run from a clean checkout,
including untracked files, with `origin` pointing to `OpenCyphal/CETL` over GitHub
SSH or HTTPS. Both fetch and push URLs must identify the same repository and must
not embed credentials. Git's authentication must permit pushing a new tag.

Authenticate `gh` using `gh auth login --hostname github.com`, or supply `GH_TOKEN`
through the environment. Fine-grained tokens need repository **Contents: read/write**
and **Actions: read/write** (including workflow dispatch); your account also needs
repository write access. Never pass tokens as command arguments or put them in
the repository. SSO authorization and repository rulesets may impose additional
requirements. The script checks repository access before pushing a tag; individual
API operations also enforce the token's permissions and fail closed on errors.

The release workflow must already be on `main` and in the target commit. Existing
historical tags without this workflow cannot satisfy the new release contract.

Repository administrators should configure:

1. Required PR checks including `version`, `release-tests`, `release-ready`, and `release-assets`,
   with reviews required before merging into `main`.
2. A tag ruleset matching `v*` that prevents tag updates and deletions. Restrict
   tag creation to the maintainers responsible for releases.
3. GitHub release immutability, where available, to protect published tags/assets.
4. The `github-pages` environment permitting deployments from `main`, with any
   required reviewers retained. A tag deployment exception is unnecessary.
5. Release publishing permissions limited to release maintainers, who use this
   script. The workflow itself has read-only repository contents permission.

GitHub's `release: published` event runs **after** publication. Its validation is a
backstop for manual releases, not a way to prevent a privileged user from publishing
through the UI or API. The supported publication gate is the script plus the
repository permissions/rules above. The scripts do not modify repository settings.

## Cut a release

First run the tooling tests and select the exact reviewed commit:

```sh
python3 -m unittest discover -s scripts/tests -v
git fetch origin
git rev-parse origin/main
python3 scripts/release.py --version 2.0.0 --ref <full-commit-sha> --dry-run
```

Substitute the version recorded in that commit's header. Use `--notes-file PATH`
to supply reviewed release notes, or let GitHub generate them from merged PRs.
The dry run authenticates,
checks remote refs and eligibility, and reports the tag/commit and planned actions.
It can refresh local Git refs but makes no remote writes. Then cut the release:

```sh
python3 scripts/release.py --version 2.0.0 --ref <full-commit-sha>
```

For a prerelease use, for example, `--version 2.0.0-rc.1` with header version
`2.0.0`. `--repo OWNER/NAME --remote NAME --root PATH` support a fork rehearsal;
the repository must also use `main` as its default branch. The same GitHub URL,
permission, and branch checks apply. The default wait is two hours per CI/deployment
phase; `--timeout SECONDS` and `--poll SECONDS` adjust this.

The script:

1. Checks the clean checkout, repository access, up-to-date remote `main`, commit
   ancestry, version, and existing local/remote tag and release state.
2. Pushes the exact commit as a new lightweight version tag without force. This
   starts CETLVaSt. A matching existing attempt requires `--resume`.
3. Waits for a successful tag push/manual-dispatch run of `cetlvast.yml` for that
   exact SHA. It checks every required job in the current run attempt, including
   all 15 compiler/standard matrix entries, documentation, and release packaging.
   Missing, skipped, failed, cancelled, or incomplete checks cannot authorize a release.
4. Downloads that run's `release-bundle` artifact, validates its manifest and
   checksums, and checks that the remote tag still points to the expected commit.
5. Creates a draft with supplied/generated release notes, the source SHA, and a verification
   link. Uploads the verified files and checks all remote asset digests before
   publishing. A retry may add missing assets, but cannot replace existing bytes.
6. Publishes the draft. Stable releases then dispatch full verification on `main`
   with `docs_ref=<tag>` and wait for the protected Pages deployment. This run
   checks out the release commit for all builds. Prerelease docs remain downloadable
   assets. The script prints the tag, commit, verification, release, and docs status.

PR/full branch runs also rehearse packaging with an ephemeral local prerelease tag
and compile C++14/C++20 consumers against only the extracted header archive. This
tag is never pushed, and these runs cannot authorize publication.

CI validates the header on every push and PR. Version-tag pushes, release events,
and dispatches run the complete verification without requiring hashtags.
The `release-ready` aggregate requires all dependencies to succeed; packaging and
Pages deployment depend on it. `docs_ref` accepts a matching release tag, rather
than arbitrary branches or commits, and does not skip verification. CI checks the
tagged checkout even if `main` advances during the release.

## Release contents and verification

The `release-bundle` CI artifact contains five files:

| File | Contents |
| --- | --- |
| `cetl-VERSION-source.tar.gz` | Full tracked source from the release commit, under `cetl-VERSION/`; no checkout caches or build products. |
| `cetl-VERSION-headers.tar.gz` | `include/` and the root `LICENSE`, with the same prefix; no test/build dependencies. |
| `cetl-VERSION-docs.tar.gz` | Built HTML under `cetl-VERSION/html/` and the root license. |
| `release.json` | Version, tag, prerelease flag, and full source commit SHA. |
| `SHA256SUMS` | SHA-256 checksums of the four files above. |

The archives use normalized gzip timestamps. Source/header tar metadata comes
from Git; generated documentation is the output of the recorded CI run. GitHub's
automatically generated source downloads are separate from these named assets.

After publication, download the assets and run `sha256sum -c SHA256SUMS` (or the
equivalent platform tool). Confirm `release.json` names the tag's commit, that
the release's verification link shows all required checks passing, and that
the stable site displays the release version. Release notes should identify API
changes, fixes, and compatibility impacts; review the generated draft text as
part of preparing the release/version PR, and supply `--notes-file` when additional
narrative is needed. A retry retains the existing draft's notes.

## Interrupted and concurrent attempts

Leave an interrupted attempt intact, inspect the reported GitHub run/release,
and retry the same version and SHA with `--resume`. Never delete/recreate a
published tag to make a retry pass.

- A matching tag with no run can be recovered by the script's tag dispatch.
- For failed CI, fix the cause. For a transient failure, rerun the **entire** run
  (`gh run rerun RUN_ID --repo OpenCyphal/CETL`), then resume. Rerunning only failed
  jobs does not provide every job in the current attempt and is insufficient for
  the release gate. A source fix needs a new reviewed commit and a new tag/version.
- A script-managed draft is identified by its tag/commit marker. The script refuses
  unrelated drafts, conflicting tags, unexpected assets, or differing asset bytes.
  Matching assets are retained and only missing uploads are added. Concurrent
  attempts may fail on an already-created draft/asset; inspect and resume rather
  than overwriting it.
- If publication succeeded but Pages failed or timed out, resume the same command.
  It verifies the release and retries the docs deployment without republishing.
  CI bundles are retained for 30 days. If the bundle has expired, deploy the
  already-published stable tag independently through the same complete gate:

  ```sh
  gh workflow run cetlvast.yml --repo OpenCyphal/CETL --ref main -f docs_ref=v2.0.0
  ```

  Monitor that run and its `deploy-docs` job. No release assets need editing.
- Permission/network failures stop the script. Confirm the remote state before
  resuming: a failed response does not prove the preceding write did not happen.

The automated rehearsal uses disposable local checkouts and bare Git remotes,
with a simulated GitHub service. It exercises real tag pushes, archive contents,
publication ordering, dry runs, failed gates, interrupted uploads, and deployment
recovery without contacting GitHub or publishing a production release. Run the
same CLI against a disposable GitHub fork for an end-to-end credentials/Actions/
Pages rehearsal before the first production release; its environment/rules must
be configured as above. The local simulation does not substitute for those hosted
service settings.

## Additional distribution channels

Keep direct header use, Git submodules, and the header archive supported. Packaging
must not install CETLVaSt or require its build/test dependencies in consuming
projects. Package versions must equal the stable tag's numeric version; pin the
source commit and archive checksum, install the license, and compile a small
consumer using the installed include tree under C++14 and a newer standard.

| Channel | Assessment and decision |
| --- | --- |
| Conan 2 / ConanCenter | A [header-only recipe](https://docs.conan.io/2/tutorial/creating_packages/other_types_of_packages/header_only_packages.html) plus `test_package` fits CETL. Package the verified header archive with its checksum, preserve `include/cetl`, install licenses, and expose consumer metadata without building CETLVaSt. Revisit #9, which was closed as not planned, before committing to registry maintenance. Defer registry submission until a maintainer owns recipe updates and ConanCenter review. |
| vcpkg | Prototype an [overlay port](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started-packaging), installing the verified headers/license, then consider the curated registry under its [maintainer policy](https://learn.microsoft.com/en-us/vcpkg/contributing/maintainer-guide). Pin each tag/source and SHA-512 as required by the port's download helper; test installed consumers on Windows and Linux. Defer submission until a port owner and update process are agreed. |
| CMake package / FetchContent | The lowest external maintenance cost: a future minimal interface target and install/export config can serve both package managers and direct consumers, without configuring CETLVaSt. Recommend this as the first packaging follow-up. Until implemented, consumers can fetch a pinned archive and use its `include/` directory; no `find_package(CETL)` target is promised by this change. |

The core release process ships independently of registry adoption. Future packaging
PRs should name the owner, add installed-consumer smoke tests, and specify whether
the release script opens recipe/port update PRs or a maintainer does so manually.
Registry review/acceptance is separate from successful CETL release publication.
