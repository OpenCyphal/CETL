#!/usr/bin/env python3
#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#
"""
    Translate a JUnit XML report into the SonarQube generic test execution format.

    Googletest can generate a JUnit XML report, but SonarQube does not support this format.
    Use this script to convert the JUnit XML report into the SonarQube generic test execution
    format to allow test results to be recorded in SonarQube.
"""
# cSpell: words testsuite testsuites quirksmode levelname

import argparse
import logging
import os
import pathlib
import sys
import textwrap
import typing
import xml.etree.ElementTree as ET

# +---------------------------------------------------------------------------+

def _junit_to_sonarqube_generic_execution_format(junit_report: pathlib.Path, test_executions: ET.Element) -> None:
    """Append junit testsuite data to sonarqube testExecutions data.

    Input Format: http://google.github.io/googletest/advanced.html#generating-an-xml-report
    Output Format: https://docs.sonarqube.org/8.9/analyzing-source-code/generic-test-data/#generic-execution

    """
    sq_files: typing.Dict[str, ET.Element] = dict()
    junit_xml = ET.parse(junit_report)

    testsuite_or_testsuites = junit_xml.getroot()

    if testsuite_or_testsuites.tag == "testsuite":
        testsuites: typing.Iterable[ET.Element] = [testsuite_or_testsuites]
    else:
        testsuites = testsuite_or_testsuites.findall("testsuite")

    for testsuite in testsuites:
        logging.info("junit2sonarqube: parsing junit testsuite: {} name=\"{}\" tests=\"{}\""
            .format(testsuite.tag,
                    testsuite.get("name"),
                    testsuite.get("tests")))

        for testcase in testsuite:

            testcase_file = testcase.get("file")
            if testcase_file is not None:
                quirksmode = "gtest"
            else:
                quirksmode = "ctest"
                testcase_file = testcase.get("classname")
                if testcase_file is None:
                    logging.warn("junit2sonarqube: Unknown tag {} (skipping)".format(testcase.tag))
                    continue

            testcase_name: str = testcase.get("name", "")
            if (type_param := testcase.get("type_param", None)) != None:
                # mypy is not able to parse assignment expressions, apparently.
                testcase_name = testcase_name + " " + type_param # type: ignore

            logging.debug("junit2sonarqube: found testcase \"{}\" (quirks={})".format(testcase_name, quirksmode))

            try:
                sq_file = sq_files[testcase_file]
            except KeyError:
                sq_file = ET.Element("file", attrib={"path": testcase_file})
                test_executions.append(sq_file)
                sq_files[testcase_file] = sq_file

            sq_testcase_attrib: typing.Dict[str, str] = dict()
            sq_testcase_attrib["name"] = testcase_name
            test_duration = float(testcase.get("time", 0.0))
            sq_testcase_attrib["duration"] = str(test_duration)

            sq_test_case = ET.Element("testCase", attrib=sq_testcase_attrib)
            sq_file.append(sq_test_case)

            skipped: typing.Optional[ET.Element] = testcase.find("skipped")
            if skipped is not None:
                sq_skipped = ET.Element("skipped", attrib={"message": skipped.get("message", "").rstrip()})
                sq_skipped.text = skipped.text.rstrip()  # type: ignore
                sq_test_case.append(sq_skipped)
            failures = testcase.findall("failure")
            if failures is not None and len(failures) > 0:
                sq_failure = ET.Element("failure", attrib={"message": "failed"})
                sq_test_case.append(sq_failure)
                sq_failure_text = []
                for failure in failures:
                    for failure_line in failure.get("message", "").split("\n"):
                        sq_failure_text.append(failure_line)
                sq_failure.set("message", sq_failure_text[0])
                if len(sq_failure_text) > 1:
                    sq_failure.text = "\n".join(sq_failure_text[1:])


# +---------------------------------------------------------------------------+


def _handle_generate_test_report(args: argparse.Namespace, cmake_dir: pathlib.Path, test_result: int) -> int:
    if (output_file := args.generate_test_report) is None:
        return 0

    output_path = pathlib.Path.cwd().joinpath(_suite_dir(args, cmake_dir) / output_file)
    test_executions = ET.Element("testExecutions", attrib={"version": "1"})
    sq_report = ET.ElementTree(test_executions)

    for gtest_report in _suite_dir(args, cmake_dir).glob("*-gtest.xml"):
        logging.debug("Found gtest report {}. Will combine into sonarqube report.".format(gtest_report))
        _junit_to_sonarqube_generic_execution_format(gtest_report, sq_report.getroot())

    for ctest_report in _suite_dir(args, cmake_dir).glob("*ctest.xml"):
        logging.debug("Found ctest report {}. Will combine into sonarqube report.".format(ctest_report))
        _junit_to_sonarqube_generic_execution_format(ctest_report, sq_report.getroot())

    if args.dry_run:
        logging.debug("Would have written a test report for {} files to {}".format(len(test_executions.findall("file")), output_path))
    else:
        logging.debug("About to write a test report for {} files to {}".format(len(test_executions.findall("file")), output_path))
        ET.indent(sq_report)
        output_path.parent.mkdir(parents=True, exist_ok=True)
        sq_report.write(output_path, encoding="UTF-8")
    return test_result

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
    elif args.verbose >= 3:
        logging_level = logging.DEBUG

    logging.basicConfig(format="%(levelname)s: %(message)s", level=logging_level)

    logging.info(
        textwrap.dedent(
            """

    *****************************************************************
    Command-line Arguments to {}:

    {}

    For verify version {}
    *****************************************************************

    """
        ).format(os.path.basename(__file__), str(args), _get_version_string(verification_dir))
    )


# +---------------------------------------------------------------------------+


if __name__ == "__main__":
    sys.exit(main())
