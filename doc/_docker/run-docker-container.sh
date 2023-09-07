#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
WORKSPACE_DIR=`realpath $SCRIPT_DIR/../../..`

docker run -it --rm --name=my_doc_buils -w ${PWD} -v$WORKSPACE_DIR:$WORKSPACE_DIR my/doc_build:0.1 \
	$WORKSPACE_DIR/nrf/doc/_docker/setup-docker-user.sh `id -un` `id -u` `id -gn` `id -g` "$@"

