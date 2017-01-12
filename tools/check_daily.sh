#!/bin/bash

echo "======================================================="

echo "checking.. make run-test262"
TEST262_RESULT="out/test262_result"
OCTANE_RESULT="out/octane_result"
SUNSPIDER_RESULT="out/sunspider_result"
if [[ -z "$TARGET" ]]; then
    TARGET="escargot"
fi
make run-test262 > $TEST262_RESULT
if (( $(grep -c 'All tests succeeded' $TEST262_RESULT) == 0 )); then
   echo "[FAIL] make run-test262"
   cat $TEST262_RESULT | grep -vE "^ch|^Excluded|^annexB"
   exit 1
fi

echo "checking.. octane"
./tools/measure.sh $TARGET.interpreter octane time > $OCTANE_RESULT
if (( $(grep -cE 'Error|NaN' $OCTANE_RESULT) != 0 )); then
   echo "[FAIL] octane"
   cat $OCTANE_RESULT
   exit 1
fi

echo "checking.. sunspider"
./tools/measure.sh $TARGET.interpreter time > $SUNSPIDER_RESULT
if (( $(grep -cE 'Error|NaN' $SUNSPIDER_RESULT) != 0 )); then
   echo "[FAIL] sunspider"
   cat $SUNSPIDER_RESULT
   exit 1
fi

echo "======== Succeeded test262, octane, sunspider ========="
cat $SUNSPIDER_RESULT
cat $OCTANE_RESULT
cat $TEST262_RESULT | grep -vE "^ch|^Excluded|^annexB"
echo "======================================================="
