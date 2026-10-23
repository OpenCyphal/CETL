#!/usr/bin/env python3
"""Cut a CETL release using Git and the maintainer's authenticated GitHub CLI."""

import argparse
import json
from pathlib import Path
import re
import tempfile
import time
from urllib.parse import urlencode

from release_tools import (Git, ReleaseError, WORKFLOW, check_bundle, check_run, command,
                           parse_version, sha256)


class GitHub:
    def __init__(self, repository):
        self.repository = repository

    def cli(self, *args):
        return command(["gh", *args, "--repo", "github.com/" + self.repository])

    def api(self, path, method="GET", **fields):
        args = ["gh", "api", "--hostname", "github.com", "--method", method,
                f"repos/{self.repository}/{path}".rstrip("/")]
        for key, value in fields.items():
            args.extend(["-F" if isinstance(value, bool) else "-f",
                         f"{key}={str(value).lower() if isinstance(value, bool) else value}"])
        result = command(args)
        return json.loads(result) if result else None

    def pages(self, path, key=None):
        items = []
        for page in range(1, 1001):
            separator = "&" if "?" in path else "?"
            result = self.api(f"{path}{separator}per_page=100&page={page}")
            batch = result[key] if key else result
            items.extend(batch)
            if len(batch) < 100:
                return items
        raise ReleaseError("GitHub pagination limit exceeded.")

    def release(self, tag):
        # List rather than treating every failed tag lookup (403, outage, etc.) as 'not found'.
        return next((release for release in self.pages("releases") if release["tag_name"] == tag), None)

    def published_stable_versions(self):
        versions = set()
        for release in self.pages("releases"):
            if not release["draft"] and not release["prerelease"]:
                match = re.fullmatch(r"v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", release["tag_name"])
                if match:
                    versions.add(tuple(map(int, match.groups())))
        return versions

    def runs(self, **filters):
        return self.pages(f"actions/workflows/{WORKFLOW}/runs?{urlencode(filters)}", "workflow_runs")

    def jobs(self, run):
        # Only the current attempt counts; successful jobs from an older attempt cannot fill gaps.
        return self.pages(f"actions/runs/{run['id']}/attempts/{run['run_attempt']}/jobs", "jobs")

    def assets(self, release):
        return self.pages(f"releases/{release['id']}/assets")

    def create_draft(self, tag, commit, prerelease, marker, run, notes=None):
        if notes is None:
            notes = self.api("releases/generate-notes", "POST", tag_name=tag, target_commitish=commit)["body"]
        body = f"{notes}\n\nSource commit: `{commit}`\n\nVerification: {run['html_url']}\n\n{marker}\n"
        self.api("releases", "POST", tag_name=tag, target_commitish=commit, name=tag,
                 draft=True, prerelease=prerelease, body=body)

    def asset_matches(self, asset, path):
        if asset.get("state") != "uploaded" or asset.get("size") != path.stat().st_size:
            return False
        if asset.get("digest"):
            return asset["digest"] == "sha256:" + sha256(path)
        # Older GitHub assets may not have a server-side digest. Compare the actual bytes.
        with tempfile.TemporaryDirectory(prefix="cetl-asset-") as directory:
            self.cli("release", "download", asset["release_tag"], "--pattern", path.name, "--dir", directory)
            return sha256(Path(directory) / path.name) == sha256(path)


def wait_for_release_run(github, commit, tag, timeout, poll):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        candidates = [run for run in github.runs(head_sha=commit)
                      if run["head_branch"] == tag and run["event"] in ("push", "workflow_dispatch")]
        if candidates:
            run = max(candidates, key=lambda item: item["id"])
            if run["status"] == "completed":
                check_run(run, github.jobs(run), commit, tag)
                print(f"Verification: {run['html_url']}")
                return run
        time.sleep(poll)
    raise ReleaseError("Timed out waiting for release CI; leave the tag/draft intact and retry with --resume.")


def check_draft(release, tag, prerelease, marker):
    if (release.get("tag_name") != tag or release.get("prerelease") != prerelease
            or marker not in (release.get("body") or "")):
        raise ReleaseError("Existing release is not a matching script-managed release; refusing to change it.")


def version_core(version):
    return tuple(map(int, parse_version(version).groups()[:3]))


def require_new_version(github, version):
    if any(prior >= version_core(version) for prior in github.published_stable_versions()):
        raise ReleaseError("A new release must advance the numeric version beyond published stable releases.")


