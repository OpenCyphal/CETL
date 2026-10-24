"""Test release contracts and rehearse publication against disposable Git repositories.

Pure checks exercise version, transport, CI-gate, and documentation contracts.
Repository rehearsals use real Git subprocesses and a local bare remote, with
SimulatedGitHub supplying API state and failure injection. GitHub/network access
is replaced by fixtures or mocks; temporary repositories and assets are cleaned
up by unittest. Run the suite through unittest discovery on Python 3.14+.
"""

import argparse
import ast
import contextlib
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
from typing import Any
import unittest
from unittest.mock import patch

SCRIPTS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS))

from release import GitHub, check_local_tools, cut_release, require_latest_stable  # noqa: E402
from release_tools import (Git, HEADER, RELEASE_FILES, ReleaseError, check_bundle, check_documentation, check_run, command,  # noqa: E402
                           header_version, package, parse_version, repository_from_url,
                           required_jobs, sha256, validate)

HEADER_TEXT = "#define CETL_VERSION_MAJOR 2\n#define CETL_VERSION_MINOR 0\n#define CETL_VERSION_PATCH 0\n"


def _workflow_group(pattern: str, workflow: str, flags: int = 0) -> str:
    """Extract a required workflow field, failing explicitly when its pattern is absent."""
    match = re.search(pattern, workflow, flags)
    if match is None:
        raise AssertionError(f"Required workflow pattern did not match: {pattern}")
    return match.group(1)


class VersionTests(unittest.TestCase):
    """Check the accepted version grammar, header definitions, and remote identities."""

    def test_stable_and_prerelease(self):
        """Accept matching stable versions and each supported prerelease stage."""
        self.assertEqual(header_version(HEADER_TEXT), "2.0.0")
        self.assertFalse(validate(HEADER_TEXT, "v2.0.0", False)["prerelease"])
        for stage in ("alpha", "beta", "rc"):
            self.assertTrue(validate(HEADER_TEXT, f"v2.0.0-{stage}.1", True)["prerelease"])

    def test_invalid_versions(self):
        """Reject ambiguous components, unsupported suffixes, and malformed version text."""
        for version in ("02.0.0", "2.00.0", "2.0", "v2.0.0", "2.0.0-rc.0", "2.0.0-rc.01",
                        "2.0.0-dev.1", "2.0.0+build", "2.0.0\n", "-n", "2.0.0-rc.1/branch"):
            with self.subTest(version=version), self.assertRaises(ReleaseError):
                parse_version(version)

    def test_bad_header_definitions(self):
        """Require each version macro exactly once as a bare decimal integer."""
        for header in (HEADER_TEXT.replace("#define CETL_VERSION_MAJOR 2\n", ""),
                       HEADER_TEXT + "#define CETL_VERSION_MAJOR 2\n",
                       HEADER_TEXT.replace("MAJOR 2", "MAJOR (2)"),
                       HEADER_TEXT.replace("MAJOR 2", "MAJOR 02"),
                       HEADER_TEXT.replace("MAJOR 2", "MAJOR -2"),
                       HEADER_TEXT.replace("MAJOR 2", "MAJOR 2 garbage")):
            with self.subTest(header=header), self.assertRaises(ReleaseError):
                header_version(header)

    def test_misaligned_tags_and_flags(self):
        """Reject tag/header disagreements and inconsistent prerelease flags."""
        for tag, flag in (("v2.0.1", None), ("2.0.0", None), ("v2.0.0", True), ("v2.0.0-rc.1", False)):
            with self.subTest(tag=tag, flag=flag), self.assertRaises(ReleaseError):
                validate(HEADER_TEXT, tag, flag)

    def test_github_remote_formats(self):
        """Accept supported GitHub remotes and reject credentials or another host."""
        for url in ("https://github.com/OpenCyphal/CETL.git", "git@github.com:OpenCyphal/CETL.git",
                    "ssh://git@github.com/OpenCyphal/CETL", "https://github.com/OpenCyphal/CETL/"):
            self.assertEqual(repository_from_url(url), "OpenCyphal/CETL")
        for url in ("https://token@github.com/OpenCyphal/CETL", "https://github.com.evil/CETL",
                    "/tmp/origin.git", "https://github.com/OpenCyphal/CETL?token=secret"):
            with self.assertRaises(ReleaseError):
                repository_from_url(url)


def successful_run(commit: str, tag: str) -> dict[str, Any]:
    """Build a successful tagged-push Actions record for gate tests.

    Args:
        commit (str): Source object ID exposed as head_sha.
        tag (str): Exact release tag exposed as head_branch.

    Returns:
        dict: Mutable run fixture identifying CETLVaSt, attempt 1, and run 123.
    """
    return {"id": 123, "run_attempt": 1, "head_sha": commit, "head_branch": tag,
            "event": "push", "path": ".github/workflows/cetlvast.yml",
            "status": "completed", "conclusion": "success", "html_url": "https://example.test/runs/123"}


