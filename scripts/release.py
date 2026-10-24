#!/usr/bin/env python3
"""Publish verified CETL releases using Git and authenticated GitHub CLI access.

Preflight selects a merged commit and validates the release configuration.
Publication uses verified main CI, checks assets, and deploys documentation.
Matching attempts can resume after interruption; completed remote operations are
preserved. Python and the standard library implement the orchestration;
``release_tools`` supplies shared validation and packaging contracts.
"""

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
from typing import Any
from urllib.parse import urlencode
from urllib.error import HTTPError
from urllib.request import Request, urlopen

from release_tools import (DOCS_WORKFLOW, Git, ReleaseError, WORKFLOW, check_bundle, check_documentation, check_run, command,
                           parse_version, sha256)

_PYTHON_MINIMUM = (3, 14)


class _AssetsPending(ReleaseError):
    """Expected uploads have not appeared in GitHub's asset listing yet."""

def check_local_tools(python_version=None):
    """Require the supported Python version and Git/GitHub CLI operations.

    Args:
        python_version (tuple[int, ...] | None): Version tuple to check; None uses
            sys.version_info. Supplying a tuple supports isolated preflight tests.

    Raises:
        ReleaseError: Python is older than the minimum supported version, an executable
            is missing, or a required GitHub CLI help command is unavailable.

    Executes local version/help commands; repository access is checked separately.
    """
    if (python_version or sys.version_info) < _PYTHON_MINIMUM:
        raise ReleaseError(
            f"Run the release script with Python {_PYTHON_MINIMUM[0]}.{_PYTHON_MINIMUM[1]} or newer."
        )
    command(["git", "--version"])
    command(["gh", "--version"])
    for operation in (("run", "download"), ("release", "upload"), ("workflow", "run")):
        try:
            command(["gh", *operation, "--help"])
        except ReleaseError as ex:
            raise ReleaseError("Update GitHub CLI from https://cli.github.com/ and retry; "
                               f"gh {' '.join(operation)} is unavailable.") from ex


def release_command(args, root, commit):
    """Format a retry command pinned to a checked source commit.

    Args:
        args (argparse.Namespace): Repository, remote, timeout, poll, and notes options.
        root (str | Path): Checkout directory.
        commit (str): Full source commit object ID.

    Returns:
        str: Quoted command using this interpreter and script.
    """
    parts = [sys.executable, str(Path(__file__).resolve()), "--root", str(root), "--repo", args.repo,
             "--remote", args.remote, "--ref", commit, "--timeout", str(args.timeout), "--poll", str(args.poll)]
    if args.notes_file:
        parts.extend(["--notes-file", str(args.notes_file.resolve())])
    return subprocess.list2cmdline(parts) if os.name == "nt" else shlex.join(parts)