def require_latest_stable(github, tag):
    release = github.release(tag)
    versions = github.published_stable_versions()
    if (not release or release["draft"] or release["prerelease"] or not versions
            or version_core(tag[1:]) != max(versions)):
        raise ReleaseError("Only the latest published stable release may replace the documentation site.")


def sync_assets(github, release, tag, directory, names, upload):
    existing = github.assets(release)
    if len({asset["name"] for asset in existing}) != len(existing):
        raise ReleaseError("Release contains duplicate asset names.")
    if {asset["name"] for asset in existing} - set(names):
        raise ReleaseError("Release contains unexpected assets; refusing to overwrite it.")
    for asset in existing:
        if not github.asset_matches({**asset, "release_tag": tag}, directory / asset["name"]):
            raise ReleaseError(f"Existing asset differs: {asset['name']}; no assets were overwritten.")
    missing = set(names) - {asset["name"] for asset in existing}
    if missing and not upload:
        raise ReleaseError("Release is missing required assets.")
    for name in sorted(missing):
        # No --clobber: concurrent attempts must fail safely rather than replace bytes.
        github.cli("release", "upload", tag, str(directory / name))


def deploy_docs(github, tag, timeout, poll):
    title = f"CETLVaSt / {tag}"
    runs = [run for run in github.runs(event="workflow_dispatch", branch="main")
            if run["display_title"] == title]
    # A completed successful deployment makes --resume a no-op.
    if runs:
        latest = max(runs, key=lambda run: run["id"])
        if latest["status"] == "completed" and latest["conclusion"] == "success":
            jobs = github.jobs(latest)
            if any(job["name"] == "deploy-docs" and job["conclusion"] == "success" for job in jobs):
                print(f"Documentation: {latest['html_url']}")
                return
    previous = max((run["id"] for run in runs), default=0)
    github.cli("workflow", "run", WORKFLOW, "--ref", "main", "-f", f"docs_ref={tag}")
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        runs = [run for run in github.runs(event="workflow_dispatch", branch="main")
                if run["display_title"] == title and run["id"] > previous]
        if runs:
            run = max(runs, key=lambda item: item["id"])
            if run["status"] == "completed":
                jobs = github.jobs(run)
                if run["conclusion"] != "success" or not any(
                    job["name"] == "deploy-docs" and job["conclusion"] == "success" for job in jobs
                ):
                    raise ReleaseError(f"Release is published, but documentation failed: {run['html_url']}; use --resume.")
                print(f"Documentation: {run['html_url']}")
                return
        time.sleep(poll)
    raise ReleaseError("Release is published, but documentation timed out; use --resume to recover.")