class GateTests(unittest.TestCase):
    """Check that release gates require the exact workflow, revision, and job set."""

    def setUp(self):
        """Create an accepted run and one successful record for every required job."""
        self.workflow_run = successful_run("a" * 40, "v2.0.0")
        self.jobs = [{"name": name, "conclusion": "success"} for name in sorted(required_jobs())]

    def check(self):
        """Validate the mutable run/job fixtures against the fixed release identity."""
        check_run(self.workflow_run, self.jobs, "a" * 40, "v2.0.0")

    def test_success(self):
        """Accept a successful run containing every required release job."""
        self.check()

    def test_missing_or_unsuccessful_job(self):
        """Reject a missing job and every non-successful required-job conclusion."""
        for result in (None, "skipped", "failure", "cancelled", "timed_out"):
            with self.subTest(result=result):
                jobs = self.jobs[:-1] if result is None else [*self.jobs[:-1], {**self.jobs[-1], "conclusion": result}]
                with self.assertRaises(ReleaseError):
                    check_run(self.workflow_run, jobs, "a" * 40, "v2.0.0")

    def test_wrong_commit_event_workflow_tag_or_state(self):
        """Reject a run whose identity or completion state differs from the release."""
        for key, value in (("head_sha", "b" * 40), ("event", "pull_request"), ("head_branch", "main"),
                           ("path", ".github/workflows/other.yml"), ("status", "in_progress"),
                           ("conclusion", "cancelled")):
            with self.subTest(key=key), self.assertRaises(ReleaseError):
                check_run({**self.workflow_run, key: value}, self.jobs, "a" * 40, "v2.0.0")

    def test_matrix_and_gate_contract_matches_workflow(self):
        """Keep required job names and aggregate dependencies aligned with CETLVaSt."""
        # Parse only the inline arrays we own; no third-party YAML dependency is needed by the tooling.
        workflow = (SCRIPTS.parent / ".github/workflows/cetlvast.yml").read_text()
        for architecture in ("amd64", "arm64", "legacy-arm64"):
            section = _workflow_group(
                rf"^  verification-{architecture}:\n(.*?)(?=^  [a-z]|\Z)", workflow, re.M | re.S
            )
            compilers = ast.literal_eval(_workflow_group(r"toolchain: (\[.*\])", section))
            standards = ast.literal_eval(_workflow_group(r"standard: (\[.*\])", section))
            expected = {f"verification-{architecture} ({compiler}, {standard})"
                        for compiler in compilers for standard in standards}
            self.assertEqual(expected, {name for name in required_jobs() if name.startswith(f"verification-{architecture} (")})
            self.assertIn("startsWith(github.ref, 'refs/tags/v')", section)
            self.assertNotIn("!inputs.docs_ref", section)
        gate = _workflow_group(r"^  release-ready:\n(.*?)(?=^  [a-z])", workflow, re.M | re.S)
        dependencies = _workflow_group(r"needs: \[(.*)\]", gate).split(", ")
        self.assertEqual(set(dependencies), {"version", "release-tests", "cache-warmup", "docs",
                                            "verification-amd64", "verification-arm64", "verification-legacy-arm64"})
        self.assertIn("job['result'] != 'success'", gate)


