#!/usr/bin/env python3
"""Cut a CETL release using Git and the maintainer's authenticated GitHub CLI."""

import argparse
from fnmatch import fnmatchcase
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile
import time
from urllib.parse import urlencode
from urllib.error import HTTPError
from urllib.request import Request, urlopen

from release_tools import (Git, ReleaseError, WORKFLOW, check_bundle, check_documentation, check_run, command,
                           parse_version, sha256)


def check_local_tools(python_version=None):
    if (python_version or sys.version_info) < (3, 10):
        raise ReleaseError("Run the release script with Python 3.10 or newer.")
    command(["git", "--version"])
    command(["gh", "--version"])
    for operation in (("run", "download"), ("release", "upload"), ("workflow", "run")):
        try:
            command(["gh", *operation, "--help"])
        except ReleaseError as ex:
            raise ReleaseError("Update GitHub CLI from https://cli.github.com/ and retry; "
                               f"gh {' '.join(operation)} is unavailable.") from ex


def release_command(args, root, version, commit, resume=False):
    parts = [sys.executable, str(Path(__file__).resolve()), "--root", str(root), "--repo", args.repo,
             "--remote", args.remote, "--version", version, "--ref", commit,
             "--timeout", str(args.timeout), "--poll", str(args.poll)]
    if args.notes_file:
        parts.extend(["--notes-file", str(args.notes_file.resolve())])
    if resume:
        parts.append("--resume")
    return subprocess.list2cmdline(parts) if os.name == "nt" else shlex.join(parts)


class GitHub:
    def __init__(self, repository):
        self.repository = repository

    def cli(self, *args):
        try:
            return command(["gh", *args, "--repo", "github.com/" + self.repository])
        except ReleaseError as ex:
            raise ReleaseError(f"gh {' '.join(args[:2])} failed. Run gh auth status --hostname github.com; "
                               "release operations need Contents read/write and Actions read/write access. "
                               "Then retry the printed resume command.") from ex

    def api(self, path, method="GET", **fields):
        args = ["gh", "api", "--hostname", "github.com", "--method", method,
                f"repos/{self.repository}/{path}".rstrip("/")]
        for key, value in fields.items():
            args.extend(["-F" if isinstance(value, bool) else "-f",
                         f"{key}={str(value).lower() if isinstance(value, bool) else value}"])
        try:
            result = command(args)
        except ReleaseError as ex:
            raise ReleaseError(f"GitHub {method} {path or 'repository access'} failed. "
                               "Run gh auth login --hostname github.com or fix GH_TOKEN/SSO access. "
                               "The credential needs Contents read/write and Actions read/write "
                               "(plus Pages read for stable documentation).") from ex
        return json.loads(result) if result else None

    def preflight(self, stable):
        workflow = self.api(f"actions/workflows/{WORKFLOW}")
        if workflow.get("state") != "active":
            raise ReleaseError(f"Enable CETLVaSt: gh workflow enable {WORKFLOW} --repo {self.repository}")
        if not stable:
            return
        try:
            site = self.api("pages")
            environment = self.api("environments/github-pages")
        except ReleaseError as ex:
            raise ReleaseError(f"Cannot read Pages setup. Check credential Pages/Actions read access and "
                               f"https://github.com/{self.repository}/settings/pages and "
                               f"https://github.com/{self.repository}/settings/environments.") from ex
        if site.get("build_type") != "workflow":
            raise ReleaseError(f"Select GitHub Actions as the Pages build source at "
                               f"https://github.com/{self.repository}/settings/pages.")
        policy = environment.get("deployment_branch_policy") or {}
        if policy.get("protected_branches"):
            allowed = self.api("branches/main").get("protected", False)
        elif policy.get("custom_branch_policies"):
            policies = self.pages("environments/github-pages/deployment-branch-policies", "branch_policies")
            allowed = any(item.get("type", "branch") == "branch" and fnmatchcase("main", item["name"])
                          for item in policies)
        else:
            allowed = True
        if not allowed:
            raise ReleaseError(f"Allow main to deploy to github-pages at "
                               f"https://github.com/{self.repository}/settings/environments.")

    def verify_pages(self, version, timeout, poll):
        url = self.api("pages")["html_url"]
        print(f"Checking published documentation: {url} ({version})")
        deadline = time.monotonic() + timeout
        last_error = "No documentation response received."
        while time.monotonic() < deadline:
            try:
                with urlopen(Request(url, headers={"Cache-Control": "no-cache"}), timeout=15) as response:
                    check_documentation(response.read(2 * 1024 * 1024).decode("utf-8"), version)
                print(f"Published documentation verified: {url} ({version})")
                return
            except ReleaseError as ex:
                last_error = str(ex)
            except HTTPError as ex:
                last_error = f"Documentation request returned HTTP {ex.code}."
            except (OSError, ValueError) as ex:
                last_error = f"Could not read documentation ({type(ex).__name__}); check network/TLS access."
            time.sleep(poll)
        raise ReleaseError(f"Release is published, but documentation verification failed at {url}. {last_error} "
                           "Fix access or allow Pages/CDN propagation, then retry the printed resume command.")

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
                try:
                    check_run(run, github.jobs(run), commit, tag)
                except ReleaseError as ex:
                    raise ReleaseError(f"{ex} See {run['html_url']}. For a transient failure run "
                                       f"gh run rerun {run['id']} --repo {github.repository} "
                                       "(the entire run), then use the printed resume command.") from ex
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


