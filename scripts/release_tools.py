#!/usr/bin/env python3
"""Shared release validation and packaging. Python 3.10+, standard library only."""

import argparse
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import re
import subprocess
import tarfile


class ReleaseError(RuntimeError):
    """An unsafe or incomplete release operation."""


VERSION = re.compile(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-(alpha|beta|rc)\.([1-9][0-9]*))?")
HEADER = "include/cetl/cetl.hpp"
WORKFLOW = "cetlvast.yml"


def parse_version(value):
    match = VERSION.fullmatch(value)
    if not match:
        raise ReleaseError("Expected X.Y.Z or X.Y.Z-{alpha,beta,rc}.N (no leading zeros; N >= 1).")
    return match


def header_version(text):
    parts = []
    for part in ("MAJOR", "MINOR", "PATCH"):
        definitions = re.findall(r"^\s*#\s*define[ \t]+CETL_VERSION_" + part + r"\b([^\n]*)", text, re.M)
        if len(definitions) != 1 or not re.fullmatch(r"[ \t]+(0|[1-9][0-9]*)[ \t]*", definitions[0]):
            raise ReleaseError(f"{HEADER}: CETL_VERSION_{part} must be defined exactly once as a decimal integer.")
        parts.append(definitions[0].strip())
    return ".".join(parts)


def validate(text, tag=None, prerelease=None):
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


def command(args, cwd=None, binary=False):
    # Do not echo commands, environment variables, or subprocess stderr: gh/git may include credentials.
    try:
        result = subprocess.run(args, cwd=cwd, capture_output=True, check=False, timeout=120)
    except FileNotFoundError as ex:
        raise ReleaseError(f"Required executable is missing: {args[0]}") from ex
    except subprocess.TimeoutExpired as ex:
        raise ReleaseError(f"{args[0]} timed out; inspect remote state before retrying.") from ex
    if result.returncode:
        raise ReleaseError(f"{args[0]} failed (exit {result.returncode}); check authentication, permissions, refs, and connectivity.")
    return result.stdout if binary else result.stdout.decode().strip()


class Git:
    def __init__(self, root):
        self.root = Path(root).resolve()

    def run(self, *args, binary=False):
        return command(["git", *args], cwd=self.root, binary=binary)

    def commit(self, ref):
        # --end-of-options prevents a ref supplied at the CLI from becoming a Git option.
        return self.run("rev-parse", "--verify", "--end-of-options", ref + "^{commit}")

    def validate(self, commit, tag=None, prerelease=None):
        return validate(self.run("show", f"{commit}:{HEADER}"), tag, prerelease)

    def remote_repository(self, remote):
        fetch = repository_from_url(self.run("remote", "get-url", remote))
        push = repository_from_url(self.run("remote", "get-url", "--push", remote))
        if fetch.lower() != push.lower():
            raise ReleaseError("Fetch and push remotes must identify the same GitHub repository.")
        return fetch

    def remote_tag(self, remote, tag):
        refs = dict(line.split()[::-1] for line in self.run(
            "ls-remote", "--tags", remote, f"refs/tags/{tag}", f"refs/tags/{tag}^{{}}"
        ).splitlines())
        return refs.get(f"refs/tags/{tag}^{{}}", refs.get(f"refs/tags/{tag}"))


def repository_from_url(url):
    match = re.fullmatch(r"(?:https://github\.com/|git@github\.com:|ssh://git@github\.com/)([\w.-]+/[\w.-]+?)(?:\.git)?/?", url)
    if not match:
        raise ReleaseError("Use a github.com SSH or HTTPS remote without embedded credentials.")
    return match.group(1)


def required_jobs():
    names = {"version", "release-tests", "cache-warmup", "docs", "release-ready", "release-assets"}
    for architecture, compilers, standards in (
        ("amd64", ("gcc-native-32", "clang-native"), (14, 17, 20, 23)),
        ("arm64", ("gcc-native", "clang-native"), (14, 17, 20)),
        ("legacy-arm64", ("gcc7-native",), (14,)),
    ):
        names.update(f"verification-{architecture} ({compiler}, cpp-{standard})"
                     for compiler in compilers for standard in standards)
    return names


def check_run(run, jobs, commit, tag):
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
    return {f"cetl-{version}-{kind}.tar.gz" for kind in ("source", "headers", "docs")} | {"release.json"}


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def package(git, commit, tag, docs, output):
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
    args = parser.parse_args()
    git = Git(Path.cwd())
    try:
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