class GitHubTransportTests(unittest.TestCase):
    """Check CLI/API encoding, local prerequisites, diagnostics, and public HTML reads."""

    def test_local_tools_require_python_314(self):
        """Reject unsupported Python versions before launching any subprocess."""
        for version in ((3, 10), (3, 13)):
            with self.subTest(version=version), patch("release.command") as execute:
                with self.assertRaisesRegex(ReleaseError, r"Python 3\.14"):
                    check_local_tools(version)
                execute.assert_not_called()

    def test_old_github_cli_prints_upgrade_instruction(self):
        """Report an upgrade action when a required GitHub CLI operation is unavailable."""
        with patch("release.command", side_effect=["git version", "gh version", ReleaseError("unsupported")]):
            with self.assertRaisesRegex(ReleaseError, "Update GitHub CLI"):
                check_local_tools((3, 14))

    def test_missing_tool_prints_install_instruction(self):
        """Report installation guidance when the GitHub CLI executable is missing."""
        with patch("release_tools.subprocess.run", side_effect=FileNotFoundError):
            with self.assertRaisesRegex(ReleaseError, "Install GitHub CLI"):
                command(["gh", "--version"])

    def test_api_error_explains_authentication_without_echoing_credentials(self):
        """Replace transport diagnostics with authentication guidance that omits secrets."""
        with patch("release.command", side_effect=ReleaseError("secret-token")):
            with self.assertRaisesRegex(ReleaseError, "gh auth login") as caught:
                GitHub("example/cetl").api("")
        self.assertNotIn("secret-token", str(caught.exception))

    def test_documentation_version_reads_visible_doxygen_number(self):
        """Match the displayed project number rather than incidental page text."""
        check_documentation('<span id="projectnumber">&#160;2.0.0</span>', "2.0.0")
        for html in ('<span id="projectnumber">1.0.0</span>', '<p>2.0.0 elsewhere in the page</p>'):
            with self.assertRaisesRegex(ReleaseError, "Documentation displays"):
                check_documentation(html, "2.0.0")

    def test_site_verification_fetches_actual_published_html(self):
        """Fetch public site HTML without attaching GitHub credentials."""
        github = GitHub("example/cetl")
        response = io.BytesIO(b'<span id="projectnumber">2.0.0</span>')
        with patch.object(github, "api", return_value={"html_url": "https://example.test/cetl/"}), \
                patch("release.urlopen", return_value=response) as fetch:
            with contextlib.redirect_stdout(io.StringIO()):
                github.verify_pages("2.0.0", 1, 1)
        self.assertEqual(fetch.call_args.args[0].full_url, "https://example.test/cetl/")
        self.assertIsNone(fetch.call_args.args[0].get_header("Authorization"))

    def test_wrong_published_site_version_fails_with_resume_instruction(self):
        """Report the displayed/expected version mismatch and a recovery action."""
        github = GitHub("example/cetl")
        response = io.BytesIO(b'<span id="projectnumber">1.0.0</span>')
        with patch.object(github, "api", return_value={"html_url": "https://example.test/cetl/"}), \
                patch("release.urlopen", return_value=response), \
                patch("release.time.monotonic", side_effect=[0, 0, 2]), patch("release.time.sleep"), \
                contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaisesRegex(ReleaseError, "Documentation displays 1.0.0, expected 2.0.0.*printed resume command"):
                github.verify_pages("2.0.0", 1, 1)

    def test_site_network_failure_is_distinguished_from_wrong_version(self):
        """Identify network access failures without exposing raw transport details."""
        github = GitHub("example/cetl")
        with patch.object(github, "api", return_value={"html_url": "https://example.test/cetl/"}), \
                patch("release.urlopen", side_effect=OSError("sensitive transport details")), \
                patch("release.time.monotonic", side_effect=[0, 0, 2]), patch("release.time.sleep"), \
                contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaisesRegex(ReleaseError, "check network/TLS access") as caught:
                github.verify_pages("2.0.0", 1, 1)
        self.assertNotIn("sensitive transport details", str(caught.exception))

    def test_cli_pins_host_and_api_encodes_booleans(self):
        """Scope commands to github.com and preserve API field values and boolean types."""
        github = GitHub("example/cetl")
        with patch("release.command", return_value="{}") as execute:
            github.cli("release", "upload", "v2.0.0", "/tmp/notes")
            self.assertEqual(execute.call_args.args[0][-2:], ["--repo", "github.com/example/cetl"])
            github.api("releases/1", "PATCH", draft=False, body="literal $() and `quotes`\nnext line")
            args = execute.call_args.args[0]
            self.assertIn("draft=false", args)
            self.assertIn("body=literal $() and `quotes`\nnext line", args)
            self.assertIn("github.com", args)

    def test_pagination_keeps_later_pages(self):
        """Collect records beyond the first full API page."""
        github = GitHub("example/cetl")
        with patch.object(github, "api", side_effect=[{"jobs": list(range(100))}, {"jobs": [100]}]):
            self.assertEqual(github.pages("actions/runs/123/jobs?filter=latest", "jobs"), list(range(101)))

    def test_permission_error_is_not_a_missing_release(self):
        """Propagate listing failures instead of treating inaccessible releases as absent."""
        github = GitHub("example/cetl")
        with patch.object(github, "api", side_effect=ReleaseError("Forbidden")):
            with self.assertRaisesRegex(ReleaseError, "Forbidden"):
                github.release("v2.0.0")

    def test_unexpected_metadata_response_stops_preflight(self):
        """Reject empty or non-object metadata before reading workflow configuration."""
        github = GitHub("example/cetl")
        for response in (None, [], "not an object"):
            with self.subTest(response=response), patch.object(github, "api", return_value=response):
                with self.assertRaisesRegex(ReleaseError, "returned no JSON object"):
                    github.preflight(stable=False)

    def test_malformed_collection_does_not_hide_missing_records(self):
        """Reject malformed collections instead of treating their records as absent."""
        github = GitHub("example/cetl")
        for response, key in ((None, None), ({}, None), ([], "jobs"), ({}, "jobs"),
                              ({"jobs": None}, "jobs"), ({"jobs": {}}, "jobs")):
            with self.subTest(response=response, key=key), patch.object(github, "api", return_value=response):
                with self.assertRaisesRegex(ReleaseError, "returned no JSON (object|array)"):
                    github.pages("actions/runs/123/jobs", key)

    def test_stable_version_comparison_uses_numeric_versions(self):
        """Order published stable versions numerically and ignore other release records."""
        github = GitHub("example/cetl")
        releases = [{"tag_name": tag, "draft": draft, "prerelease": prerelease} for tag, draft, prerelease in (
            ("v2.9.0", False, False), ("v2.10.0", False, False), ("v3.0.0", True, False),
            ("v4.0.0-rc.1", False, True), ("historical-non-semver", False, False))]
        with patch.object(github, "pages", return_value=releases):
            self.assertEqual(max(github.published_stable_versions()), (2, 10, 0))

    def test_subprocess_errors_do_not_echo_credentials(self):
        """Keep captured subprocess stderr out of reported command failures."""
        with patch("release_tools.subprocess.run", return_value=subprocess.CompletedProcess(
            ["gh"], 1, stdout=b"", stderr=b"secret-token"
        )):
            with self.assertRaises(ReleaseError) as caught:
                command(["gh", "api", "user"])
        self.assertNotIn("secret-token", str(caught.exception))


class LocalGit(Git):
    """Use a local bare remote while supplying the rehearsal's GitHub identity.

    Construction and Git operations follow ``Git``. Overriding only repository
    identity lets release checks use real commits, refs, archives, and pushes
    without a hosted repository.
    """

    def remote_repository(self, remote):
        """Return example/cetl for the fixture remote, bypassing GitHub URL parsing."""
        return "example/cetl"


