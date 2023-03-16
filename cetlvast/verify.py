#!/usr/bin/env python3
#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#
"""
    Command-line helper for running verification builds.

    emrainey says this script should be replaced with conan profiles. I'll look
    into this but for now "it's better than a bash script."

"""

import argparse
import logging
import os
import pathlib
import re
import shutil
import subprocess
import sys
import textwrap
import typing

# +---------------------------------------------------------------------------+


def _make_parser() -> argparse.ArgumentParser:

    epilog = textwrap.dedent(
        """

        **Example Usage**::

            ./verify.py docs

    """
    )

    parser = argparse.ArgumentParser(
        description="CMake command-line helper for running cetlvast suites.",
        epilog=epilog,
        formatter_class=argparse.RawTextHelpFormatter,
    )

    parser.add_argument(
        "-v",
        "--verbose",
        action="count",
        default=0,
        help=textwrap.dedent(
            """
        Used to form --log-level value passed into cmake as well as the
        verbosity of this script.

        #      | cmake   | verify.py
        ----------------------------
        (none) : NOTICE  : warning
             1 : STATUS  : warning
             2 : VERBOSE : info
             3 : DEBUG   : debug
            4+ : TRACE   : debug

    """[1:])
    )

    kit_args = parser.add_argument_group(
        title="CETLVaSt suites",
        description=textwrap.dedent(
            """
        Select the test suite to run and the toolchain to run it against.
    """[1:])
    )

    kit_args.add_argument(
        "suite",
        choices=["none", "unittest", "compile", "lint", "docs", "ontarget"],
        default="none",
        help=textwrap.dedent(
            """
        Used to form -DCETLVAST_TEST_SUITE value

        The five cetlvast suites are:

            unit tests      : googletest/googlemock tests compiled and executed
                              on the local system. these tests verify the core
                              interface contracts of CETL types are not violated
                              including the behavioural contracts, where
                              defined. These also provide some level of
                              verification of compatibility with different tool
                              chains.
            compile tests   : A specalized suite of unit tests that run in the
                              compiler. These verify that certain invariants are
                              properly protected by static assertions.
            lint/analysis   : Runs various static analysis tools against the
                              cetl types.
            documentation   : While not typically considered a test, this target
                              builds the doxygen documentation for cetlvast
                              which verifies that the types are all properly
                              documented. Furthermore, this builds and executes
                              any example code included in the docs to ensure
                              the code is correct.
            on-target       : This is future work. The suite will build one or
                              more firmware images for verifying the performance
                              of CETL on a select one or two embedded
                              processors.

        Psedo-suites:
            none            : No test suite.

    """[1:])
    )

    kit_args.add_argument(
        "-tc",
        "--toolchain",
        choices=["gcc", "clang"],
        default="clang",
        help=textwrap.dedent(
            """

        Used to form -DCMAKE_TOOLCHAIN_FILE value

        This selects the toolchain description cetlvast will tell Cmake to use.

    """[1:])
    )

    variant_args = parser.add_argument_group(
        title="build variants",
        description=textwrap.dedent(
            """
        Arguments the modify build parameters.
    """[1:])
    )

    variant_args.add_argument(
        "-bf",
        "--build-flavor",
        choices=["Debug", "Release"],
        default="Debug",
        help=textwrap.dedent(
            """
        Sets -DCMAKE_BUILD_TYPE value

        Debug   : builds will be lightly optimized or not optimized. Debug
                  symbols will be included.
        Release : builds will be reasonably optimized.

    """[1:])
    )

    variant_args.add_argument(
        "--coverage",
        action="store_true",
        help=textwrap.dedent(
            """
        Sets -DCETLVAST_ENABLE_COVERAGE:BOOL=ON

        Enables instrumentation of code and selects coverage reporting test
        targets.

    """[1:])
    )

    variant_args.add_argument(
        "-cda",
        "--asserts",
        action="store_true",
        help=textwrap.dedent(
            """
        Sets -DCETL_ENABLE_DEBUG_ASSERT:BOOL=ON

        Enables CETL debug asserts. Also forces the build flavor to be Debug.

    """[1:])
    )

    variant_args.add_argument(
        "-std",
        "--cpp-standard",
        choices=["base", "intermediate", "target"],
        default="target",
        help=textwrap.dedent(
            """
        Sets -DCETLVAST_CPP_STANDARD value

        base (C++14)          : Use the C++ 14 standard which is the base
                                standard for CETL 1.0. This allows testing of
                                CETL as a polyfill library for C++17 and 20.
        intermediate (C++17)  : Use the C++ 17 standard. This enables testing of
                                CETL as a polyfill library for C++20 and enables
                                A/B testing of CETL against any C++17 types
                                it supports.
        target (C++20)        : Use the C++20 standard, the target support level
                                for CETL 1.0. This enables A/B testing of CETL
                                to ensure forwards compatibility.

    """[1:])
    )

    action_args = parser.add_argument_group(
        title="action modifiers",
        description=textwrap.dedent(
            """
        Arguments that change the actions taken by this script.
    """[1:])
    )

    action_args.add_argument(
        "-f",
        "--force",
        action="store_true",
        help=textwrap.dedent(
            """
        Force recreation of verification directory if it already exists.

        ** WARNING ** This will delete the cmake build directory!

    """[1:])
    )

    action_args.add_argument(
        "--version",
        action="store_true",
        help=textwrap.dedent(
            """
        Emits the current version. Use "none" suite to simply emit the version
        and exit:

            export CETL_VERSION=$(./verify.py --version none)

    """[1:])
    )

    action_args.add_argument(
        "-lsbd",
        "--ls-builddir",
        action="store_true",
        help=textwrap.dedent(
            """
        Emits a relative path to the build directory. Use with "none" suite to
        simply emit this path and exit. For example:

            pushd $(./verify.py -lsbd none)
            ninja -t commands
            popd

    """[1:])
    )

    action_args.add_argument(
        "-lssd",
        "--ls-builddir-suite",
        action="store_true",
        help=textwrap.dedent(
            """
        Emits a relative path to the build directory for a given suite then
        exits. For Example:

            open "$(./verify.py -lssd docs)/html/index.html"

        This action happens before the build directory action so -rm will be
        ignored.

    """[1:])
    )

    action_args.add_argument(
        "--builddir-only",
        action="store_true",
        help=textwrap.dedent(
            """
        Handle -rm and -f arguments but do not configure, build, or run tests.
        Use with none to perform a nuclear-clean operation:

            alias cv_superclean="./verify.py -rm -f --builddir-only none"

    """[1:])
    )

    action_args.add_argument(
        "-c",
        "--configure-only",
        action="store_true",
        help=textwrap.dedent(
            """
        Configure but do not build.
    """[1:])
    )

    action_args.add_argument(
        "-b",
        "--build-only",
        action="store_true",
        help=textwrap.dedent(
            """
        Try to build without configuring. Do not try to run tests.
    """[1:])
    )

    action_args.add_argument(
        "-t",
        "--test-only",
        action="store_true",
        help=textwrap.dedent(
            """
        Only try to run tests. Don't configure or build.
    """[1:])
    )

    action_args.add_argument(
        "--dry-run",
        action="store_true",
        help=textwrap.dedent(
            """
        Don't actually do anything. Just log what this script would have done.
        Combine with --verbose to ensure you actually see the script's log
        output.
    """[1:])
    )

    action_args.add_argument(
        "-rm",
        "--remove-first",
        action="store_true",
        help=textwrap.dedent(
            """
        If specified, any existing build directory will be deleted first. Use
        -f to skip the user prompt.

        Note: This only applies to the configure step. If you do a build-only
        this argument has no effect.
    """[1:])
    )

    other_args = parser.add_argument_group(
        title="other options",
        description=textwrap.dedent(
            """
        Additional stuff you probably can ignore.
    """[1:])
    )

    other_args.add_argument(
        "--build-dir-name",
        default="verifypy",
        help=textwrap.dedent(
            """
        This script always uses [build_{build-dir-name}] as the name of the
        top-level directory it creates and under which it does its work. This
        option lets you change the {build-dir-name} part of that file name.

    """[1:])
    )

    other_args.add_argument(
        "-cd",
        "--cetlvast-dir",
        default=pathlib.Path.cwd(),
        type=pathlib.Path,
        help=textwrap.dedent(
            """
        By default this script uses the current-working directory as the root
        for CETLVaSt. Use this option to specify a different root directory when
        running the script.

    """[1:])
    )

    other_args.add_argument(
        "--force-ninja",
        action="store_true",
        help=textwrap.dedent(
            """

        -DCMAKE_GENERATOR=Ninja

        Form an argument requireing cmake use the Ninja build system instead of
        the default for the current system which can be make.

    """[1:])
    )


    return parser