class GitHub:
    """Access a github.com repository through the authenticated ``gh`` executable.

    Args:
        repository (str): OWNER/NAME repository identity.

    Attributes:
        repository (str): Identity used for every API and CLI request.

    API records retain GitHub's JSON field names. Methods propagate ReleaseError
    for command failures and ValueError for malformed JSON responses. Read
    methods query remote state; ``cli``, ``api``, and ``create_draft`` can mutate
    it. Authentication comes from GitHub CLI's configuration or environment.
    """

    def __init__(self, repository):
        """Store the repository identity without issuing a network request."""
        self.repository = repository

    def cli(self, *args):
        """Execute a repository-scoped GitHub CLI command on github.com.

        Args:
            *args (str): Subcommand and arguments; the repository option is added.

        Returns:
            str: Decoded, stripped stdout from the command.

        Raises:
            ReleaseError: The command is unavailable, fails, or times out.

        The supplied subcommand controls downloads, uploads, or other mutations.
        """
        try:
            return command(["gh", *args, "--repo", "github.com/" + self.repository])
        except ReleaseError as ex:
            raise ReleaseError(f"gh {' '.join(args[:2])} failed. Run gh auth status --hostname github.com; "
                               "release operations need Contents read/write and Actions read/write access. "
                               "Then retry the same release command.") from ex

    def api(self, path, method="GET", **fields):
        """Issue a repository-relative REST request and decode its JSON response.

        Args:
            path (str): Path beneath repos/OWNER/NAME, including any query string;
                an empty string requests repository metadata.
            method (str): HTTP method; defaults to a read-only GET.
            **fields: Request fields. Booleans are typed JSON values; other
                supplied values are passed as strings to GitHub CLI.

        Returns:
            dict | list | None: Parsed response, or None for an empty response.

        Raises:
            ReleaseError: GitHub CLI cannot complete the request.
            ValueError: A nonempty response is invalid JSON.

        Write methods may change remote state before a transport failure occurs.
        """
        args = ["gh", "api", "--hostname", "github.com", "--method", method,
                f"repos/{self.repository}/{path}".rstrip("/")]
        if method == "GET":
            args.extend(["-H", "Cache-Control: no-cache"])
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

    def _api_object(self, path: str, method: str = "GET", **fields) -> dict[str, Any]:
        """Require an object response from a metadata endpoint, or raise ReleaseError."""
        result = self.api(path, method, **fields)
        if not isinstance(result, dict):
            raise ReleaseError(f"GitHub {method} {path or 'repository metadata'} returned no JSON object.")
        return result

    def preflight(self, stable):
        """Check workflow availability and, when needed, Pages deployment setup.

        Args:
            stable (bool): Require Actions-based Pages and an environment policy
                permitting main when True; otherwise check only CETLVaSt.

        Raises:
            ReleaseError: Configuration is inaccessible, CETLVaSt is disabled,
                Pages uses another build source, or main cannot deploy.

        Performs reads and reports setup fixes without changing repository policy.
        """
        workflow = self._api_object(f"actions/workflows/{WORKFLOW}")
        if workflow.get("state") != "active":
            raise ReleaseError(f"Enable CETLVaSt: gh workflow enable {WORKFLOW} --repo {self.repository}")
        if not stable:
            return
        if self._api_object(f"actions/workflows/{DOCS_WORKFLOW}").get("state") != "active":
            raise ReleaseError(f"Enable release documentation: gh workflow enable {DOCS_WORKFLOW} --repo {self.repository}")
        try:
            site = self._api_object("pages")
            environment = self._api_object("environments/github-pages")
        except ReleaseError as ex:
            raise ReleaseError(f"Cannot read Pages setup. Check credential Pages/Actions read access and "
                               f"https://github.com/{self.repository}/settings/pages and "
                               f"https://github.com/{self.repository}/settings/environments.") from ex
        if site.get("build_type") != "workflow":
            raise ReleaseError(f"Select GitHub Actions as the Pages build source at "
                               f"https://github.com/{self.repository}/settings/pages.")
        policy = environment.get("deployment_branch_policy") or {}
        if policy.get("protected_branches"):
            allowed = self._api_object("branches/main").get("protected", False)
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
        """Wait for the public Pages HTML to display the expected release version.

        Args:
            version (str): Expected Doxygen project number, without the tag's v.
            timeout (int | float): Polling budget in seconds.
            poll (int | float): Seconds to sleep between unsuccessful requests.

        Raises:
            ReleaseError: Pages metadata cannot be read, or network/content
                verification remains unsuccessful when the budget expires.

        Reads at most two MiB per HTML response with a 15-second request timeout.
        Public HTML requests carry no GitHub credentials. Progress and successful
        verification are printed; the last failure supplies recovery guidance.
        """
        url = self._api_object("pages")["html_url"]
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
                           "Fix access or allow Pages/CDN propagation, then retry the same release command.")

    def pages(self, path, key=None):
        """Collect a REST collection across pages of up to 100 records.

        Args:
            path (str): Repository-relative collection path, optionally with a
                query string, without pagination parameters.
            key (str | None): Collection field in an object response; None means
                that each response is itself a list.

        Returns:
            list: Records in API page order, including an empty collection.

        Raises:
            ReleaseError: A request fails or 1,000 full pages are exhausted.
        """
        items = []
        for page in range(1, 1001):
            separator = "&" if "?" in path else "?"
            result = self.api(f"{path}{separator}per_page=100&page={page}")
            if key:
                if not isinstance(result, dict):
                    raise ReleaseError(f"GitHub collection {path} returned no JSON object.")
                batch = result.get(key)
            else:
                batch = result
            if not isinstance(batch, list):
                raise ReleaseError(f"GitHub collection {path} returned no JSON array.")
            items.extend(batch)
            if len(batch) < 100:
                return items
        raise ReleaseError("GitHub pagination limit exceeded.")

    def release(self, tag):
        """Find a release, including visible drafts, by exact tag name.

        Args:
            tag (str): v-prefixed tag identifying the release.

        Returns:
            dict | None: GitHub release record, or None after a successful
                complete listing finds no matching tag.

        Raises:
            ReleaseError: Listing fails; permission errors are not treated as
                evidence that the release is absent.
        """
        return next((release for release in self.pages("releases") if release["tag_name"] == tag), None)

    def release_by_id(self, release_id):
        """Read a known release directly, bypassing release-list visibility delays.

        Args:
            release_id (int): ID returned by release creation or discovery.

        Returns:
            dict: GitHub release metadata.

        Raises:
            ReleaseError: The request fails or does not return an object.
        """
        return self._api_object(f"releases/{release_id}")

    def published_stable_versions(self):
        """Collect numeric versions from published, stable, vX.Y.Z releases.

        Returns:
            set[tuple[int, int, int]]: Version cores suitable for numeric ordering.
                Drafts, prereleases, and nonconforming historical tags are ignored.

        Raises:
            ReleaseError: The release listing cannot be completed.
        """
        versions = set()
        for release in self.pages("releases"):
            if not release["draft"] and not release["prerelease"]:
                match = re.fullmatch(r"v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", release["tag_name"])
                if match:
                    versions.add(tuple(map(int, match.groups())))
        return versions

    def runs(self, workflow=WORKFLOW, **filters):
        """List workflow runs matching GitHub's REST filter fields.

        Args:
            workflow (str): Workflow filename; defaults to CETLVaSt.
            **filters: Run-list parameters such as head_sha, event, and branch;
                values are URL-encoded before requesting every result page.

        Returns:
            list[dict]: GitHub Actions run records, in API order.

        Raises:
            ReleaseError: The workflow run listing cannot be completed.
        """
        return self.pages(f"actions/workflows/{workflow}/runs?{urlencode(filters)}", "workflow_runs")

    def jobs(self, run):
        """List jobs belonging to exactly the attempt identified by a run record.

        Args:
            run (dict): Actions record containing id and run_attempt.

        Returns:
            list[dict]: Attempt-specific job records with names and conclusions.

        Raises:
            ReleaseError: The job listing cannot be completed.

        Successful jobs from earlier attempts cannot satisfy a later attempt.
        """
        return self.pages(f"actions/runs/{run['id']}/attempts/{run['run_attempt']}/jobs", "jobs")

    def assets(self, release):
        """List all assets attached to a release.

        Args:
            release (dict): GitHub release record containing id.

        Returns:
            list[dict]: Asset records, including upload state, size, and any digest.

        Raises:
            ReleaseError: The asset listing cannot be completed.
        """
        return self.pages(f"releases/{release['id']}/assets")

    def create_draft(self, tag, commit, marker, run, notes=None):
        """Create a draft with source identity and successful verification evidence.

        Args:
            tag (str): Existing v-prefixed remote tag; also the release title.
            commit (str): Full source commit object ID.
            marker (str): Script-management marker used to identify matching retries.
            run (dict): Verified Actions run containing html_url.
            notes (str | None): Reviewed body text, or None to generate GitHub notes.

        Raises:
            ReleaseError: Note generation or release creation fails.

        Returns:
            dict: Creation response, including the release ID.

        Writes a remote draft. Callers must check for an existing release first
        and re-read remote state after an interrupted creation request.
        """
        if notes is None:
            notes = self._api_object("releases/generate-notes", "POST", tag_name=tag, target_commitish=commit)["body"]
        body = f"{notes}\n\nSource commit: `{commit}`\n\nVerification: {run['html_url']}\n\n{marker}\n"
        return self._api_object("releases", "POST", tag_name=tag, target_commitish=commit, name=tag,
                                draft=True, prerelease=False, body=body)

    def asset_matches(self, asset, path):
        """Compare a remote asset's uploaded bytes with a local file.

        Args:
            asset (dict): Asset record with state and size, optional digest, and
                release_tag for the download fallback.
            path (Path): Local file whose name identifies the remote asset.

        Returns:
            bool: True when upload state, size, and SHA-256 match. Assets lacking
                a server digest are downloaded into a temporary directory.

        Raises:
            ReleaseError: A fallback download fails.
            OSError: The local or downloaded file cannot be read.
        """
        if asset.get("state") != "uploaded" or asset.get("size") != path.stat().st_size:
            return False
        if asset.get("digest"):
            return asset["digest"] == "sha256:" + sha256(path)
        # Older GitHub assets may not have a server-side digest. Compare the actual bytes.
        with tempfile.TemporaryDirectory(prefix="cetl-asset-") as directory:
            self.cli("release", "download", asset["release_tag"], "--pattern", path.name, "--dir", directory)
            return sha256(Path(directory) / path.name) == sha256(path)


