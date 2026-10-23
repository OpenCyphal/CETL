"""Release contract tests and local Git rehearsal; GitHub is simulated, never contacted."""

import argparse
import ast
import contextlib
import hashlib
import io
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

SCRIPTS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS))

from release import GitHub, cut_release, require_latest_stable  # noqa: E402
from release_tools import (Git, HEADER, ReleaseError, check_bundle, check_run, command,  # noqa: E402
                           header_version, package, parse_version, repository_from_url,
                           required_jobs, sha256, validate)

HEADER_TEXT = "#define CETL_VERSION_MAJOR 2\n#define CETL_VERSION_MINOR 0\n#define CETL_VERSION_PATCH 0\n"


class VersionTests(unittest.TestCase):
    def test_stable_and_prerelease(self):
        self.assertEqual(header_version(HEADER_TEXT), "2.0.0")
        self.assertFalse(validate(HEADER_TEXT, "v2.0.0", False)["prerelease"])
        for stage in ("alpha", "beta", "rc"):
            self.assertTrue(validate(HEADER_TEXT, f"v2.0.0-{stage}.1", True)["prerelease"])

    def test_invalid_versions(self):
        for version in ("02.0.0", "2.00.0", "2.0", "v2.0.0", "2.0.0-rc.0", "2.0.0-rc.01",
                        "2.0.0-dev.1", "2.0.0+build", "2.0.0\n", "-n", "2.0.0-rc.1/branch"):
            with self.subTest(version=version), self.assertRaises(ReleaseError):
                parse_version(version)

    def test_bad_header_definitions(self):
        for header in (HEADER_TEXT.replace("#define CETL_VERSION_MAJOR 2\n", ""),
                       HEADER_TEXT + "#define CETL_VERSION_MAJOR 2\n",
                       HEADER_TEXT.replace("MAJOR 2", "MAJOR (2)"),
                       HEADER_TEXT.replace("MAJOR 2", "MAJOR 02"),
                       HEADER_TEXT.replace("MAJOR 2", "MAJOR -2"),
                       HEADER_TEXT.replace("MAJOR 2", "MAJOR 2 garbage")):
            with self.subTest(header=header), self.assertRaises(ReleaseError):
                header_version(header)

    def test_misaligned_tags_and_flags(self):
        for tag, flag in (("v2.0.1", None), ("2.0.0", None), ("v2.0.0", True), ("v2.0.0-rc.1", False)):
            with self.subTest(tag=tag, flag=flag), self.assertRaises(ReleaseError):
                validate(HEADER_TEXT, tag, flag)

    def test_github_remote_formats(self):
        for url in ("https://github.com/OpenCyphal/CETL.git", "git@github.com:OpenCyphal/CETL.git",
                    "ssh://git@github.com/OpenCyphal/CETL", "https://github.com/OpenCyphal/CETL/"):
            self.assertEqual(repository_from_url(url), "OpenCyphal/CETL")
        for url in ("https://token@github.com/OpenCyphal/CETL", "https://github.com.evil/CETL",
                    "/tmp/origin.git", "https://github.com/OpenCyphal/CETL?token=secret"):
            with self.assertRaises(ReleaseError):
                repository_from_url(url)


def successful_run(commit, tag):
    return {"id": 123, "run_attempt": 1, "head_sha": commit, "head_branch": tag,
            "event": "push", "path": ".github/workflows/cetlvast.yml",
            "status": "completed", "conclusion": "success", "html_url": "https://example.test/runs/123"}


