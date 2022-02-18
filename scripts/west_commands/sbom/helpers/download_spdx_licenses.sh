#!/bin/sh

# Helper script for updating licenses.json file to the newest SPDX license list revision.

wget https://raw.githubusercontent.com/spdx/license-list-data/master/json/licenses.json -O ../licenses.json