# +---------------------------------------------------------------------------+


def _cmake_run(
    cmake_args: typing.List[str],
    cmake_dir: pathlib.Path,
    verbose: int,
    dry_run: bool,
    env: typing.Optional[typing.Dict] = None,
) -> int:
    """
    Simple wrapper around cmake execution logic
    """
    logging.info(
        textwrap.dedent(
            """
    *****************************************************************
    About to run command: {}
    in directory        : {}
    *****************************************************************
    """
        ).format(" ".join(cmake_args), str(cmake_dir))
    )

    copy_of_env: typing.Dict = {}
    copy_of_env.update(os.environ)
    if env is not None:
        copy_of_env.update(env)

    if verbose > 1:
        logging.debug("        *****************************************************************")
        logging.debug("        Using Environment:")
        for key, value in copy_of_env.items():
            overridden = key in env if env is not None else False
            logging.debug("            {} = {}{}".format(key, value, (" (override)" if overridden else "")))
        logging.debug("        *****************************************************************\n")

    if not dry_run:
        return subprocess.run(cmake_args, cwd=cmake_dir, env=copy_of_env).returncode
    else:
        return 0


# +---------------------------------------------------------------------------+


def _remove_build_dir_action(args: argparse.Namespace, cmake_dir: pathlib.Path) -> None:
    """
    Handle all the logic, user input, logging, and file-system operations needed to
    remove the cmake build directory ahead of invoking cmake.
    """
    if args.remove_first and cmake_dir.exists():
        okay_to_remove = False
        if not args.force:
            response = input("Are you sure you want to delete {}? [y/N]:".format(cmake_dir))
            if (len(response) == 1 and response.lower() == "y") or (len(response) == 3 and response.lower() == "yes"):
                okay_to_remove = True
        else:
            okay_to_remove = True

        if okay_to_remove:
            if not args.dry_run:
                logging.info("Removing directory {}".format(cmake_dir))
                shutil.rmtree(cmake_dir)
            else:
                logging.info("Is dry-run. Would have removed directory {}".format(cmake_dir))
        else:
            raise RuntimeError(
                """
                Build directory {} already exists, -rm or --remove-first was specified,
                and permission was not granted to delete it. We cannot continue. Either
                allow re-use of this build directory or allow deletion. (use -f flag to
                skip user prompts).""".lstrip().format(
                    cmake_dir
                )
            )


