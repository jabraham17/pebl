#!/usr/bin/env bash

DIR=`dirname $0`
ROOTDIR=$(cd $DIR/..; pwd)
set -x
python3 $DIR/run_tests.py -D ROOT=$ROOTDIR $@