class GateTests(unittest.TestCase):
    def setUp(self):
        self.run = successful_run("a" * 40, "v2.0.0")
        self.jobs = [{"name": name, "conclusion": "success"} for name in sorted(required_jobs())]

    def check(self):
        check_run(self.run, self.jobs, "a" * 40, "v2.0.0")

    def test_success(self):
        self.check()

    def test_missing_or_unsuccessful_job(self):
        for result in (None, "skipped", "failure", "cancelled", "timed_out"):
            with self.subTest(result=result):
                jobs = self.jobs[:-1] if result is None else [*self.jobs[:-1], {**self.jobs[-1], "conclusion": result}]
                with self.assertRaises(ReleaseError):
                    check_run(self.run, jobs, "a" * 40, "v2.0.0")

    def test_wrong_commit_event_workflow_tag_or_state(self):
        for key, value in (("head_sha", "b" * 40), ("event", "pull_request"), ("head_branch", "main"),
                           ("path", ".github/workflows/other.yml"), ("status", "in_progress"),
                           ("conclusion", "cancelled")):
            with self.subTest(key=key), self.assertRaises(ReleaseError):
                check_run({**self.run, key: value}, self.jobs, "a" * 40, "v2.0.0")

    def test_matrix_and_gate_contract_matches_workflow(self):
        # Parse only the inline arrays we own; no third-party YAML dependency is needed by the tooling.
        workflow = (SCRIPTS.parent / ".github/workflows/cetlvast.yml").read_text()
        for architecture in ("amd64", "arm64", "legacy-arm64"):
            section = re.search(
                rf"^  verification-{architecture}:\n(.*?)(?=^  [a-z]|\Z)", workflow, re.M | re.S
            ).group(1)
            compilers = ast.literal_eval(re.search(r"toolchain: (\[.*\])", section).group(1))
            standards = ast.literal_eval(re.search(r"standard: (\[.*\])", section).group(1))
            expected = {f"verification-{architecture} ({compiler}, {standard})"
                        for compiler in compilers for standard in standards}
            self.assertEqual(expected, {name for name in required_jobs() if name.startswith(f"verification-{architecture} (")})
            self.assertIn("startsWith(github.ref, 'refs/tags/v')", section)
            self.assertNotIn("!inputs.docs_ref", section)
        gate = re.search(r"^  release-ready:\n(.*?)(?=^  [a-z])", workflow, re.M | re.S).group(1)
        dependencies = re.search(r"needs: \[(.*)\]", gate).group(1).split(", ")
        self.assertEqual(set(dependencies), {"version", "release-tests", "cache-warmup", "docs",
                                            "verification-amd64", "verification-arm64", "verification-legacy-arm64"})
        self.assertIn("job['result'] != 'success'", gate)


class GitHubTransportTests(unittest.TestCase):
    def test_cli_pins_host_and_api_encodes_booleans(self):
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
        github = GitHub("example/cetl")
        with patch.object(github, "api", side_effect=[{"jobs": list(range(100))}, {"jobs": [100]}]):
            self.assertEqual(github.pages("actions/runs/123/jobs?filter=latest", "jobs"), list(range(101)))

    def test_permission_error_is_not_a_missing_release(self):
        github = GitHub("example/cetl")
        with patch.object(github, "api", side_effect=ReleaseError("Forbidden")):
            with self.assertRaisesRegex(ReleaseError, "Forbidden"):
                github.release("v2.0.0")

    def test_stable_version_comparison_uses_numeric_versions(self):
        github = GitHub("example/cetl")
        releases = [{"tag_name": tag, "draft": draft, "prerelease": prerelease} for tag, draft, prerelease in (
            ("v2.9.0", False, False), ("v2.10.0", False, False), ("v3.0.0", True, False),
            ("v4.0.0-rc.1", False, True), ("historical-non-semver", False, False))]
        with patch.object(github, "pages", return_value=releases):
            self.assertEqual(max(github.published_stable_versions()), (2, 10, 0))

    def test_subprocess_errors_do_not_echo_credentials(self):
        with patch("release_tools.subprocess.run", return_value=subprocess.CompletedProcess(
            ["gh"], 1, stdout=b"", stderr=b"secret-token"
        )):
            with self.assertRaises(ReleaseError) as caught:
                command(["gh", "api", "user"])
        self.assertNotIn("secret-token", str(caught.exception))


class LocalGit(Git):
    """Only GitHub URL identity is simulated; all other Git operations use a local bare remote."""

    def remote_repository(self, remote):
        return "example/cetl"


