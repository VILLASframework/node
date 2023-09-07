#!/usr/bin/env bash
#
# Install / run pre-commit hook in repo
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

format_file() {
  FILE="${1}"
  if [ -f ${FILE} ]; then
    if ! clang-format --Werror --dry-run ${FILE}; then
      clang-format -i ${FILE}
      return 1
    fi
  fi
}

case "${1}" in
  help)
    echo "Run ${0} install to install the git commit hooks"
    ;;

  install)
    TOP_DIR=$(git rev-parse --show-toplevel)
    cp "${BASH_SOURCE[0]}" "${TOP_DIR}/.git/hooks/pre-commit"
    ;;

  *)
    FILES=$(git diff-index --cached --name-only HEAD | grep -iE '\.(cpp|hpp|c|h)$')
    CHANGES=0
    for FILE in ${FILES}; do
      if ! format_file "${FILE}"; then
        CHANGES=$((CHANGES+1))
      fi
    done

    if (( ${CHANGES} > 0 )); then
      echo "Formatting of ${CHANGES} files has been fixed. Please stage and commit again."
      exit -1
    fi
    ;;
esac
