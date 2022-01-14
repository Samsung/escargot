#!/bin/bash

ESCARGOT=$1
TEST_CASE=$2
DEBUGGER_CLIENT=$3
IS_CLIENT_SOURCE=$4
RESULT_TEMP=`mktemp ${TEST_CASE}.out.XXXXXXXXXX`
if [[ $IS_CLIENT_SOURCE == 0  ]] && [[ $TEST_CASE != *"tools/debugger/tests/client_source_multiple"* ]]; then
  if [[ $TEST_CASE == *"tools/debugger/tests/do_wait_exit2"* ]]; then
    ${ESCARGOT} --start-debug-server --wait-before-exit ${TEST_CASE}.js &
  else
    ${ESCARGOT} --start-debug-server ${TEST_CASE}.js &
  fi
  sleep 1s
  if [[ $TEST_CASE == *"tools/debugger/tests/do_command"* ]]; then
    (cat "${TEST_CASE}.cmd" | ${DEBUGGER_CLIENT} --non-interactive --command "b do_command.js:2;c;e i;c") >${RESULT_TEMP} 2>&1
  elif [[ $TEST_CASE == *"tools/debugger/tests/do_wait_exit1"* ]]; then
    (cat "${TEST_CASE}.cmd" | ${DEBUGGER_CLIENT} --non-interactive --wait-before-exit 1) >${RESULT_TEMP} 2>&1
  else
    (cat "${TEST_CASE}.cmd" | ${DEBUGGER_CLIENT} --non-interactive) >${RESULT_TEMP} 2>&1
  fi
  diff -u ${TEST_CASE}.expected ${RESULT_TEMP}
elif [[ $IS_CLIENT_SOURCE == 1 ]]; then
  if [[ $TEST_CASE == *"tools/debugger/tests/do_wait_exit2"* ]]; then
    ${ESCARGOT} --start-debug-server --debugger-wait-source --wait-before-exit &
  else
    ${ESCARGOT} --start-debug-server --debugger-wait-source &
  fi
  sleep 1s
  if [[ $TEST_CASE == *"client_source"* ]]; then
    (cat "${TEST_CASE}.cmd" | ${DEBUGGER_CLIENT} --client-source ${TEST_CASE}_2.js ${TEST_CASE}_1.js --non-interactive) >${RESULT_TEMP} 2>&1
  elif [[ $TEST_CASE == *"tools/debugger/tests/do_command"* ]]; then
    (cat "${TEST_CASE}.cmd" | ${DEBUGGER_CLIENT} --client-source ${TEST_CASE}.js --non-interactive --command "b do_command.js:2;c;e i;c") >${RESULT_TEMP} 2>&1
  elif [[ $TEST_CASE == *"tools/debugger/tests/do_wait_exit1"* ]]; then
    (cat "${TEST_CASE}.cmd" | ${DEBUGGER_CLIENT} --client-source ${TEST_CASE}.js --non-interactive --wait-before-exit 1) >${RESULT_TEMP} 2>&1
  else
    (cat "${TEST_CASE}.cmd" | ${DEBUGGER_CLIENT} --client-source ${TEST_CASE}.js --non-interactive) >${RESULT_TEMP} 2>&1
  fi
  diff -u ${TEST_CASE}.expected ${RESULT_TEMP}
fi


STATUS_CODE=$?

rm -f ${RESULT_TEMP}

exit ${STATUS_CODE}