def verify_published_assets(github, release, commit, tag):
    # Published releases can outlive CI artifacts. Verify their actual assets directly on --resume.
    with tempfile.TemporaryDirectory(prefix="cetl-published-") as temporary:
        directory = Path(temporary)
        github.cli("release", "download", tag, "--dir", str(directory))
        names = check_bundle(directory, commit, tag)
        sync_assets(github, release, tag, directory, names, upload=False)
    print("Published release assets verified.")


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
    if runs and latest["status"] != "completed":
        previous = latest["id"] - 1  # Resume an in-flight deployment instead of queueing another full build.
    else:
        previous = max((run["id"] for run in runs), default=0)
        github.cli("workflow", "run", WORKFLOW, "--ref", "main", "-f", f"docs_ref={tag}")
    deadline = time.monotonic() + timeout
    approval_reported = set()
    while time.monotonic() < deadline:
        runs = [run for run in github.runs(event="workflow_dispatch", branch="main")
                if run["display_title"] == title and run["id"] > previous]
        if runs:
            run = max(runs, key=lambda item: item["id"])
            if run["status"] == "waiting" and run["id"] not in approval_reported:
                print(f"GitHub environment approval required: {run['html_url']}")
                approval_reported.add(run["id"])
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
    check_local_tools()
    if args.resume and (not args.version or not args.ref):
        raise ReleaseError("Use the printed Resume command: --resume requires explicit --version and --ref "
                           "so an advancing main branch cannot change the release being resumed.")
    if args.version:
        parse_version(args.version)
    if not re.fullmatch(r"[\w.-]+/[\w.-]+", args.repo) or args.remote.startswith("-"):
        raise ReleaseError("Expected --repo OWNER/NAME and a named Git remote.")
    git = git or Git(args.root)
    github = github or GitHub(args.repo)
    if git.run("status", "--porcelain"):
        raise ReleaseError("The checkout must be clean, including untracked files. "
                           "Run git status --short and commit or stash the reported changes.")
    if git.remote_repository(args.remote).lower() != args.repo.lower():
        raise ReleaseError("Git remote does not match --repo.")
    repository = github.api("")
    if repository.get("archived") or not repository.get("permissions", {}).get("push"):
        raise ReleaseError("GitHub credentials require repository write access. "
                           "Run gh auth login --hostname github.com with a maintainer account.")
    if repository["default_branch"] != "main":
        raise ReleaseError("The supported release branch is main; review the policy before changing it.")
    git.run("fetch", "--no-tags", args.remote, "refs/heads/main:refs/remotes/" + args.remote + "/main")
    main_commit = git.commit(f"refs/remotes/{args.remote}/main")
    commit = git.commit(args.ref) if args.ref else main_commit
    try:
        git.run("merge-base", "--is-ancestor", commit, f"refs/remotes/{args.remote}/main")
    except ReleaseError as ex:
        raise ReleaseError("Release commit must already be merged into the remote main branch.") from ex
    for target in {main_commit, commit}:
        git.require_release_files(target)
    version = args.version or git.validate(commit)["version"]
    tag = "v" + version
    metadata = git.validate(commit, tag)
    print(f"Repository: {args.repo}\nTag: {tag}\nCommit: {commit}\nPrerelease: {metadata['prerelease']}")
    print("Resume command: " + release_command(args, git.root, version, commit, resume=True))
    remote_commit = git.remote_tag(args.remote, tag)
    if remote_commit and remote_commit != commit:
        raise ReleaseError(f"Remote tag {tag} points to another commit; refusing to move it.")
    local_tags = git.run("tag", "--list", tag)
    if local_tags and git.commit(f"refs/tags/{tag}") != commit:
        raise ReleaseError(f"Local tag {tag} points to another commit.")
    release = github.release(tag)
    notes = args.notes_file.read_text(encoding="utf-8") if args.notes_file and not release else None
    runs = github.runs(head_sha=commit)  # Check Actions read access before pushing a tag.
    marker = f"<!-- CETL release tag={tag} commit={commit} -->"
    if release:
        if not release["draft"] and marker not in (release.get("body") or "") and not args.resume:
            raise ReleaseError(f"Version {version} is already published. Merge a reviewed version bump into main "
                               "before cutting a new release.")
        check_draft(release, tag, metadata["prerelease"], marker)
        if not remote_commit:
            raise ReleaseError("A release exists without its remote tag; manual investigation is required.")
    if not release or release["draft"]:
        require_new_version(github, version)
    deploy_stable = not metadata["prerelease"] and not any(
        prior > version_core(version) for prior in github.published_stable_versions())
    github.preflight(deploy_stable)
    if (release or remote_commit) and not args.resume and not args.dry_run:
        raise ReleaseError("Release/tag already exists; inspect it and use --resume for a matching attempt.")
    if not remote_commit:
        git.run("push", "--dry-run", args.remote, f"{commit}:refs/tags/{tag}")
    if args.dry_run:
        print("Dry run: push immutable tag; await full CETLVaSt; download and verify bundle; create/resume draft; "
              "upload matching assets; publish; deploy stable documentation. No remote writes performed.")
        print("Release this checked commit: " + release_command(
            args, git.root, version, commit, resume=bool(release or remote_commit or args.resume)))
        return
    if release and not release["draft"]:
        verify_published_assets(github, release, commit, tag)
        print(f"Release: https://github.com/{args.repo}/releases/tag/{tag}")
        finish_documentation(github, tag, version, metadata["prerelease"], args.timeout, args.poll)
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
            require_new_version(github, version)
            if git.remote_tag(args.remote, tag) != commit:
                raise ReleaseError("Remote tag changed before publication.")
            # Never edit a published release, including on a retry.
            github.api(f"releases/{release['id']}", "PATCH", draft=False, make_latest="false" if metadata["prerelease"] else "true")
        release = github.release(tag)
        check_draft(release, tag, metadata["prerelease"], marker)
        if release["draft"]:
            raise ReleaseError("GitHub still reports a draft; retry the printed resume command.")
        sync_assets(github, release, tag, directory, names, upload=False)
        print(f"Release: https://github.com/{args.repo}/releases/tag/{tag}")
    finish_documentation(github, tag, version, metadata["prerelease"], args.timeout, args.poll)


def finish_documentation(github, tag, version, prerelease, timeout, poll):
    if prerelease:
        print("Documentation: built and attached; prereleases do not replace the stable Pages site.")
    else:
        if any(prior > version_core(version) for prior in github.published_stable_versions()):
            print("Documentation: a newer stable release is published; its site is retained.")
        else:
            require_latest_stable(github, tag)
            deploy_docs(github, tag, timeout, poll)
            github.verify_pages(version, timeout, poll)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", help="Override header version, e.g. X.Y.Z-rc.1 (without v)")
    parser.add_argument("--ref", help="Commit/ref merged into remote main; defaults to freshly fetched main")
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
    except KeyboardInterrupt:
        parser.exit(130, "Release interrupted. Use the printed Resume command to continue.\n")
    except (ReleaseError, OSError, ValueError, KeyError) as ex:
        parser.exit(1, f"Release stopped: {ex}\nFix the reported problem and retry; existing tags/assets are preserved.\n")


if __name__ == "__main__":
    main()
