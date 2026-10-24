#!/usr/bin/env python3
"""Validate CETL releases and build their distribution archives.

The maintainer CLI and CI share the version, verification-job, documentation,
and bundle contracts defined here. Git reads use committed content; packaging
writes local archives, a release manifest, and checksums. The module uses the
Python standard library and the Git executable. ``main`` provides the CI
entry point and writes validated metadata to GitHub Actions outputs.
"""

import argparse
from collections.abc import Iterable, Mapping, Sequence
import gzip
import hashlib
from html.parser import HTMLParser
import io
import json
import os
from pathlib import Path
import re
import subprocess
import tarfile
from typing import Any, Literal, overload


class ReleaseError(RuntimeError):
    """Report a failed release check or operation to the maintainer.

    The message describes the failed condition or corrective action. CLI entry
    points catch this exception and exit unsuccessfully. Operations completed
    before the exception, including remote writes, may need to be resumed.
    """


VERSION = re.compile(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-(alpha|beta|rc)\.([1-9][0-9]*))?")
HEADER = "include/cetl/cetl.hpp"
WORKFLOW = "cetlvast.yml"
RELEASE_FILES = ("scripts/release.py", "scripts/release_tools.py", "scripts/tests/test_release.py",
                 f".github/workflows/{WORKFLOW}")


def parse_version(value):
    """Parse a supported stable or prerelease version without a tag prefix.

    Args:
        value (str): X.Y.Z or X.Y.Z-{alpha,beta,rc}.N, with decimal components,
            no leading zeros, and a positive prerelease sequence number.

    Returns:
        re.Match: Five groups: major, minor, patch, stage, and sequence number.
            The final two groups are None for stable versions.

    Raises:
        ReleaseError: The entire string does not match the version grammar.
    """
    match = VERSION.fullmatch(value)
    if not match:
        raise ReleaseError("Expected X.Y.Z or X.Y.Z-{alpha,beta,rc}.N (no leading zeros; N >= 1).")
    return match


def header_version(text):
    """Read the numeric version from CETL's public header.

    Args:
        text (str): Header source containing CETL_VERSION_MAJOR, MINOR, and PATCH.

    Returns:
        str: The normalized X.Y.Z version.

    Raises:
        ReleaseError: A macro is missing, repeated, or not a bare nonnegative
            decimal integer without leading zeros.
    """
    parts = []
    for part in ("MAJOR", "MINOR", "PATCH"):
        definitions = re.findall(r"^\s*#\s*define[ \t]+CETL_VERSION_" + part + r"\b([^\n]*)", text, re.M)
        if len(definitions) != 1 or not re.fullmatch(r"[ \t]+(0|[1-9][0-9]*)[ \t]*", definitions[0]):
            raise ReleaseError(f"{HEADER}: CETL_VERSION_{part} must be defined exactly once as a decimal integer.")
        parts.append(definitions[0].strip())
    return ".".join(parts)


def validate(text, tag=None, prerelease=None):
    """Check agreement between header macros, a tag, and release classification.

    Args:
        text (str): CETL header source to inspect.
        tag (str | None): A v-prefixed version; None uses the header's version.
        prerelease (bool | None): Expected GitHub prerelease flag, or None to
            derive the classification from the version suffix.

    Returns:
        dict: ``version`` without v, ``tag`` (empty when omitted), and the
            boolean ``prerelease`` classification.

    Raises:
        ReleaseError: Header/version syntax is invalid, the tag's numeric core
            differs from the header, or the supplied classification disagrees.
    """
    core = header_version(text)
    version = tag[1:] if tag and tag.startswith("v") else core
    if tag and not tag.startswith("v"):
        raise ReleaseError("Release tags must start with v.")
    match = parse_version(version)
    if ".".join(match.groups()[:3]) != core:
        raise ReleaseError(f"Tag {tag} does not match {HEADER} ({core}).")
    is_prerelease = match.group(4) is not None
    if prerelease is not None and prerelease != is_prerelease:
        raise ReleaseError("GitHub's prerelease flag does not match the tag suffix.")
    return {"version": version, "tag": tag or "", "prerelease": is_prerelease}


@overload
def command(args: Sequence[str], cwd: str | Path | None = None, binary: Literal[False] = False) -> str: ...


@overload
def command(args: Sequence[str], cwd: str | Path | None = None, *, binary: Literal[True]) -> bytes: ...