def wait_for_release_run(github, commit, timeout, poll):
    """Wait for full verification of a main commit before creating its release tag.

    Args:
        github (GitHub): Repository client.
        commit (str): Exact merged source commit.
        timeout (int | float): Polling budget in seconds.
        poll (int | float): Seconds between status reads.

    Returns:
        dict: Successful main-branch CETLVaSt run, including the artifact run ID.

    Raises:
        ReleaseError: CI fails, cannot be read, or does not finish within the budget.
    """
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        candidates = [run for run in github.runs(head_sha=commit, branch="main", event="push")
                      if run["head_branch"] == "main" and run["event"] == "push"]
        if candidates:
            run = max(candidates, key=lambda item: item["id"])
            if run["status"] == "completed":
                try:
                    check_run(run, github.jobs(run), commit)
                except ReleaseError as ex:
                    raise ReleaseError(f"{ex} See {run['html_url']}. For a transient failure run "
                                       f"gh run rerun {run['id']} --repo {github.repository}, "
                                       "then rerun the release command.") from ex
                print(f"Verification: {run['html_url']}")
                return run
        time.sleep(poll)
    raise ReleaseError("Timed out waiting for main CI; rerun the release command when verification finishes.")


def check_draft(release, tag, marker):
    """Require a release to belong to the same script-managed attempt.

    Args:
        release (dict): Draft or published release with tag_name, prerelease,
            and body fields. Publication state is checked by the caller.
        tag (str): Expected exact tag name.
        marker (str): Management marker that must appear in the release body.

    Raises:
        ReleaseError: The tag, classification, or management marker disagrees.
    """
    if (release.get("tag_name") != tag or release.get("prerelease") is not False
            or marker not in (release.get("body") or "")):
        raise ReleaseError("Existing release is not a matching script-managed release; refusing to change it.")


