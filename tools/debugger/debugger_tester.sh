#!/bin/bash

ESCARGOT=$1
TEST_CASE=$2
DEBUGGER_CLIENT=$3
IS_CLIENT_SOURCE=$4
RESULT_TEMP=`mktemp ${TEST_CASE}.out.XXXXXXXXXX`
if [[ $IS_CLIENT_SOURCE == 0  ]] && [[ $TEST_CASE != *"tools/debugger/tests/client_source_multiple"* ]]; then
  ${ESCARGOT} --start-debug-server ${TEST_CASE}.js &
  sleep 1s
  if [[ $TEST_CASE == *"tools/debugger/tests/do_command"* ]]; then
    (cat "${TEST_CASE}.cmd" | ${DEBUGGER_CLIENT} --non-interactive --command "e i;b do_command.js:2;c;c") >${RESULT_TEMP} 2>&1
  else
    (cat "${TEST_CASE}.cmd" | ${DEBUGGER_CLIENT} --non-interactive) >${RESULT_TEMP} 2>&1
  fi
  diff -u ${TEST_CASE}.expected ${RESULT_TEMP}
elif [[ $IS_CLIENT_SOURCE == 1 ]]; then
  ${ESCARGOT} --start-debug-server --debugger-wait-source &
  sleep 1s
  if [[ $TEST_CASE == *"client_source"* ]]; then
    (cat "${TEST_CASE}.cmd" | ${DEBUGGER_CLIENT} --client-source ${TEST_CASE}_2.js ${TEST_CASE}_1.js --non-interactive) >${RESULT_TEMP} 2>&1
  elif [[ $TEST_CASE == *"tools/debugger/tests/do_command"* ]]; then
    (cat "${TEST_CASE}.cmd" | ${DEBUGGER_CLIENT} --client-source ${TEST_CASE}.js --non-interactive --command "e i;b do_command.js:2;c;c") >${RESULT_TEMP} 2>&1
  else
    (cat "${TEST_CASE}.cmd" | ${DEBUGGER_CLIENT} --client-source ${TEST_CASE}.js --non-interactive) >${RESULT_TEMP} 2>&1
  fi
  diff -u ${TEST_CASE}.expected ${RESULT_TEMP}
fi


STATUS_CODE=$?

rm -f ${RESULT_TEMP}

exit ${STATUS_CODE}