class SimulatedGitHub(GitHub):
    """Implement the release client's test interface with mutable in-memory state.

    Args:
        git (LocalGit): Fixture checkout used to build actual release archives.
        docs (Path): Fixture HTML directory supplied to ``package``.
        tag (str): Release tag shared by the fixture's run and release.

    Tests adjust run, release_data, docs_run, policy, and site_version fields to
    model remote state. asset_data maps filenames to uploaded bytes; writes
    records draft/upload/publish/dispatch events. fail_after_upload interrupts
    after the specified upload count, and fail_docs controls deployment failure.
    Unsupported API paths and CLI operations raise AssertionError.
    """

    def __init__(self, git, docs, tag):
        """Initialize an accessible repository with passing CI and usable Pages setup."""
        self.git, self.docs, self.tag = git, docs, tag
        super().__init__("example/cetl")
        self.run: dict[str, Any] | None = successful_run(git.commit("HEAD"), tag)
        self.release_data: dict[str, Any] | None = None
        self.asset_data: dict[str, bytes] = {}
        self.writes: list[str] = []
        self.fail_after_upload: int | None = None
        self.permissions = {"push": True}
        self.docs_run: dict[str, Any] | None = None
        self.fail_docs = False
        self.other_versions = set()
        self.workflow_state = "active"
        self.pages_type = "workflow"
        self.environment_policy = {"protected_branches": False, "custom_branch_policies": True}
        self.branch_policies = [{"name": "main", "type": "branch"}]
        self.main_protected = True
        self.site_version = "2.0.0"

    def preflight(self, stable):
        """Run the production preflight against simulated workflow and policy records."""
        return super().preflight(stable)

    def pages(self, path, key=None):
        """Supply the configured deployment branch policies for preflight checks."""
        if path == "environments/github-pages/deployment-branch-policies":
            return self.branch_policies
        raise AssertionError(path)

    def verify_pages(self, version, timeout, poll):
        """Check site_version through the real HTML validator without polling."""
        check_documentation(f'<span id="projectnumber">{self.site_version}</span>', version)

    def published_stable_versions(self):
        """Combine other_versions with the fixture release's core when published stable."""
        versions = set(self.other_versions)
        if self.release_data and not self.release_data["draft"] and not self.release_data["prerelease"]:
            versions.add(tuple(map(int, self.tag[1:].split("."))))
        return versions

    def api(self, path, method="GET", **fields):
        """Return configuration records or apply and record the fixture's publish PATCH."""
        if path == "":
            return {"permissions": self.permissions, "default_branch": "main"}
        if path == "actions/workflows/cetlvast.yml":
            return {"state": self.workflow_state}
        if path == "pages":
            return {"build_type": self.pages_type, "html_url": "https://example.test/cetl/"}
        if path == "environments/github-pages":
            return {"deployment_branch_policy": self.environment_policy}
        if path == "branches/main":
            return {"protected": self.main_protected}
        if path == "releases/1" and method == "PATCH":
            assert self.release_data is not None
            self.writes.append("publish")
            self.release_data["draft"] = fields["draft"]
            return self.release_data
        raise AssertionError((path, method, fields))

    def release(self, tag):
        """Return release_data; each fixture represents a single release identity."""
        return self.release_data

    def runs(self, **filters):
        """Return the docs run for dispatch queries, or the fixture's tag run otherwise."""
        if filters.get("event") == "workflow_dispatch":
            return [self.docs_run] if self.docs_run else []
        return [self.run] if self.run else []

    def jobs(self, run):
        """Supply a deployment result for docs_run, or a complete successful gate."""
        if run is self.docs_run:
            return [{"name": "deploy-docs", "conclusion": "failure" if self.fail_docs else "success"}]
        return [{"name": name, "conclusion": "success"} for name in required_jobs()]

    def assets(self, release):
        """Expose stored asset bytes as uploaded records with sizes and SHA-256 digests."""
        return [{"name": name, "state": "uploaded", "size": len(data),
                 "digest": "sha256:" + hashlib.sha256(data).hexdigest()} for name, data in self.asset_data.items()]

    def asset_matches(self, asset, path):
        """Delegate byte/digest comparison to the production client implementation."""
        return super().asset_matches(asset, path)

    def create_draft(self, tag, commit, prerelease, marker, run, notes=None):
        """Record draft creation and retain the classification and management marker."""
        self.writes.append("draft")
        self.release_data = {"id": 1, "draft": True, "tag_name": tag, "prerelease": prerelease, "body": marker}

    def cli(self, *args) -> str:
        """Simulate downloads, immutable uploads, and workflow dispatches.

        Run downloads package the fixture's real Git tree; release downloads
        write saved asset bytes. Uploads record bytes before optional failure
        injection, reproducing an interrupted operation's partial remote state.
        Main dispatches create a completed docs run controlled by fail_docs.
        """
        if args[:2] == ("run", "download"):
            assert self.run is not None
            package(self.git, self.run["head_sha"], self.tag, self.docs, Path(args[args.index("--dir") + 1]))
        elif args[:2] == ("release", "upload"):
            path = Path(args[3])
            if path.name in self.asset_data:
                raise AssertionError("Attempted to overwrite an asset")
            self.asset_data[path.name] = path.read_bytes()
            self.writes.append("upload")
            if self.fail_after_upload == len(self.asset_data):
                raise ReleaseError("Simulated interrupted upload")
        elif args[:2] == ("release", "download"):
            directory = Path(args[args.index("--dir") + 1])
            for name, data in self.asset_data.items():
                (directory / name).write_bytes(data)
        elif args[:2] == ("workflow", "run"):
            self.writes.append("dispatch")
            if args[args.index("--ref") + 1] == "main":
                self.docs_run = {"id": (self.docs_run or {"id": 200})["id"] + 1, "display_title": f"CETLVaSt / {self.tag}",
                                 "status": "completed", "conclusion": "failure" if self.fail_docs else "success",
                                 "html_url": "https://example.test/docs"}
        else:
            raise AssertionError(args)
        return ""