def version_core(version):
    """Extract an orderable numeric core from a supported version string.

    Args:
        version (str): X.Y.Z version without a v prefix.

    Returns:
        tuple[int, int, int]: Major, minor, and patch as integers.

    Raises:
        ReleaseError: The version fails the shared grammar.
    """
    return tuple(map(int, parse_version(version).groups()[:3]))


def require_new_version(github, version):
    """Require a candidate's numeric core to exceed every published stable core.

    Args:
        github (GitHub): Repository client used to list published stable versions.
        version (str): Candidate X.Y.Z version without v.

    Raises:
        ReleaseError: A published stable core is greater than or equal to the
            candidate's core, or the remote listing cannot be completed.
    """
    if any(prior >= version_core(version) for prior in github.published_stable_versions()):
        raise ReleaseError("A new release must advance the numeric version beyond published stable releases.")


def require_latest_stable(github, tag, release=None):
    """Require a tag to identify the newest published stable release.

    Args:
        github (GitHub): Repository client providing release and version records.
        tag (str): Validated v-prefixed version being considered for deployment.
        release (dict | None): Known publication response; otherwise read the
            published release by its tag.

    Raises:
        ReleaseError: The release is absent, draft, prerelease, older than the
            newest stable core, or remote state cannot be read.
    """
    release = release if release is not None else github._api_object(f"releases/tags/{tag}")
    versions = github.published_stable_versions()
    if (release.get("tag_name") != tag or release["draft"] or release["prerelease"]
            or any(prior > version_core(tag[1:]) for prior in versions)):
        raise ReleaseError("Only the latest published stable release may replace the documentation site.")