# +---------------------------------------------------------------------------+


def _create_build_dir_action(args: argparse.Namespace, cmake_dir: pathlib.Path) -> None:
    """
    Handle all the logic, user input, logging, and file-system operations needed to
    create the cmake build directory ahead of invoking cmake.
    """
    if not cmake_dir.exists():
        if not args.dry_run:
            logging.info("Creating build directory at {}".format(cmake_dir))
            cmake_dir.mkdir()
        else:
            logging.info("Dry run: Would have created build directory at {}".format(cmake_dir))
    else:
        logging.info("Using existing build directory at {}".format(cmake_dir))


# +---------------------------------------------------------------------------+


def _cmake_configure(args: argparse.Namespace, cmake_args: typing.List[str], cmake_dir: pathlib.Path) -> int:
    """
    Format and execute cmake configure command. This also include the cmake build directory (re)creation
    logic.
    """

    if args.build_only or args.test_only:
        return 0

    if args.verbose == 1:
        cmake_logging_level = "STATUS"
    elif args.verbose == 2:
        cmake_logging_level = "VERBOSE"
    elif args.verbose == 3:
        cmake_logging_level = "DEBUG"
    elif args.verbose > 3:
        cmake_logging_level = "TRACE"
    else:
        cmake_logging_level = "NOTICE"

    cmake_configure_args = cmake_args.copy()

    cmake_configure_args.append("--log-level={}".format(cmake_logging_level))

    flag_set_dir = pathlib.Path("cmake") / pathlib.Path("compiler_flag_sets")
    flagset_file = (flag_set_dir / pathlib.Path("native")).with_suffix(".cmake")

    cmake_configure_args.append("-DCETLVAST_FLAG_SET={}".format(str(flagset_file)))

    if args.suite != "none":
        test_suite_dir = pathlib.Path("cmake") / pathlib.Path("suites")
        test_suite_dir = (test_suite_dir / pathlib.Path(args.suite)).with_suffix(".cmake")
        cmake_configure_args.append("-DCETLVAST_TEST_SUITE={}".format(str(test_suite_dir)))
    if args.toolchain != "none":
        toolchain = pathlib.Path("cmake") / pathlib.Path("toolchains")
        if args.toolchain == "clang":
            toolchain_file = toolchain / pathlib.Path("clang-native").with_suffix(".cmake")
        else:
            toolchain_file = toolchain / pathlib.Path("gcc-native").with_suffix(".cmake")

        cmake_configure_args.append("-DCMAKE_TOOLCHAIN_FILE={}".format(str(toolchain_file)))

    if args.coverage:
        cmake_configure_args.append("-DCETLVAST_ENABLE_COVERAGE:BOOL=ON")
    else:
        cmake_configure_args.append("-DCETLVAST_ENABLE_COVERAGE:BOOL=OFF")

    if args.asserts:
        if args.build_flavor != "Debug":
            logging.warning("-cda/--asserts forces the build to be Debug. Ignoring -bf/--build-flavor {}".format(args.build_flavor))
        cmake_configure_args.append("-DCMAKE_BUILD_TYPE=Debug")
        cmake_configure_args.append("-DCETL_ENABLE_DEBUG_ASSERT:BOOL=ON")
    else:
        cmake_configure_args.append("-DCMAKE_BUILD_TYPE={}".format(args.build_flavor))
        cmake_configure_args.append("-DCETL_ENABLE_DEBUG_ASSERT:BOOL=OFF")

    if args.cpp_standard == "base":
        cmake_configure_args.append("-DCETLVAST_CPP_STANDARD=14")
    elif args.cpp_standard == "intermediate":
        cmake_configure_args.append("-DCETLVAST_CPP_STANDARD=17")
    elif args.cpp_standard == "target":
        cmake_configure_args.append("-DCETLVAST_CPP_STANDARD=20")
    else:
        raise RuntimeError("internal error: illegal cpp-standard choice got through? ({})".format(args.cpp_standard))

    if args.force_ninja:
        cmake_configure_args.append("-DCMAKE_GENERATOR=Ninja")

    cmake_configure_args.append("..")

    return _cmake_run(cmake_configure_args, cmake_dir, args.verbose, args.dry_run)


