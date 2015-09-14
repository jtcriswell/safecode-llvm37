#!/bin/sh

if [ -s $1 ]; then
    echo "FAILED"
else
    echo "PASSED"
fi