def sync_assets(github, release, tag, directory, names, upload):
    """Verify existing assets and optionally upload missing expected files.

    Args:
        github (GitHub): Client used to list, compare, and upload assets.
        release (dict): Target release record containing id.
        tag (str): Exact release tag used for downloads and uploads.
        directory (Path): Local directory containing the verified bundle files.
        names (Collection[str]): Complete collection of expected asset filenames.
        upload (bool): Permit missing assets to be uploaded when True; otherwise
            require the release to contain the complete bundle already.

    Raises:
        ReleaseError: Names are duplicated/unexpected, existing bytes differ,
            required assets are missing, or an upload/comparison fails.
        OSError: A file needed for comparison cannot be read.

    Existing files are checked before uploads and never overwritten. Interrupted
    uploads may leave a partial remote set for a matching attempt to resume.
    """
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
        raise _AssetsPending("Release is missing required assets.")
    for name in sorted(missing):
        # No --clobber: concurrent attempts must fail safely rather than replace bytes.
        github.cli("release", "upload", tag, str(directory / name))


def verify_assets(github, release, tag, directory, names, timeout, poll):
    """Wait up to 60 seconds for uploaded assets to appear, then verify their bytes.

    Args:
        github (GitHub): Repository client.
        release (dict): Release with a known ID.
        tag (str): Expected vX.Y.Z tag.
        directory (Path): Verified local bundle directory.
        names (Collection[str]): Expected asset filenames.
        timeout (int | float): Upper bound for the visibility wait in seconds.
        poll (int | float): Poll interval, capped at two seconds.

    Raises:
        ReleaseError: Assets conflict, remain missing, or cannot be read.
    """
    try:
        sync_assets(github, release, tag, directory, names, upload=False)
        return
    except _AssetsPending:
        pass
    deadline = time.monotonic() + min(timeout, 60)
    while time.monotonic() < deadline:
        time.sleep(min(poll, 2))
        try:
            sync_assets(github, release, tag, directory, names, upload=False)
            return
        except _AssetsPending:
            pass
    raise ReleaseError("Uploaded assets are not visible yet; rerun the release command.")


