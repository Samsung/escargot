#!/bin/bash

ESCARGOT=$1
TEST_CASE=$2
DEBUGGER_CLIENT=$3
IS_CLIENT_SOURCE=$4
RESULT_TEMP=`mktemp ${TEST_CASE}.out.XXXXXXXXXX`

if [[ $IS_CLIENT_SOURCE == 0 ]]; then
  ${ESCARGOT} --start-debug-server ${TEST_CASE}.js &
  sleep 1s
  (cat "${TEST_CASE}.cmd" | ${DEBUGGER_CLIENT} --non-interactive) >${RESULT_TEMP} 2>&1
else
  ${ESCARGOT} --start-debug-server --debugger-wait-source &
  sleep 1s
  (cat "${TEST_CASE}.cmd" | ${DEBUGGER_CLIENT} --client-source ${TEST_CASE}.js --non-interactive) >${RESULT_TEMP} 2>&1
fi
diff -u ${TEST_CASE}.expected ${RESULT_TEMP}

STATUS_CODE=$?

rm -f ${RESULT_TEMP}

exit ${STATUS_CODE}
