#!/bin/sh

SRC_FN=$1
OUTPUT_FN=$2
BAD_LINENO=`grep -n -m1 "BAD" ${SRC_FN}|cut -f1 -d:|awk '{print $1+1; }'`
BAD_PATTERN=`basename ${SRC_FN}:${BAD_LINENO}`

CATCH_ERR=`grep -c -m1 "^SAFECode:" ${OUTPUT_FN}`

if [ ${CATCH_ERR} -eq 0 ]; then
	echo "FAILED"
else
	PATTERN_MATCH=`grep -c -m1 "${BAD_PATTERN}$" ${OUTPUT_FN}`
	if [ ${PATTERN_MATCH} -eq 0 ]; then
		echo "MISMATCH"
	else
		echo "PASSED"
	fi
fi