def cut_release(args, git=None, github=None):
    parse_version(args.version)
    notes = args.notes_file.read_text(encoding="utf-8") if args.notes_file else None
    tag = "v" + args.version
    if not re.fullmatch(r"[\w.-]+/[\w.-]+", args.repo) or args.remote.startswith("-"):
        raise ReleaseError("Expected --repo OWNER/NAME and a named Git remote.")
    git = git or Git(args.root)
    github = github or GitHub(args.repo)
    if git.run("status", "--porcelain"):
        raise ReleaseError("The checkout must be clean, including untracked files.")
    if git.remote_repository(args.remote).lower() != args.repo.lower():
        raise ReleaseError("Git remote does not match --repo.")
    repository = github.api("")
    if repository.get("archived") or not repository.get("permissions", {}).get("push"):
        raise ReleaseError("GitHub credentials require repository write access.")
    if repository["default_branch"] != "main":
        raise ReleaseError("The supported release branch is main; review the policy before changing it.")
    git.run("fetch", "--no-tags", args.remote, "refs/heads/main:refs/remotes/" + args.remote + "/main")
    commit = git.commit(args.ref)
    try:
        git.run("merge-base", "--is-ancestor", commit, f"refs/remotes/{args.remote}/main")
    except ReleaseError as ex:
        raise ReleaseError("Release commit must already be merged into the remote main branch.") from ex
    metadata = git.validate(commit, tag)
    remote_commit = git.remote_tag(args.remote, tag)
    if remote_commit and remote_commit != commit:
        raise ReleaseError(f"Remote tag {tag} points to another commit; refusing to move it.")
    local_tags = git.run("tag", "--list", tag)
    if local_tags and git.commit(f"refs/tags/{tag}") != commit:
        raise ReleaseError(f"Local tag {tag} points to another commit.")
    release = github.release(tag)
    runs = github.runs(head_sha=commit)  # Check Actions read access before pushing a tag.
    marker = f"<!-- CETL release tag={tag} commit={commit} -->"
    if release:
        check_draft(release, tag, metadata["prerelease"], marker)
        if not remote_commit:
            raise ReleaseError("A release exists without its remote tag; manual investigation is required.")
    if not release or release["draft"]:
        require_new_version(github, args.version)
    if (release or remote_commit) and not args.resume and not args.dry_run:
        raise ReleaseError("Release/tag already exists; inspect it and use --resume for a matching attempt.")
    if not remote_commit:
        git.run("push", "--dry-run", args.remote, f"{commit}:refs/tags/{tag}")
    print(f"Repository: {args.repo}\nTag: {tag}\nCommit: {commit}\nPrerelease: {metadata['prerelease']}")
    if args.dry_run:
        print("Dry run: push immutable tag; await full CETLVaSt; download and verify bundle; create/resume draft; "
              "upload matching assets; publish; deploy stable documentation. No remote writes performed.")
        return
    if not remote_commit:
        git.run("push", args.remote, f"{commit}:refs/tags/{tag}")
    elif not any(run["head_branch"] == tag and run["event"] in ("push", "workflow_dispatch")
                 for run in runs):
        github.cli("workflow", "run", WORKFLOW, "--ref", tag)
    run = wait_for_release_run(github, commit, tag, args.timeout, args.poll)
    with tempfile.TemporaryDirectory(prefix="cetl-release-") as temporary:
        directory = Path(temporary)
        github.cli("run", "download", str(run["id"]), "--name", "release-bundle", "--dir", str(directory))
        names = check_bundle(directory, commit, tag)
        if git.remote_tag(args.remote, tag) != commit:
            raise ReleaseError("Remote tag changed while waiting for CI.")
        # Refresh after the wait: a concurrent attempt may have created or published the draft.
        release = github.release(tag)
        if not release:
            github.create_draft(tag, commit, metadata["prerelease"], marker, run, notes)
            release = github.release(tag)
        check_draft(release, tag, metadata["prerelease"], marker)
        sync_assets(github, release, tag, directory, names, upload=release["draft"])
        release = github.release(tag)
        check_draft(release, tag, metadata["prerelease"], marker)
        sync_assets(github, release, tag, directory, names, upload=False)
        if release["draft"]:
            require_new_version(github, args.version)
            if git.remote_tag(args.remote, tag) != commit:
                raise ReleaseError("Remote tag changed before publication.")
            # Never edit a published release, including on a retry.
            github.api(f"releases/{release['id']}", "PATCH", draft=False, make_latest="false" if metadata["prerelease"] else "true")
        print(f"Release: https://github.com/{args.repo}/releases/tag/{tag}")
    if metadata["prerelease"]:
        print("Documentation: built and attached; prereleases do not replace the stable Pages site.")
    else:
        if any(prior > version_core(args.version) for prior in github.published_stable_versions()):
            print("Documentation: a newer stable release is published; its site is retained.")
        else:
            require_latest_stable(github, tag)
            deploy_docs(github, tag, args.timeout, args.poll)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True, help="X.Y.Z or X.Y.Z-{alpha,beta,rc}.N, without v")
    parser.add_argument("--ref", required=True, help="Explicit commit/ref already merged into origin/main")
    parser.add_argument("--repo", default="OpenCyphal/CETL")
    parser.add_argument("--remote", default="origin")
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--notes-file", type=Path, help="Reviewed release notes; otherwise generate notes from GitHub")
    parser.add_argument("--dry-run", action="store_true", help="Perform preflight reads, without remote writes")
    parser.add_argument("--resume", action="store_true", help="Resume only an identical tag/script-managed release")
    parser.add_argument("--timeout", type=int, default=7200, help="Seconds to wait for each CI/deployment phase")
    parser.add_argument("--poll", type=int, default=15, help="Seconds between GitHub status reads")
    args = parser.parse_args()
    if args.timeout < 1 or args.poll < 1:
        parser.error("--timeout and --poll must be positive")
    try:
        cut_release(args)
    except (ReleaseError, OSError, ValueError, KeyError) as ex:
        parser.exit(1, f"Release stopped: {ex}\nNo tags or assets were force-updated. Inspect remote state before --resume.\n")


if __name__ == "__main__":
    main()
