#!/bin/bash

ESCARGOT=$1
TEST_CASE=$2
DEBUGGER_CLIENT=$3
${ESCARGOT} --start-debug-server ${TEST_CASE}.js &
sleep 1s
RESULT_TEMP=`mktemp ${TEST_CASE}.out.XXXXXXXXXX`

(cat "${TEST_CASE}.cmd" | ${DEBUGGER_CLIENT} --non-interactive) >${RESULT_TEMP} 2>&1
diff -U0  ${RESULT_TEMP} ${TEST_CASE}.expected
STATUS_CODE=$?

rm -f ${RESULT_TEMP}

exit ${STATUS_CODE}