@overload
def command(args: Sequence[str], cwd: str | Path | None = None, binary: bool = False) -> str | bytes: ...


def command(args: Sequence[str], cwd: str | Path | None = None, binary: bool = False) -> str | bytes:
    """Run an argument vector with captured output and a 120-second timeout.

    Args:
        args (Sequence[str]): Executable and arguments, passed without a shell.
        cwd (str | Path | None): Working directory; None inherits the caller's.
        binary (bool): Return stdout bytes instead of decoded, stripped text.

    Returns:
        str | bytes: Captured stdout in the requested representation.

    Raises:
        ReleaseError: The executable is missing, times out, or exits nonzero.
        OSError: Another process-launch error occurs.
        UnicodeDecodeError: Text stdout cannot be decoded as UTF-8.

    Subprocess diagnostics omit arguments and stderr, which may contain
    credentials. The invoked command determines filesystem and network effects.
    """
    try:
        result = subprocess.run(args, cwd=cwd, capture_output=True, check=False, timeout=120)
    except FileNotFoundError as ex:
        hint = {"git": "Install Git and rerun the release command.",
                "gh": "Install GitHub CLI from https://cli.github.com/ and run gh auth login --hostname github.com."}
        raise ReleaseError(f"Required executable is missing: {args[0]}. " + hint.get(args[0], "Install it and retry.")) from ex
    except subprocess.TimeoutExpired as ex:
        raise ReleaseError(f"{args[0]} timed out; inspect remote state before retrying.") from ex
    if result.returncode:
        raise ReleaseError(f"{args[0]} failed (exit {result.returncode}); check authentication, permissions, refs, and connectivity.")
    return result.stdout if binary else result.stdout.decode().strip()


class Git:
    """Run Git operations against one checkout using captured subprocess output.

    Args:
        root (str | Path): Checkout directory, resolved to an absolute path.

    Attributes:
        root (Path): Working directory used by every Git subprocess.

    Construction stores the path; individual operations check repository state.
    Methods propagate ReleaseError from failed Git commands.
    """

    def __init__(self, root):
        """Store the resolved checkout path without opening or changing it."""
        self.root = Path(root).resolve()

    @overload
    def run(self, *args: str, binary: Literal[False] = False) -> str: ...

    @overload
    def run(self, *args: str, binary: Literal[True]) -> bytes: ...

    @overload
    def run(self, *args: str, binary: bool = False) -> str | bytes: ...

    def run(self, *args: str, binary: bool = False) -> str | bytes:
        """Execute Git in this checkout with the output contract of ``command``.

        Args:
            *args (str): Git subcommand and arguments, passed without a shell.
            binary (bool): Return raw stdout bytes when True.

        Returns:
            str | bytes: Captured stdout, stripped when returned as text.

        Raises:
            ReleaseError: Git is unavailable, times out, or exits nonzero.

        Mutations and network access depend on the requested subcommand.
        """
        return command(["git", *args], cwd=self.root, binary=binary)

    def commit(self, ref):
        """Resolve a revision to its full commit object ID.

        Args:
            ref (str): Git revision expression; annotated tags are peeled.

        Returns:
            str: The full commit object ID.

        Raises:
            ReleaseError: The revision is missing or cannot resolve to a commit.
        """
        # --end-of-options prevents a ref supplied at the CLI from becoming a Git option.
        return self.run("rev-parse", "--verify", "--end-of-options", ref + "^{commit}")

    def validate(self, commit, tag=None, prerelease=None):
        """Validate a committed header using ``validate``'s metadata contract.

        Args:
            commit (str): Commit or revision containing the header.
            tag (str | None): Expected v-prefixed release version.
            prerelease (bool | None): Expected GitHub prerelease classification.

        Returns:
            dict: Validated ``version``, ``tag``, and ``prerelease`` fields.

        Raises:
            ReleaseError: Git cannot read the header or its metadata disagrees.
        """
        return validate(self.run("show", f"{commit}:{HEADER}"), tag, prerelease)

    def require_release_files(self, commit):
        """Require every path in RELEASE_FILES to exist in a committed tree.

        Args:
            commit (str): Revision to inspect for release scripts and workflow.

        Raises:
            ReleaseError: Git cannot find or inspect a required path.
        """
        for path in RELEASE_FILES:
            try:
                self.run("cat-file", "-e", f"{commit}:{path}")
            except ReleaseError as ex:
                raise ReleaseError(f"{path} is missing from {commit}. Merge the release tooling into main "
                                   "and select a commit containing it.") from ex

    def remote_repository(self, remote):
        """Identify a remote whose fetch and push URLs name the same repository.

        Args:
            remote (str): Configured Git remote name.

        Returns:
            str: GitHub OWNER/NAME, preserving the fetch URL's spelling.

        Raises:
            ReleaseError: The remote is unavailable, a URL is unsupported, or
                fetch and push identities differ when compared without case.
        """
        fetch = repository_from_url(self.run("remote", "get-url", remote))
        push = repository_from_url(self.run("remote", "get-url", "--push", remote))
        if fetch.lower() != push.lower():
            raise ReleaseError("Fetch and push remotes must identify the same GitHub repository.")
        return fetch

    def remote_tag(self, remote, tag):
        """Query a remote tag without fetching it into the checkout.

        Args:
            remote (str): Git remote name.
            tag (str): Exact tag name, including the release's v prefix.

        Returns:
            str | None: Peeled target object ID for an annotated tag, direct
                object ID for a lightweight tag, or None when the tag is absent.

        Raises:
            ReleaseError: Git cannot query the remote.
        """
        refs = dict(line.split()[::-1] for line in self.run(
            "ls-remote", "--tags", remote, f"refs/tags/{tag}", f"refs/tags/{tag}^{{}}"
        ).splitlines())
        return refs.get(f"refs/tags/{tag}^{{}}", refs.get(f"refs/tags/{tag}"))