class RepositoryTests(unittest.TestCase):
    """Rehearse publication and recovery with real Git and a simulated GitHub service."""

    def setUp(self):
        """Create a clean main checkout, bare remote, tooling files, and release options.

        Register repository cleanup and replace local executable preflight; its
        behavior is tested separately by GitHubTransportTests.
        """
        tools = patch("release.check_local_tools")
        tools.start()
        self.addCleanup(tools.stop)
        self.temporary = tempfile.TemporaryDirectory(prefix="cetl-rehearsal-")
        self.addCleanup(self.temporary.cleanup)
        root = Path(self.temporary.name)
        self.root = root / "checkout"
        self.root.mkdir()
        self.git = LocalGit(self.root)
        self.git.run("init", "--initial-branch=main")
        self.git.run("config", "user.name", "CETL release rehearsal")
        self.git.run("config", "user.email", "release@example.test")
        (self.root / HEADER).parent.mkdir(parents=True)
        (self.root / HEADER).write_text(HEADER_TEXT)
        (self.root / "LICENSE").write_text("Fixture license\n")
        (self.root / "README.md").write_text("Fixture source\n")
        for path in RELEASE_FILES:
            destination = self.root / path
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(SCRIPTS.parent / path, destination)
        self.git.run("add", ".")
        self.git.run("commit", "-m", "Release fixture")
        command(["git", "init", "--bare", "--initial-branch=main", str(root / "remote.git")])
        self.git.run("remote", "add", "origin", str(root / "remote.git"))
        self.git.run("push", "origin", "main")
        self.commit = self.git.commit("HEAD")
        self.docs = root / "html"
        self.docs.mkdir()
        (self.docs / "index.html").write_text("<html>Fixture built documentation</html>")
        self.args = argparse.Namespace(version="2.0.0", ref=self.commit, repo="example/cetl", remote="origin",
                                       root=self.root, notes_file=None, dry_run=False, resume=False, timeout=1, poll=1)
        self.github = SimulatedGitHub(self.git, self.docs, "v2.0.0")

    def cut(self):
        """Run the release with mutable fixture options and return captured progress text."""
        with contextlib.redirect_stdout(io.StringIO()) as output:
            cut_release(self.args, self.git, self.github)
        return output.getvalue()

    def add_tag(self, tag="v2.0.0"):
        """Create a lightweight tag at fixture HEAD and push it to the bare remote."""
        self.git.run("tag", tag)
        self.git.run("push", "origin", f"refs/tags/{tag}")

    def test_full_release_rehearsal_and_published_resume(self):
        """Publish once and verify a repeated matching attempt performs no remote writes."""
        self.add_tag()
        self.git.run("tag", "-d", "v2.0.0")  # Cutting a release must not require a local tag.
        self.args.resume = True
        # The simulated CI checkout has the tag, just as actions/checkout fetch-depth: 0 does.
        self.git.run("fetch", "--tags", "origin")
        output = self.cut()
        self.assertIn("Documentation:", output)
        assert self.github.release_data is not None
        self.assertFalse(self.github.release_data["draft"])
        self.assertEqual(self.git.remote_tag("origin", "v2.0.0"), self.commit)
        writes = list(self.github.writes)
        self.cut()
        self.assertEqual(self.github.writes, writes)

    def test_new_tag_push_and_prerelease(self):
        """Push and publish a prerelease while retaining the stable documentation site."""
        self.args.version = "2.0.0-rc.1"
        self.github.tag = "v" + self.args.version
        assert self.github.run is not None
        self.github.run["head_branch"] = self.github.tag
        # CI receives the pushed tag in its separate checkout. Simulate that fetch just before packaging.
        original_cli = self.github.cli
        def cli(*args):
            """Fetch the pushed tag before emulating CI's separate packaging checkout."""
            if args[:2] == ("run", "download"):
                self.git.run("fetch", "--tags", "origin")
            return original_cli(*args)
        self.github.cli = cli
        self.cut()
        self.assertEqual(self.git.remote_tag("origin", self.github.tag), self.commit)
        assert self.github.release_data is not None
        self.assertTrue(self.github.release_data["prerelease"])
        self.assertNotIn("dispatch", self.github.writes)

    def test_dry_run_has_no_remote_writes(self):
        """Complete preflight without pushing a tag or changing simulated GitHub state."""
        self.args.dry_run = True
        self.assertIn("No remote writes", self.cut())
        self.assertFalse(self.github.writes)
        self.assertIsNone(self.git.remote_tag("origin", "v2.0.0"))

    def test_defaults_fetch_main_and_derive_header_version(self):
        """Pin commands to freshly fetched main even when the local checkout is older."""
        (self.root / HEADER).write_text(HEADER_TEXT.replace("MINOR 0", "MINOR 1"))
        self.git.run("add", ".")
        self.git.run("commit", "-m", "Prepare version 2.1.0")
        new_commit = self.git.commit("HEAD")
        self.git.run("push", "origin", "main")
        self.git.run("checkout", "--detach", self.commit)
        self.args.ref, self.args.version, self.args.dry_run = None, None, True
        output = self.cut()
        self.assertIn("Tag: v2.1.0", output)
        self.assertIn("Commit: " + new_commit, output)
        self.assertIn("--ref " + new_commit, output)
        self.assertIn("Release this checked commit:", output)
        self.assertFalse(self.github.writes)
        self.assertIsNone(self.git.remote_tag("origin", "v2.1.0"))

    def test_missing_workflow_on_main_stops_before_tag_creation(self):
        """Reject a main commit missing release tooling before publishing any tag."""
        self.git.run("rm", ".github/workflows/cetlvast.yml")
        self.git.run("commit", "-m", "Remove workflow")
        self.git.run("push", "origin", "main")
        self.args.ref = self.git.commit("HEAD")
        with self.assertRaisesRegex(ReleaseError, "Merge the release tooling"):
            self.cut()
        self.assertIsNone(self.git.remote_tag("origin", "v2.0.0"))

    def test_disabled_workflow_stops_before_tag_creation(self):
        """Reject disabled CETLVaSt with an enable command before pushing a tag."""
        self.github.workflow_state = "disabled_manually"
        with self.assertRaisesRegex(ReleaseError, "gh workflow enable"):
            self.cut()
        self.assertIsNone(self.git.remote_tag("origin", "v2.0.0"))

    def test_pages_must_use_actions_before_publishing_stable(self):
        """Reject a stable release when Pages uses the legacy build source."""
        self.github.pages_type = "legacy"
        with self.assertRaisesRegex(ReleaseError, "Pages build source"):
            self.cut()
        self.assertIsNone(self.git.remote_tag("origin", "v2.0.0"))

    def test_tag_only_pages_policy_does_not_allow_main(self):
        """Require branch deployment permission even when tag deployment is unrestricted."""
        self.github.branch_policies = [{"name": "*", "type": "tag"}]
        with self.assertRaisesRegex(ReleaseError, "Allow main"):
            self.cut()
        self.assertFalse(self.github.writes)

    def test_protected_branch_pages_policy_rejects_unprotected_main(self):
        """Reject an unprotected main under a protected-branches-only Pages policy."""
        self.github.environment_policy = {"protected_branches": True, "custom_branch_policies": False}
        self.github.main_protected = False
        with self.assertRaisesRegex(ReleaseError, "Allow main"):
            self.cut()

    def test_prerelease_does_not_require_pages_configuration(self):
        """Allow prerelease preflight without a stable Pages deployment configuration."""
        self.args.version, self.args.dry_run = "2.0.0-rc.1", True
        self.github.pages_type = "legacy"
        self.github.branch_policies = []
        self.cut()
        self.assertFalse(self.github.writes)

    def test_authentication_failure(self):
        """Stop on inaccessible repository metadata before creating remote release state."""
        with patch.object(self.github, "api", side_effect=ReleaseError("Authentication failed")):
            with self.assertRaisesRegex(ReleaseError, "Authentication"):
                self.cut()
        self.assertIsNone(self.git.remote_tag("origin", "v2.0.0"))

    def test_read_only_credentials(self):
        """Refuse publication when repository permissions lack push access."""
        self.github.permissions = {"push": False}
        with self.assertRaisesRegex(ReleaseError, "write access"):
            self.cut()
        self.assertFalse(self.github.writes)

    def test_dirty_checkout(self):
        """Refuse release preparation when the checkout contains an untracked file."""
        (self.root / "untracked").write_text("untracked")
        with self.assertRaisesRegex(ReleaseError, "clean"):
            self.cut()

    def test_wrong_repository(self):
        """Reject a requested repository that disagrees with the Git remote identity."""
        self.args.repo = "other/cetl"
        with self.assertRaisesRegex(ReleaseError, "does not match"):
            self.cut()

    def test_unmerged_commit(self):
        """Require the selected commit to be an ancestor of remote main."""
        self.git.run("switch", "-c", "unmerged")
        (self.root / "new").write_text("unmerged")
        self.git.run("add", ".")
        self.git.run("commit", "-m", "Not merged")
        self.args.ref = self.git.commit("HEAD")
        with self.assertRaises(ReleaseError):
            self.cut()
        self.assertFalse(self.github.writes)

    def test_mismatched_version_never_pushes(self):
        """Reject a version differing from the selected header before pushing its tag."""
        self.args.version = "2.0.1"
        with self.assertRaisesRegex(ReleaseError, "does not match"):
            self.cut()
        self.assertIsNone(self.git.remote_tag("origin", "v2.0.1"))

    def test_conflicting_remote_tag(self):
        """Preserve an existing remote tag when the requested source commit differs."""
        self.add_tag()
        (self.root / "new").write_text("new")
        self.git.run("add", ".")
        self.git.run("commit", "-m", "Next commit")
        self.git.run("push", "origin", "main")
        self.args.ref = self.git.commit("HEAD")
        self.args.resume = True
        with self.assertRaisesRegex(ReleaseError, "another commit"):
            self.cut()
        self.assertEqual(self.git.remote_tag("origin", "v2.0.0"), self.commit)

    def test_existing_tag_requires_resume(self):
        """Require explicit resume intent for an existing matching tag."""
        self.add_tag()
        with self.assertRaisesRegex(ReleaseError, "--resume"):
            self.cut()

    def test_dry_run_for_existing_tag_prints_a_working_resume_command(self):
        """Include resume intent and the exact commit in an existing-tag dry-run command."""
        self.add_tag()
        self.args.dry_run = True
        output = self.cut()
        release_line = next(line for line in output.splitlines() if line.startswith("Release this checked commit:"))
        self.assertIn("--resume", release_line)
        self.assertIn("--ref " + self.commit, release_line)
        self.assertFalse(self.github.writes)

    def test_resume_requires_pinned_version_and_commit(self):
        """Reject resume options that could follow an advancing main branch."""
        self.args.resume, self.args.ref = True, None
        with self.assertRaisesRegex(ReleaseError, "printed Resume command"):
            self.cut()
        self.assertIsNone(self.git.remote_tag("origin", "v2.0.0"))

    def test_failed_ci_cannot_create_draft_or_publish(self):
        """Block draft creation and publication when tagged CI fails."""
        self.add_tag()
        self.args.resume = True
        assert self.github.run is not None
        self.github.run["conclusion"] = "failure"
        with self.assertRaisesRegex(ReleaseError, "CI"):
            self.cut()
        self.assertFalse(self.github.writes)

    def test_unrelated_draft_cannot_be_adopted(self):
        """Refuse a draft lacking the matching script-management marker."""
        self.add_tag()
        self.args.resume = True
        self.github.release_data = {"tag_name": "v2.0.0", "prerelease": False, "body": "Manual draft", "draft": True}
        with self.assertRaisesRegex(ReleaseError, "script-managed"):
            self.cut()
        self.assertFalse(self.github.writes)

    def test_missing_release_after_draft_creation_stops_before_upload(self):
        """Refuse uploads when a draft creation attempt leaves no visible release."""
        self.add_tag()
        self.args.resume = True
        with patch.object(self.github, "create_draft"):
            with self.assertRaisesRegex(ReleaseError, "Release is missing after draft creation"):
                self.cut()
        self.assertNotIn("upload", self.github.writes)
        self.assertNotIn("publish", self.github.writes)

    def test_remote_tag_movement_while_waiting_prevents_publication(self):
        """Recheck the remote tag after CI and reject movement before draft creation."""
        self.add_tag()
        self.args.resume = True
        with patch.object(self.git, "remote_tag", side_effect=[self.commit, "b" * 40]):
            with self.assertRaisesRegex(ReleaseError, "changed"):
                self.cut()
        self.assertFalse(self.github.writes)

    def test_absent_ci_times_out_without_publication(self):
        """Dispatch missing tagged CI and stop without publication when waiting expires."""
        self.add_tag()
        self.args.resume = True
        self.github.run = None
        with patch("release.time.monotonic", side_effect=[0, 2]):
            with self.assertRaisesRegex(ReleaseError, "Timed out"):
                self.cut()
        self.assertEqual(self.github.writes, ["dispatch"])

    def test_interrupted_upload_resumes_without_overwrite(self):
        """Retain uploaded bytes and upload only missing assets on a matching retry."""
        self.add_tag()
        self.args.resume = True
        self.github.fail_after_upload = 2
        with self.assertRaisesRegex(ReleaseError, "interrupted"):
            self.cut()
        assert self.github.release_data is not None
        self.assertTrue(self.github.release_data["draft"])
        saved = dict(self.github.asset_data)
        self.github.fail_after_upload = None
        self.cut()
        assert self.github.release_data is not None
        self.assertFalse(self.github.release_data["draft"])
        for name, data in saved.items():
            self.assertEqual(self.github.asset_data[name], data)
        self.assertEqual(self.github.writes.count("upload"), 5)

    def test_conflicting_asset_prevents_publication(self):
        """Preserve conflicting remote bytes and leave the release as a draft."""
        self.add_tag()
        self.args.resume = True
        self.github.fail_after_upload = 1
        with self.assertRaises(ReleaseError):
            self.cut()
        name = next(iter(self.github.asset_data))
        self.github.asset_data[name] = b"wrong bytes"
        self.github.fail_after_upload = None
        with self.assertRaisesRegex(ReleaseError, "differs"):
            self.cut()
        assert self.github.release_data is not None
        self.assertTrue(self.github.release_data["draft"])
        self.assertEqual(self.github.asset_data[name], b"wrong bytes")

    def test_documentation_failure_can_recover_after_publication(self):
        """Retry failed documentation deployment without republishing the release."""
        self.add_tag()
        self.args.resume = True
        self.github.fail_docs = True
        with self.assertRaisesRegex(ReleaseError, "published, but documentation failed"):
            self.cut()
        assert self.github.release_data is not None
        self.assertFalse(self.github.release_data["draft"])
        self.github.fail_docs = False
        self.cut()
        self.assertEqual(self.github.writes.count("publish"), 1)
        self.assertEqual(self.github.writes.count("dispatch"), 2)

    def test_published_resume_works_without_ci_runs_or_artifacts(self):
        """Verify published assets after CI retention and the original notes file expire."""
        self.add_tag()
        self.args.resume = True
        self.cut()
        before = list(self.github.writes)
        self.github.run = None
        self.args.notes_file = Path(self.temporary.name) / "deleted-original-release-notes.md"
        self.cut()
        self.assertEqual(self.github.writes, before)

    def test_published_resume_detects_corrupt_actual_assets(self):
        """Reject corrupted published checksums without modifying remote release state."""
        self.add_tag()
        self.args.resume = True
        self.cut()
        self.github.asset_data["SHA256SUMS"] = b"corrupt"
        before = list(self.github.writes)
        with self.assertRaisesRegex(ReleaseError, "checksums"):
            self.cut()
        self.assertEqual(self.github.writes, before)

    def test_resume_reuses_pending_documentation_deployment(self):
        """Reuse an awaiting-approval deployment instead of dispatching another build."""
        self.add_tag()
        self.args.resume = True
        self.cut()
        assert self.github.docs_run is not None
        self.github.docs_run.update(status="waiting", conclusion=None)
        before = list(self.github.writes)
        with patch("release.time.monotonic", side_effect=[0, 2]):
            with self.assertRaisesRegex(ReleaseError, "documentation timed out"):
                self.cut()
        self.assertEqual(self.github.writes, before)

    def test_new_release_cannot_downgrade_or_prerelease_a_stable_version(self):
        """Require new stable and prerelease attempts to advance the published numeric core."""
        self.github.other_versions = {(2, 0, 0)}
        for version in ("2.0.0", "2.0.0-rc.1"):
            self.args.version = version
            with self.assertRaisesRegex(ReleaseError, "advance"):
                self.cut()
            self.assertIsNone(self.git.remote_tag("origin", "v" + version))

    def test_resume_old_release_does_not_roll_documentation_back(self):
        """Retain the newer stable site when an older published release is resumed."""
        self.add_tag()
        self.args.resume = True
        self.cut()
        before = list(self.github.writes)
        self.github.other_versions.add((2, 1, 0))
        self.github.docs_run = None
        self.assertIn("newer stable release", self.cut())
        self.assertEqual(self.github.writes, before)
        with self.assertRaisesRegex(ReleaseError, "latest published"):
            require_latest_stable(self.github, "v2.0.0")

    def test_unpublished_tag_cannot_deploy_stable_documentation(self):
        """Require a published stable release before allowing a Pages deployment."""
        self.add_tag()
        with self.assertRaisesRegex(ReleaseError, "latest published"):
            require_latest_stable(self.github, "v2.0.0")

    def test_packaging_contents_reproducibility_and_corruption(self):
        """Verify archive layout, licenses, reproducible bytes, and manifest validation."""
        self.add_tag()
        first, second = Path(self.temporary.name) / "first", Path(self.temporary.name) / "second"
        for output in (first, second):
            package(self.git, self.commit, "v2.0.0", self.docs, output)
            self.assertEqual(len(check_bundle(output, self.commit, "v2.0.0")), 5)
        self.assertEqual({p.name: sha256(p) for p in first.iterdir()}, {p.name: sha256(p) for p in second.iterdir()})
        with tarfile.open(first / "cetl-2.0.0-headers.tar.gz") as archive:
            files = {member.name for member in archive if member.isfile()}
        self.assertEqual(files, {"cetl-2.0.0/LICENSE", "cetl-2.0.0/" + HEADER})
        with tarfile.open(first / "cetl-2.0.0-docs.tar.gz") as archive:
            self.assertIn("cetl-2.0.0/html/index.html", archive.getnames())
            self.assertIn("cetl-2.0.0/LICENSE", archive.getnames())
        (first / "release.json").write_text("{}")
        with self.assertRaisesRegex(ReleaseError, "metadata"):
            check_bundle(first, self.commit, "v2.0.0")

    def test_cli_validates_tagged_checkout_and_release_event(self):
        """Check release-event prerelease metadata through the actual validation CLI."""
        self.add_tag()
        event = Path(self.temporary.name) / "event.json"
        event.write_text(json.dumps({"release": {"tag_name": "v2.0.0", "prerelease": True}}))
        env = {**os.environ, "GITHUB_EVENT_NAME": "release", "GITHUB_EVENT_PATH": str(event)}
        result = subprocess.run([sys.executable, str(SCRIPTS / "release_tools.py"), "validate"],
                                cwd=self.root, env=env, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("prerelease flag", result.stderr)
        event.write_text(json.dumps({"release": {"tag_name": "v2.0.0", "prerelease": False}}))
        result = subprocess.run([sys.executable, str(SCRIPTS / "release_tools.py"), "validate"],
                                cwd=self.root, env=env, capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)


    def test_bundle_checksums_and_extra_files_are_rejected(self):
        """Reject a changed archive digest and unexpected files in a release bundle."""
        self.add_tag()
        output = Path(self.temporary.name) / "bundle"
        package(self.git, self.commit, "v2.0.0", self.docs, output)
        archive = output / "cetl-2.0.0-headers.tar.gz"
        original = archive.read_bytes()
        archive.write_bytes(original + b"corruption")
        with self.assertRaisesRegex(ReleaseError, "checksums"):
            check_bundle(output, self.commit, "v2.0.0")
        archive.write_bytes(original)
        (output / "unexpected").write_text("extra")
        with self.assertRaisesRegex(ReleaseError, "unexpected"):
            check_bundle(output, self.commit, "v2.0.0")


if __name__ == "__main__":
    unittest.main()