# +---------------------------------------------------------------------------+


def _cmake_build(args: argparse.Namespace, cmake_args: typing.List[str], cmake_dir: pathlib.Path) -> int:
    """
    Format and execute cmake build command. This method assumes that the cmake_dir is already properly
    configured.
    """
    if not args.configure_only and not args.test_only:
        cmake_build_args = cmake_args.copy()

        cmake_build_args += ["--build", ".", "--target"]

        if args.suite == "unittest":
            cmake_build_args.append("build_all")
        elif args.suite == "docs":
            cmake_build_args.append("build_all_examples")
        elif args.suite == "compile":
            cmake_build_args.append("build_all")
        elif args.suite == "lint":
            logging.debug("lint target doesn't currently have a build step")
            return 0
        elif args.suite == "ontarget":
            logging.warning("ontarget tests not implemented yet!")
            return -1
        elif args.suite == "none":
            logging.debug("no test suite specified. Nothing to do.")
            return 0
        else:
            raise RuntimeError("invalid test suite {} got through argparse?".format(args.suite))

        return _cmake_run(cmake_build_args, cmake_dir, args.verbose, args.dry_run)

    return 0


# +---------------------------------------------------------------------------+


def _cmake_test(args: argparse.Namespace, cmake_args: typing.List[str], cmake_dir: pathlib.Path) -> int:
    """
    Format and execute cmake test command. This method assumes that the cmake_dir is already properly
    configured.
    """
    if not args.configure_only and not args.build_only:

        cmake_test_args = cmake_args.copy()

        cmake_test_args += ["--build", ".", "--target"]

        if args.suite == "unittest":
            if args.coverage:
                cmake_test_args.append("cov_all")
            else:
                cmake_test_args.append("test_all")
        elif args.suite == "docs":
            cmake_test_args.append("docs")
        elif args.suite == "lint":
            cmake_test_args.append("all")
        elif args.suite == "compile":
            cmake_test_args.append("test_all")
        elif args.suite == "ontarget":
            logging.warning("ontarget tests not implemented yet!")
            return -1
        elif args.suite == "none":
            logging.debug("No test suite specified. Nothing to do.")
            return 0
        else:
            raise RuntimeError("invalid test suite {} got through argparse?".format(args.suite))

        return _cmake_run(cmake_test_args, cmake_dir, args.verbose, args.dry_run)

    return 0