def verify_published_assets(github, release, commit, tag):
    """Verify a published release directly, independent of CI artifact retention.

    Args:
        github (GitHub): Client used for asset downloads and remote comparisons.
        release (dict): Published release record containing id.
        commit (str): Expected full source commit object ID.
        tag (str): Expected validated v-prefixed release version.

    Raises:
        ReleaseError: Download, bundle validation, or remote comparison fails.
        OSError: Temporary assets cannot be read or written.
        ValueError: The downloaded manifest is invalid JSON.

    Downloads into a temporary directory, prints successful verification, and
    leaves the published release unchanged.
    """
    with tempfile.TemporaryDirectory(prefix="cetl-published-") as temporary:
        directory = Path(temporary)
        github.cli("release", "download", tag, "--dir", str(directory))
        names = check_bundle(directory, commit, tag)
        sync_assets(github, release, tag, directory, names, upload=False)
    print("Published release assets verified.")


def deploy_docs(github, tag, timeout, poll):
    """Deploy verified release documentation through the main-branch Pages environment.

    Args:
        github (GitHub): Client used to find/dispatch documentation runs and inspect jobs.
        tag (str): Published stable tag, checked by the caller before deployment.
        timeout (int | float): Polling budget in seconds.
        poll (int | float): Seconds between deployment-status polls.

    Raises:
        ReleaseError: API/dispatch access fails, the newest matching run fails,
            its deployment job is unsuccessful, or the budget expires.

    Reuses a successful or in-flight matching dispatch. Otherwise starts a run
    on main with the release tag. Prints environment-approval links
    when required; site-content verification belongs to ``finish_documentation``.
    """
    title = f"CETL docs / {tag}"
    runs = [run for run in github.runs(workflow=DOCS_WORKFLOW, event="workflow_dispatch", branch="main")
            if run["display_title"] == title]
    # A completed successful deployment makes repeating the command a no-op.
    latest = max(runs, key=lambda run: run["id"]) if runs else None
    if latest is not None:
        if latest["status"] == "completed" and latest["conclusion"] == "success":
            jobs = github.jobs(latest)
            if any(job["name"] == "deploy-docs" and job["conclusion"] == "success" for job in jobs):
                print(f"Documentation: {latest['html_url']}")
                return
    if latest is not None and latest["status"] != "completed":
        previous = latest["id"] - 1  # Resume an in-flight deployment instead of queueing another full build.
    else:
        previous = max((run["id"] for run in runs), default=0)
        github.cli("workflow", "run", DOCS_WORKFLOW, "--ref", "main", "-f", f"tag={tag}")
    deadline = time.monotonic() + timeout
    approval_reported = set()
    while time.monotonic() < deadline:
        runs = [run for run in github.runs(workflow=DOCS_WORKFLOW, event="workflow_dispatch", branch="main")
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
                    raise ReleaseError(f"Release is published, but documentation failed: {run['html_url']}; rerun the release command.")
                print(f"Documentation: {run['html_url']}")
                return
        time.sleep(poll)
    raise ReleaseError("Release is published, but documentation timed out; rerun the release command to recover.")


def cut_release(args, git=None, github=None):
    """Release the header version from verified main source, continuing matching attempts.

    Args:
        args (argparse.Namespace): root, repo, remote, optional ref and notes_file,
            dry_run, and positive timeout/poll durations in seconds.
        git (Git | None): Checkout adapter; None uses args.root.
        github (GitHub | None): Repository adapter; None uses args.repo.

    Raises:
        ReleaseError: Preflight, verification, identity, asset, or deployment checks fail.
        OSError: Local files or subprocesses cannot be accessed.
        ValueError: A service response or manifest is invalid JSON.
        KeyError: A service record lacks a required field.

    Checks CI and the bundle before pushing a new tag. Existing matching tags,
    drafts, uploads, and published releases are reused. Retry commands pin the
    selected commit. Dry runs perform preflight without remote writes.
    """
    check_local_tools()
    if not re.fullmatch(r"[\w.-]+/[\w.-]+", args.repo) or args.remote.startswith("-"):
        raise ReleaseError("Expected --repo OWNER/NAME and a named Git remote.")
    git = git or Git(args.root)
    github = github or GitHub(args.repo)
    if git.run("status", "--porcelain"):
        raise ReleaseError("The checkout must be clean. Commit or stash the changes reported by git status --short.")
    if git.remote_repository(args.remote).lower() != args.repo.lower():
        raise ReleaseError("Git remote does not match --repo.")
    repository = github._api_object("")
    if repository.get("archived") or not repository.get("permissions", {}).get("push"):
        raise ReleaseError("GitHub credentials require repository write access. Run gh auth login --hostname github.com.")
    if repository["default_branch"] != "main":
        raise ReleaseError("The release branch must be main.")
    git.run("fetch", "--no-tags", args.remote, "refs/heads/main:refs/remotes/" + args.remote + "/main")
    main_commit = git.commit(f"refs/remotes/{args.remote}/main")
    commit = git.commit(args.ref) if args.ref else main_commit
    version = git.validate(commit)["version"]
    tag = "v" + version
    remote_commit = git.remote_tag(args.remote, tag)
    # A rerun of the default command continues the already selected release snapshot.
    if remote_commit and not args.ref:
        commit = git.commit(remote_commit)
    try:
        git.run("merge-base", "--is-ancestor", commit, f"refs/remotes/{args.remote}/main")
    except ReleaseError as ex:
        raise ReleaseError("Release commit must already be merged into remote main.") from ex
    for target in {main_commit, commit}:
        git.require_release_files(target)
    git.validate(commit, tag)
    print(f"Repository: {args.repo}\nTag: {tag}\nCommit: {commit}")
    print("Retry command: " + release_command(args, git.root, commit))
    if remote_commit and remote_commit != commit:
        raise ReleaseError(f"Remote tag {tag} points to another commit; refusing to move it.")
    if git.run("tag", "--list", tag) and git.commit(f"refs/tags/{tag}") != commit:
        raise ReleaseError(f"Local tag {tag} points to another commit.")
    marker = f"<!-- CETL release tag={tag} commit={commit} -->"
    release = github.release(tag)
    if release:
        release = github.release_by_id(release["id"])
        check_draft(release, tag, marker)
        if not remote_commit:
            raise ReleaseError("A release exists without its remote tag; refusing to change it.")
    if not release or release["draft"]:
        require_new_version(github, version)
    deploy_stable = not any(prior > version_core(version) for prior in github.published_stable_versions())
    github.preflight(deploy_stable)
    if not remote_commit:
        git.run("push", "--dry-run", args.remote, f"{commit}:refs/tags/{tag}")
    if args.dry_run:
        print("Dry run: verify main CI and bundle; push tag; upload and publish release; deploy documentation. "
              "No remote writes performed.")
        print("Release this checked commit: " + release_command(args, git.root, commit))
        return
    if release and not release["draft"]:
        verify_published_assets(github, release, commit, tag)
        finish_documentation(github, tag, version, args.timeout, args.poll, release)
        return
    notes = args.notes_file.read_text(encoding="utf-8") if args.notes_file and not release else None
    run = wait_for_release_run(github, commit, args.timeout, args.poll)
    with tempfile.TemporaryDirectory(prefix="cetl-release-") as temporary:
        directory = Path(temporary)
        github.cli("run", "download", str(run["id"]), "--name", "release-bundle", "--dir", str(directory))
        names = check_bundle(directory, commit, tag)
        current_tag = git.remote_tag(args.remote, tag)
        if current_tag not in (None, commit):
            raise ReleaseError("Remote tag changed while waiting for verification.")
        if current_tag is None:
            git.run("push", args.remote, f"{commit}:refs/tags/{tag}")
        release = github.release(tag)
        if release is not None:
            release = github.release_by_id(release["id"])
        if release is None:
            try:
                release = github.create_draft(tag, commit, marker, run, notes)
            except ReleaseError:
                # Creation can succeed before a lost response, or race another maintainer.
                for attempt in range(5):
                    release = github.release(tag)
                    if release is not None:
                        break
                    time.sleep(min(args.poll, 2))
                if release is None:
                    raise
        if not isinstance(release, dict):
            raise ReleaseError("GitHub did not return release metadata; rerun the release command.")
        check_draft(release, tag, marker)
        sync_assets(github, release, tag, directory, names, upload=release["draft"])
        verify_assets(github, release, tag, directory, names, args.timeout, args.poll)
        release = github.release_by_id(release["id"])
        check_draft(release, tag, marker)
        if release["draft"]:
            require_new_version(github, version)
            if git.remote_tag(args.remote, tag) != commit:
                raise ReleaseError("Remote tag changed before publication.")
            publish_error = None
            release_id = release["id"]
            try:
                release = github._api_object(f"releases/{release_id}", "PATCH", draft=False, make_latest="true")
            except ReleaseError as ex:
                publish_error = ex
                release = github.release_by_id(release_id)
            check_draft(release, tag, marker)
            deadline = time.monotonic() + min(args.timeout, 60) if release["draft"] else 0
            while release["draft"] and time.monotonic() < deadline:
                time.sleep(min(args.poll, 2))
                release = github.release_by_id(release["id"])
                check_draft(release, tag, marker)
            if release["draft"]:
                if publish_error is not None:
                    raise publish_error
                raise ReleaseError("GitHub has not confirmed publication yet; rerun the release command.")
        verify_assets(github, release, tag, directory, names, args.timeout, args.poll)
        print(f"Release: https://github.com/{args.repo}/releases/tag/{tag}")
    finish_documentation(github, tag, version, args.timeout, args.poll, release)


def finish_documentation(github, tag, version, timeout, poll, release=None):
    """Deploy and verify documentation for the latest published release.

    Args:
        github (GitHub): Repository and Pages client.
        tag (str): Published vX.Y.Z tag.
        version (str): Matching header version.
        timeout (int | float): Budget for each deployment/content check.
        poll (int | float): Seconds between checks.
        release (dict | None): Known publication response, avoiding list visibility delays.

    Raises:
        ReleaseError: Publication identity, deployment, or public HTML checks fail.

    Resuming an older release retains documentation belonging to a newer version.
    """
    if any(prior > version_core(version) for prior in github.published_stable_versions()):
        print("Documentation: a newer release is published; its site is retained.")
    else:
        require_latest_stable(github, tag, release)
        deploy_docs(github, tag, timeout, poll)
        github.verify_pages(version, timeout, poll)


def main():
    """Parse maintainer CLI arguments and run the release operation.

    Reads sys.argv and prints progress or recovery diagnostics. ``cut_release``
    defines filesystem/network effects; argparse's help defines CLI options.

    Raises:
        SystemExit: Help/argument parsing exits, a release error exits with
            status 1, or keyboard interruption exits with status 130.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ref", help="Commit/ref merged into remote main; defaults to freshly fetched main")
    parser.add_argument("--repo", default="OpenCyphal/CETL")
    parser.add_argument("--remote", default="origin")
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--notes-file", type=Path, help="Reviewed release notes; otherwise generate notes from GitHub")
    parser.add_argument("--dry-run", action="store_true", help="Perform preflight reads, without remote writes")
    parser.add_argument("--timeout", type=int, default=7200, help="Seconds to wait for each CI/deployment phase")
    parser.add_argument("--poll", type=int, default=15, help="Seconds between GitHub status reads")
    args = parser.parse_args()
    if args.timeout < 1 or args.poll < 1:
        parser.error("--timeout and --poll must be positive")
    try:
        cut_release(args)
    except KeyboardInterrupt:
        parser.exit(130, "Release interrupted. Use the same release command to continue.\n")
    except (ReleaseError, OSError, ValueError, KeyError) as ex:
        parser.exit(1, f"Release stopped: {ex}\nFix the reported problem and retry; existing tags/assets are preserved.\n")


if __name__ == "__main__":
    main()