class SimulatedGitHub:
    def __init__(self, git, docs, tag):
        self.git, self.docs, self.tag = git, docs, tag
        self.run = successful_run(git.commit("HEAD"), tag)
        self.release_data = None
        self.asset_data = {}
        self.writes = []
        self.fail_after_upload = None
        self.permissions = {"push": True}
        self.docs_run = None
        self.fail_docs = False
        self.other_versions = set()

    def published_stable_versions(self):
        versions = set(self.other_versions)
        if self.release_data and not self.release_data["draft"] and not self.release_data["prerelease"]:
            versions.add(tuple(map(int, self.tag[1:].split("."))))
        return versions

    def api(self, path, method="GET", **fields):
        if path == "":
            return {"permissions": self.permissions, "default_branch": "main"}
        if path == "releases/1" and method == "PATCH":
            self.writes.append("publish")
            self.release_data["draft"] = fields["draft"]
            return self.release_data
        raise AssertionError((path, method, fields))

    def release(self, tag):
        return self.release_data

    def runs(self, **filters):
        if filters.get("event") == "workflow_dispatch":
            return [self.docs_run] if self.docs_run else []
        return [self.run] if self.run else []

    def jobs(self, run):
        if run is self.docs_run:
            return [{"name": "deploy-docs", "conclusion": "failure" if self.fail_docs else "success"}]
        return [{"name": name, "conclusion": "success"} for name in required_jobs()]

    def assets(self, release):
        return [{"name": name, "state": "uploaded", "size": len(data),
                 "digest": "sha256:" + hashlib.sha256(data).hexdigest()} for name, data in self.asset_data.items()]

    def asset_matches(self, asset, path):
        return GitHub.asset_matches(self, asset, path)

    def create_draft(self, tag, commit, prerelease, marker, run, notes=None):
        self.writes.append("draft")
        self.release_data = {"id": 1, "draft": True, "tag_name": tag, "prerelease": prerelease, "body": marker}

    def cli(self, *args):
        if args[:2] == ("run", "download"):
            package(self.git, self.run["head_sha"], self.tag, self.docs, Path(args[args.index("--dir") + 1]))
        elif args[:2] == ("release", "upload"):
            path = Path(args[3])
            if path.name in self.asset_data:
                raise AssertionError("Attempted to overwrite an asset")
            self.asset_data[path.name] = path.read_bytes()
            self.writes.append("upload")
            if self.fail_after_upload == len(self.asset_data):
                raise ReleaseError("Simulated interrupted upload")
        elif args[:2] == ("workflow", "run"):
            self.writes.append("dispatch")
            if args[args.index("--ref") + 1] == "main":
                self.docs_run = {"id": (self.docs_run or {"id": 200})["id"] + 1, "display_title": f"CETLVaSt / {self.tag}",
                                 "status": "completed", "conclusion": "failure" if self.fail_docs else "success",
                                 "html_url": "https://example.test/docs"}
        else:
            raise AssertionError(args)