# +---------------------------------------------------------------------------+


def _cmake_ctest(args: argparse.Namespace, cmake_args: typing.List[str], cmake_dir: pathlib.Path) -> int:
    """
    run ctest
    """
    if not args.configure_only and not args.build_only:

        if args.suite == "compile":
            # we use ctest to run the compile tests so we take a different
            # branch here.
            return subprocess.run(["ctest"], cwd=cmake_dir).returncode
        else:
            logging.debug("No ctest action defined for {} test suite.".format(args.suite))

    return 0


# +---------------------------------------------------------------------------+


def _create_build_dir_name(args: argparse.Namespace) -> str:
    return "build_{}".format(args.build_dir_name)


# +---------------------------------------------------------------------------+


def _get_version_string(args: argparse.Namespace, gitdir: pathlib.Path) -> typing.Tuple[int, int, int, str]:
    if _get_version_string._version_string is None:
        git_output = subprocess.run(["git", "describe", "--abbrev=0", "--tags"], cwd=gitdir, capture_output=True, text=True).stdout
        if (match_obj := re.match(r"^v(\d+)\.(\d+)\.(\d+)[-_]?(\w*)", git_output)) is not None:
            _get_version_string._version_string = (match_obj.group(1),
                                                   match_obj.group(2),
                                                   match_obj.group(3),
                                                   qualifier if (qualifier:=match_obj.group(4)) else "")
        else:
            _get_version_string._version_string = (0,0,0,"")

    return _get_version_string._version_string

_get_version_string._version_string : typing.Optional[typing.Tuple[int, int, int, str]] = None


# +---------------------------------------------------------------------------+


def _handle_special_actions(args: argparse.Namespace, cmake_args: typing.List[str], cmake_dir: pathlib.Path, gitdir: pathlib.Path) -> int:
    if args.ls_builddir:
        sys.stdout.write(str(cmake_dir))
        if args.suite != "none":
            sys.stdout.write(os.linesep)
    if args.version:
        sys.stdout.write("{}.{}.{}{}".format(*_get_version_string(args, gitdir)))
        if args.suite != "none":
            sys.stdout.write(os.linesep)

    return 0


def _handle_lssd(args: argparse.Namespace, cmake_args: typing.List[str], cmake_dir: pathlib.Path, gitdir: pathlib.Path) -> int:
    sys.stdout.write(str(cmake_dir / "cetlvast" / "suites" / args.suite))
    return 0

# +---------------------------------------------------------------------------+


def main() -> int:
    """
    Main method to execute when this package/script is invoked as a command.
    """
    args = _make_parser().parse_args()

    verification_dir = args.cetlvast_dir
    cmake_dir = verification_dir / pathlib.Path(_create_build_dir_name(args))
    cmake_args = ["cmake"]

    logging_level = logging.WARN

    if args.verbose == 2:
        logging_level = logging.INFO
    elif args.verbose > 3:
        logging_level = logging.DEBUG

    logging.basicConfig(format="%(levelname)s: %(message)s", level=logging_level)

    logging.debug(
        textwrap.dedent(
            """

    *****************************************************************
    Commandline Arguments to {}:

    {}

    For verify version {}
    *****************************************************************

    """
        ).format(os.path.basename(__file__), str(args), _get_version_string(args, verification_dir))
    )

    if args.ls_builddir_suite:
        return _handle_lssd(args, cmake_args, cmake_dir, verification_dir)

    _remove_build_dir_action(args, cmake_dir)

    if args.builddir_only:
        return 0

    special_action_result = _handle_special_actions(args, cmake_args, cmake_dir, verification_dir)

    if special_action_result != 0:
        return special_action_result
    elif args.suite == "none":
        return 0

    _create_build_dir_action(args, cmake_dir)

    configure_result = _cmake_configure(args, cmake_args, cmake_dir)

    if configure_result != 0:
        return configure_result
    elif args.configure_only:
        return 0

    build_result = _cmake_build(args, cmake_args, cmake_dir)

    if build_result != 0:
        return build_result
    elif args.build_only:
        return 0

    if not args.configure_only and not args.build_only:
        test_result = _cmake_test(args, cmake_args, cmake_dir)

        if test_result != 0:
            return test_result
        else:
            return _cmake_ctest(args, cmake_args, cmake_dir)

    raise RuntimeError("Internal logic error: only_do_x flags resulted in no action.")


# +---------------------------------------------------------------------------+


if __name__ == "__main__":
    sys.exit(main())
