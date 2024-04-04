#/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

cp -Rf $SCRIPT_DIR/* $SCRIPT_DIR/../zephyr/scripts/api_check/
