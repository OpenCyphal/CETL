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
import typing
import xml.etree.ElementTree as ET

# +---------------------------------------------------------------------------+

def _junit_to_sonarqube_generic_execution_format(junit_report: pathlib.Path, test_executions: ET.Element, base_dir: typing.Optional[pathlib.Path] = None) -> None:
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
                    logging.warning("junit2sonarqube: Unknown tag {} (skipping)".format(testcase.tag))
                    continue

            testcase_name: str = testcase.get("name", "")
            if (type_param := testcase.get("type_param", None)) != None:
                # mypy is not able to parse assignment expressions, apparently.
                testcase_name = testcase_name + " " + type_param # type: ignore

            logging.debug("junit2sonarqube: found testcase \"{}\" (quirks={})".format(testcase_name, quirksmode))

            try:
                sq_file = sq_files[testcase_file]
            except KeyError:
                  # Make path relative to base_dir if provided
                file_path = testcase_file
                if base_dir is not None:
                    try:
                        rel_path = os.path.relpath(testcase_file, base_dir)
                        file_path = rel_path
                    except ValueError:
                        file_path = testcase_file
                sq_file = ET.Element("file", attrib={"path": file_path})
                test_executions.append(sq_file)
                sq_files[testcase_file] = sq_file

            sq_testcase_attrib: typing.Dict[str, str] = dict()
            sq_testcase_attrib["name"] = testcase_name
            test_duration = float(testcase.get("time", 0.0))
            sq_testcase_attrib["duration"] = str(int(test_duration))

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


def _make_parser() -> argparse.ArgumentParser:

    parser = argparse.ArgumentParser(
        description="Utility to convert junit-style test reports into the SonarQube generic test execution format.",
    )

    parser.add_argument("-p",
                        "--pattern",
                        default="*.xml",
                        help="Pattern to match for test suites to run. (default: %(default)s)")
    parser.add_argument("-v",
                        "--verbose",
                        action="count",
                        default=0,
                        help="Increase logging verbosity. (default: %(default)s)")
    parser.add_argument("output",
                        type=pathlib.Path,
                        help="Output file to write the test report to. (default: %(default)s)")
    parser.add_argument("--dry-run",
                        action="store_true",
                        help="Do not write the test report to disk.")
    parser.add_argument("-s",
                        "--stop-on-failure",
                        action="store_true",
                        help="Stop on first test failure.")
    parser.add_argument("-i",
                        "--input",
                        action="append",
                        help="Input files to convert.")
    parser.add_argument("-b",
                        "--base-dir",
                        type=pathlib.Path,
                        help="Base directory to make file paths relative to.")
    return parser


# +---------------------------------------------------------------------------+


def append(test_report: pathlib.Path, sq_report: ET.Element, base_dir: typing.Optional[pathlib.Path] = None) -> int:
    try:
        _junit_to_sonarqube_generic_execution_format(test_report, sq_report, base_dir)
        return 0
    except ET.ParseError as err:
        logging.warning("Failed to parse report {}: {}".format(test_report, err))
        return 1


# +---------------------------------------------------------------------------+


def main() -> int:
    """
    Main method to execute when this package/script is invoked as a command.
    """
    args = _make_parser().parse_args()

    logging_level = logging.WARN

    if args.verbose == 2:
        logging_level = logging.INFO
    elif args.verbose >= 3:
        logging_level = logging.DEBUG

    logging.basicConfig(format="%(levelname)s: %(message)s", level=logging_level)

    if args.input is None:
        logging.error("No input files specified.")
        return 1

    test_executions = ET.Element("testExecutions", attrib={"version": "1"})
    sq_report = ET.ElementTree(test_executions)

    if args.pattern is not None:
        for test_report in pathlib.Path.cwd().glob(args.pattern):
            if test_report.suffix == ".xml":
                logging.info("Found report {}. Trying to combine into sonarqube report.".format(test_report))
                if append(test_report, sq_report.getroot(), args.base_dir) != 0 and args.stop_on_failure:
                    return 1

    for test_report in args.input:
        if append(test_report, sq_report.getroot(), args.base_dir) != 0 and args.stop_on_failure:
            return 1

    if args.dry_run:
        logging.info("Would have written a test report for {} files to {}".format(len(test_executions.findall("file")), args.output))
    else:
        logging.debug("About to write a test report for {} files to {}".format(len(test_executions.findall("file")), args.output))
        # ET.indent(sq_report) Not available in Python 3.8
        with open(args.output, "wb") as sq_report_file:
            sq_report.write(sq_report_file, encoding="UTF-8")

    return 0


# +---------------------------------------------------------------------------+


if __name__ == "__main__":
    sys.exit(main())
