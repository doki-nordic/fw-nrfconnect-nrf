#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
WORKSPACE_DIR=`realpath $SCRIPT_DIR/../../..`

. $WORKSPACE_DIR/zephyr/zephyr-env.sh
"$@"