class RepositoryTests(unittest.TestCase):
    def setUp(self):
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
        with contextlib.redirect_stdout(io.StringIO()) as output:
            cut_release(self.args, self.git, self.github)
        return output.getvalue()

    def add_tag(self, tag="v2.0.0"):
        self.git.run("tag", tag)
        self.git.run("push", "origin", f"refs/tags/{tag}")

    def test_full_release_rehearsal_and_published_resume(self):
        self.add_tag()
        self.git.run("tag", "-d", "v2.0.0")  # Cutting a release must not require a local tag.
        self.args.resume = True
        # The simulated CI checkout has the tag, just as actions/checkout fetch-depth: 0 does.
        self.git.run("fetch", "--tags", "origin")
        output = self.cut()
        self.assertIn("Documentation:", output)
        self.assertFalse(self.github.release_data["draft"])
        self.assertEqual(self.git.remote_tag("origin", "v2.0.0"), self.commit)
        writes = list(self.github.writes)
        self.cut()
        self.assertEqual(self.github.writes, writes)

    def test_new_tag_push_and_prerelease(self):
        self.args.version = "2.0.0-rc.1"
        self.github.tag = "v" + self.args.version
        self.github.run["head_branch"] = self.github.tag
        # CI receives the pushed tag in its separate checkout. Simulate that fetch just before packaging.
        original_cli = self.github.cli
        def cli(*args):
            if args[:2] == ("run", "download"):
                self.git.run("fetch", "--tags", "origin")
            return original_cli(*args)
        self.github.cli = cli
        self.cut()
        self.assertEqual(self.git.remote_tag("origin", self.github.tag), self.commit)
        self.assertTrue(self.github.release_data["prerelease"])
        self.assertNotIn("dispatch", self.github.writes)

    def test_dry_run_has_no_remote_writes(self):
        self.args.dry_run = True
        self.assertIn("No remote writes", self.cut())
        self.assertFalse(self.github.writes)
        self.assertIsNone(self.git.remote_tag("origin", "v2.0.0"))

    def test_authentication_failure(self):
        with patch.object(self.github, "api", side_effect=ReleaseError("Authentication failed")):
            with self.assertRaisesRegex(ReleaseError, "Authentication"):
                self.cut()
        self.assertIsNone(self.git.remote_tag("origin", "v2.0.0"))

    def test_read_only_credentials(self):
        self.github.permissions = {"push": False}
        with self.assertRaisesRegex(ReleaseError, "write access"):
            self.cut()
        self.assertFalse(self.github.writes)

    def test_dirty_checkout(self):
        (self.root / "untracked").write_text("untracked")
        with self.assertRaisesRegex(ReleaseError, "clean"):
            self.cut()

    def test_wrong_repository(self):
        self.args.repo = "other/cetl"
        with self.assertRaisesRegex(ReleaseError, "does not match"):
            self.cut()

    def test_unmerged_commit(self):
        self.git.run("switch", "-c", "unmerged")
        (self.root / "new").write_text("unmerged")
        self.git.run("add", ".")
        self.git.run("commit", "-m", "Not merged")
        self.args.ref = self.git.commit("HEAD")
        with self.assertRaises(ReleaseError):
            self.cut()
        self.assertFalse(self.github.writes)

    def test_mismatched_version_never_pushes(self):
        self.args.version = "2.0.1"
        with self.assertRaisesRegex(ReleaseError, "does not match"):
            self.cut()
        self.assertIsNone(self.git.remote_tag("origin", "v2.0.1"))

    def test_conflicting_remote_tag(self):
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
        self.add_tag()
        with self.assertRaisesRegex(ReleaseError, "--resume"):
            self.cut()

    def test_failed_ci_cannot_create_draft_or_publish(self):
        self.add_tag()
        self.args.resume = True
        self.github.run["conclusion"] = "failure"
        with self.assertRaisesRegex(ReleaseError, "CI"):
            self.cut()
        self.assertFalse(self.github.writes)

    def test_unrelated_draft_cannot_be_adopted(self):
        self.add_tag()
        self.args.resume = True
        self.github.release_data = {"tag_name": "v2.0.0", "prerelease": False, "body": "Manual draft", "draft": True}
        with self.assertRaisesRegex(ReleaseError, "script-managed"):
            self.cut()
        self.assertFalse(self.github.writes)

    def test_remote_tag_movement_while_waiting_prevents_publication(self):
        self.add_tag()
        self.args.resume = True
        with patch.object(self.git, "remote_tag", side_effect=[self.commit, "b" * 40]):
            with self.assertRaisesRegex(ReleaseError, "changed"):
                self.cut()
        self.assertFalse(self.github.writes)

    def test_absent_ci_times_out_without_publication(self):
        self.add_tag()
        self.args.resume = True
        self.github.run = None
        with patch("release.time.monotonic", side_effect=[0, 2]):
            with self.assertRaisesRegex(ReleaseError, "Timed out"):
                self.cut()
        self.assertEqual(self.github.writes, ["dispatch"])

    def test_interrupted_upload_resumes_without_overwrite(self):
        self.add_tag()
        self.args.resume = True
        self.github.fail_after_upload = 2
        with self.assertRaisesRegex(ReleaseError, "interrupted"):
            self.cut()
        self.assertTrue(self.github.release_data["draft"])
        saved = dict(self.github.asset_data)
        self.github.fail_after_upload = None
        self.cut()
        self.assertFalse(self.github.release_data["draft"])
        for name, data in saved.items():
            self.assertEqual(self.github.asset_data[name], data)
        self.assertEqual(self.github.writes.count("upload"), 5)

    def test_conflicting_asset_prevents_publication(self):
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
        self.assertTrue(self.github.release_data["draft"])
        self.assertEqual(self.github.asset_data[name], b"wrong bytes")

    def test_documentation_failure_can_recover_after_publication(self):
        self.add_tag()
        self.args.resume = True
        self.github.fail_docs = True
        with self.assertRaisesRegex(ReleaseError, "published, but documentation failed"):
            self.cut()
        self.assertFalse(self.github.release_data["draft"])
        self.github.fail_docs = False
        self.cut()
        self.assertEqual(self.github.writes.count("publish"), 1)
        self.assertEqual(self.github.writes.count("dispatch"), 2)

    def test_new_release_cannot_downgrade_or_prerelease_a_stable_version(self):
        self.github.other_versions = {(2, 0, 0)}
        for version in ("2.0.0", "2.0.0-rc.1"):
            self.args.version = version
            with self.assertRaisesRegex(ReleaseError, "advance"):
                self.cut()
            self.assertIsNone(self.git.remote_tag("origin", "v" + version))

    def test_resume_old_release_does_not_roll_documentation_back(self):
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
        self.add_tag()
        with self.assertRaisesRegex(ReleaseError, "latest published"):
            require_latest_stable(self.github, "v2.0.0")

    def test_packaging_contents_reproducibility_and_corruption(self):
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
