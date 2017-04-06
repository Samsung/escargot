#!/bin/bash

echo "======================================================="

V8_RESULT="out/v8_result"
SPIDERMONKEY_RESULT="out/spidermonkey_result"
TEST262_RESULT="out/test262_result"
OCTANE_RESULT="out/octane_result"
SUNSPIDER_RESULT="out/sunspider_result"
if [[ -z "$TARGET" ]]; then
    TARGET="escargot"
fi

echo "checking.. make run-v8"
make run-v8 > $V8_RESULT
if (( $(grep -cE 'FAIL|CRASH' $V8_RESULT) != 0 )); then
   echo "[FAIL] make run-v8"
   cat $V8_RESULT
   exit 1
fi

echo "checking.. make run-spidermonkey-full"
make run-spidermonkey-full > $SPIDERMONKEY_RESULT
if (( $(grep -c 'run-spidermonkey-full.*failed' $SPIDERMONKEY_RESULT) != 0 )); then
   echo "[FAIL] make run-spidermonkey-full"
   cat $SPIDERMONKEY_RESULT
   exit 1
fi

echo "checking.. make run-test262"
make run-test262 > $TEST262_RESULT
if (( $(grep -c 'All tests succeeded' $TEST262_RESULT) == 0 )); then
   echo "[FAIL] make run-test262"
   cat $TEST262_RESULT | grep -vE "^ch|^Excluded|^annexB"
   exit 1
fi

echo "checking.. octane"
make run-octane > $OCTANE_RESULT
if (( $(grep -cE 'Error|NaN' $OCTANE_RESULT) != 0 )); then
   echo "[FAIL] octane"
   cat $OCTANE_RESULT
   exit 1
fi

echo "checking.. sunspider"
MODE=$MODE ./tools/measure.sh $TARGET.interpreter time > $SUNSPIDER_RESULT
if (( $(grep -cE 'Error|NaN' $SUNSPIDER_RESULT) != 0 )); then
   echo "[FAIL] sunspider"
   cat $SUNSPIDER_RESULT
   exit 1
fi

echo "======== Succeeded v8, spidermonkey, test262, octane, sunspider ========="
cat $SUNSPIDER_RESULT
cat $OCTANE_RESULT
cat $TEST262_RESULT | grep -vE "^ch|^Excluded|^annexB"
cat $SPIDERMONKEY_RESULT
cat $V8_RESULT
echo "======================================================="