def repository_from_url(url):
    """Extract the repository identity from a supported github.com remote URL.

    Args:
        url (str): HTTPS, SCP-style git@github.com, or ssh://git@github.com URL.

    Returns:
        str: OWNER/NAME with an optional .git suffix and trailing slash removed.

    Raises:
        ReleaseError: The URL uses another host, embeds HTTPS credentials,
            contains URL parameters, or does not identify one repository.
    """
    match = re.fullmatch(r"(?:https://github\.com/|git@github\.com:|ssh://git@github\.com/)([\w.-]+/[\w.-]+?)(?:\.git)?/?", url)
    if not match:
        raise ReleaseError("Use a github.com SSH or HTTPS remote without embedded credentials.")
    return match.group(1)


def required_jobs():
    """Return the set of exact job names required for a release CI attempt.

    The set contains validation, tooling tests, dependency preparation, docs,
    the release gate, asset packaging, and each compiler/standard matrix entry.
    The workflow contract test checks this list against CETLVaSt's matrix.

    Returns:
        set[str]: Job names whose conclusion must be success.
    """
    names = {"version", "release-tests", "cache-warmup", "docs", "release-ready", "release-assets"}
    for architecture, compilers, standards in (
        ("amd64", ("gcc-native-32", "clang-native"), (14, 17, 20, 23)),
        ("arm64", ("gcc-native", "clang-native"), (14, 17, 20)),
        ("legacy-arm64", ("gcc7-native",), (14,)),
    ):
        names.update(f"verification-{architecture} ({compiler}, cpp-{standard})"
                     for compiler in compilers for standard in standards)
    return names


def check_run(run: Mapping[str, Any], jobs: Iterable[Mapping[str, Any]], commit: str, tag: str) -> None:
    """Require successful CETLVaSt verification of one exact release revision.

    Args:
        run (dict): Actions run with head_sha, head_branch, event, path, status,
            and conclusion fields.
        jobs (Iterable[dict]): Jobs from this run's current attempt, each with
            name and conclusion fields. The caller must isolate the attempt.
        commit (str): Expected full commit object ID.
        tag (str): Expected v-prefixed tag used as the run's head_branch.

    Raises:
        ReleaseError: The run identifies another workflow/revision/event, is
            unsuccessful or incomplete, or lacks a successful required job.
    """
    if (run.get("head_sha") != commit or run.get("head_branch") != tag
            or run.get("event") not in ("push", "workflow_dispatch")
            or run.get("path") != f".github/workflows/{WORKFLOW}"):
        raise ReleaseError("CI must be CETLVaSt on the exact release tag and commit (push or manual dispatch).")
    if run.get("status") != "completed" or run.get("conclusion") != "success":
        raise ReleaseError("Release CI has not completed successfully.")
    successful = {job["name"] for job in jobs if job.get("conclusion") == "success"}
    missing = required_jobs() - successful
    if missing:
        raise ReleaseError("Required release jobs did not succeed: " + ", ".join(sorted(missing)))


def asset_names(version):
    """Return the archive and manifest filenames covered by SHA256SUMS.

    Args:
        version (str): Validated version without the v prefix.

    Returns:
        set[str]: Source, headers, and docs archive names plus release.json.
            SHA256SUMS itself is outside this set.
    """
    return {f"cetl-{version}-{kind}.tar.gz" for kind in ("source", "headers", "docs")} | {"release.json"}


def sha256(path):
    """Hash a file in chunks of at most one MiB.

    Args:
        path (str | Path): File to read as bytes.

    Returns:
        str: Lowercase hexadecimal SHA-256 digest.

    Raises:
        OSError: The file cannot be opened or read.
    """
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def check_documentation(html, version):
    """Require Doxygen's displayed project number to equal a release version.

    Args:
        html (str): Decoded HTML containing CETL's projectnumber span.
        version (str): Expected version text, including any prerelease suffix.

    Raises:
        ReleaseError: The displayed version is absent or differs after stripping
            surrounding whitespace. Incidental version text is ignored.
    """
    class ProjectNumber(HTMLParser):
        """Collect decoded text from CETL's projectnumber span."""

        def __init__(self):
            """Initialize the capture flag and collected text fragments."""
            super().__init__()
            self.recording = False
            self.values = []

        def handle_starttag(self, tag, attrs):
            """Start capturing at an element with the projectnumber ID."""
            if dict(attrs).get("id") == "projectnumber":
                self.recording = True

        def handle_endtag(self, tag):
            """End capture at a closing span, matching CETL's Doxygen template."""
            if tag == "span":
                self.recording = False

        def handle_data(self, data):
            """Append decoded text while the project number is being captured."""
            if self.recording:
                self.values.append(data)

    parser = ProjectNumber()
    parser.feed(html)
    actual = "".join(parser.values).strip()
    if actual != version:
        raise ReleaseError(f"Documentation displays {actual or '(no version)'}, expected {version}.")


def package(git, commit, tag, docs, output):
    """Write reproducible release archives, a manifest, and their checksums.

    Args:
        git (Git): Checkout containing the selected commit and local tag.
        commit (str): Full commit object ID to archive.
        tag (str): Matching v-prefixed release version.
        docs (str | Path): Built HTML directory containing index.html.
        output (str | Path): Empty or nonexistent destination directory.

    Raises:
        ReleaseError: Version/tag checks fail, the documentation entry point is
            missing, the output is nonempty, or documentation contains symlinks.
        OSError: A directory or archive cannot be read or written.

    Archives share a cetl-VERSION root. The header and documentation archives
    include LICENSE. Archive metadata and ordering are normalized for repeatable
    bytes from identical inputs. Failures may leave partial local output.
    """
    metadata = git.validate(commit, tag)
    if git.commit(f"refs/tags/{tag}") != commit:
        raise ReleaseError("Packaging commit differs from the local release tag.")
    docs, output = Path(docs), Path(output)
    if not (docs / "index.html").is_file():
        raise ReleaseError("Built documentation must contain index.html.")
    if output.exists() and any(output.iterdir()):
        raise ReleaseError("Package output directory must be empty.")
    output.mkdir(parents=True, exist_ok=True)
    version = metadata["version"]
    prefix = f"cetl-{version}/"
    for kind, paths in (("source", []), ("headers", ["include", "LICENSE"])):
        archive = git.run("archive", "--format=tar", f"--prefix={prefix}", commit, *paths, binary=True)
        with (output / f"cetl-{version}-{kind}.tar.gz").open("wb") as stream:
            with gzip.GzipFile(fileobj=stream, mode="wb", filename="", mtime=0) as compressed:
                compressed.write(archive)
    with (output / f"cetl-{version}-docs.tar.gz").open("wb") as stream:
        with gzip.GzipFile(fileobj=stream, mode="wb", filename="", mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode="w") as archive:
                for path in sorted(docs.rglob("*")):
                    if path.is_symlink():
                        raise ReleaseError("Documentation must not contain symlinks.")
                    if path.is_file():
                        data = path.read_bytes()
                        info = tarfile.TarInfo(prefix + "html/" + path.relative_to(docs).as_posix())
                        info.size, info.mode = len(data), 0o644
                        archive.addfile(info, io.BytesIO(data))
                license_data = git.run("show", f"{commit}:LICENSE", binary=True)
                info = tarfile.TarInfo(prefix + "LICENSE")
                info.size, info.mode = len(license_data), 0o644
                archive.addfile(info, io.BytesIO(license_data))
    manifest = {**metadata, "commit": commit}
    (output / "release.json").write_text(json.dumps(manifest, indent=2) + "\n")
    (output / "SHA256SUMS").write_text("".join(
        f"{sha256(output / name)}  {name}\n" for name in sorted(asset_names(version))
    ))


def check_bundle(directory, commit, tag):
    """Check a bundle's exact filenames, release identity, and file checksums.

    Args:
        directory (str | Path): Unpacked directory produced by ``package``.
        commit (str): Expected full source commit object ID.
        tag (str): Expected validated v-prefixed release version.

    Returns:
        list[str]: Sorted filenames, including SHA256SUMS, for asset syncing.

    Raises:
        ReleaseError: Files are missing/unexpected, metadata differs, or a
            checksum differs. Archive contents are not extracted by this check.
        ValueError: The manifest is invalid JSON.
        OSError: A bundle file cannot be read.
    """
    directory = Path(directory)
    version = tag[1:]
    expected = asset_names(version)
    if {path.name for path in directory.iterdir()} != expected | {"SHA256SUMS"}:
        raise ReleaseError("Release bundle contains missing or unexpected files.")
    manifest = json.loads((directory / "release.json").read_text())
    if manifest != {"version": version, "tag": tag, "prerelease": "-" in version, "commit": commit}:
        raise ReleaseError("Release bundle metadata does not match the requested release.")
    sums = (directory / "SHA256SUMS").read_text()
    if sums != "".join(f"{sha256(directory / name)}  {name}\n" for name in sorted(expected)):
        raise ReleaseError("Release bundle checksums do not match its contents.")
    return sorted(expected | {"SHA256SUMS"})


def main():
    """Run the validation, packaging, or built-documentation CLI operation.

    Arguments come from sys.argv. Validation checks HEAD against tag/release
    event metadata from GitHub's environment, prints JSON, and appends fields to
    GITHUB_OUTPUT when configured. Packaging writes the selected local bundle;
    documentation verification reads the supplied HTML file.

    Raises:
        SystemExit: Argument parsing exits, or an operation fails with status 1.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="operation", required=True)
    check = subparsers.add_parser("validate", help="Validate checkout and GitHub event metadata.")
    check.add_argument("--tag")
    check.add_argument("--prerelease", choices=("true", "false"))
    build = subparsers.add_parser("package", help="Package the validated tagged commit and built documentation.")
    build.add_argument("--tag", required=True)
    build.add_argument("--commit", required=True)
    build.add_argument("--docs", required=True)
    build.add_argument("--output", required=True)
    docs = subparsers.add_parser("verify-docs", help="Check the version displayed by built documentation.")
    docs.add_argument("--version", required=True)
    docs.add_argument("--path", type=Path, required=True)
    args = parser.parse_args()
    git = Git(Path.cwd())
    try:
        if args.operation == "verify-docs":
            parse_version(args.version)
            check_documentation(args.path.read_text(encoding="utf-8"), args.version)
            return
        commit = git.commit("HEAD")
        if args.operation == "package":
            package(git, git.commit(args.commit), args.tag, args.docs, args.output)
            return
        tag, prerelease = args.tag, None if args.prerelease is None else args.prerelease == "true"
        event = json.loads(Path(os.environ["GITHUB_EVENT_PATH"]).read_text()) if os.getenv("GITHUB_EVENT_PATH") else {}
        if os.getenv("GITHUB_EVENT_NAME") == "release":
            tag, prerelease = event["release"]["tag_name"], event["release"]["prerelease"]
        elif os.getenv("GITHUB_REF", "").startswith("refs/tags/"):
            tag = os.environ["GITHUB_REF"][len("refs/tags/"):]
        if tag and git.commit(f"refs/tags/{tag}") != commit:
            raise ReleaseError("Checkout does not match the requested tag's commit.")
        result = {**git.validate(commit, tag, prerelease), "commit": commit}
        print(json.dumps(result, indent=2))
        if os.getenv("GITHUB_OUTPUT"):
            with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as stream:
                for key, value in result.items():
                    stream.write(f"{key}={str(value).lower() if isinstance(value, bool) else value}\n")
    except (ReleaseError, ValueError, OSError, KeyError) as ex:
        parser.exit(1, f"Release validation failed: {ex}\n")


if __name__ == "__main__":
    main()
