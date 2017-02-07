#TEST_ROOT=$(pwd)
TEST_ROOT=$(dirname $(readlink -f $0))
BIN=

if [[ $(uname -m) == *"arm"* ]]; then
	ARM=1
else
	ARM=0
fi

if [[ "$ARM" == "1" ]]; then
	DIFF_CMD=$TEST_ROOT/diff.py
else
	DIFF_CMD="diff -Z -i -a"
fi

TOOL_PATH=$TEST_ROOT/../../../tools/vendortest/chakracore
BASELINE_PATH=$TOOL_PATH/baseline
TEMPORARY_OUTPUT_FILE=$TOOL_PATH/tmp
TEMPORARY_DIFF_FILE=$TOOL_PATH/diff
LOG_FILE=$TOOL_PATH/chakracorelog.verbose.txt

INCLUDE=" $TEST_ROOT/include.js $TEST_ROOT/UnitTestFramework/UnitTestFramework.js"

ARRAY_KEYS=()
declare -a TABLE_COUNT
declare -a TABLE_PASS
declare -a TABLE_FAIL
declare -a TABLE_SKIP

print_usage() {
	echo "$ cd test/chakracore"
	echo "$ ./run.sh \$BIN \$TESTDIR "
}

read_dom() {
	local IFS=\>
	read -d \< ENTITY CONTENT
}

print_count() {
	TESTNAME=$1
	COUNT=$2
	PASS=$3
	FAIL=$4
	SKIP=$5

	PASS_RATIO=$((PASS*100/COUNT))

	printf "%20s\t\t%d\t\t%d\t\t%d\t\t%d\t\t(%d%%)\n" $TESTNAME $COUNT		$PASS		$FAIL		$SKIP		$PASS_RATIO
}

run_test() {
	FILES=$1
	BASELINE=$2
	SKIP=$3
	TZSET=$4

	CMD="$BIN $INCLUDE $FILES"
	echo "==========================================================" >> $LOG_FILE
	echo -n "[$DIR] $FILES .......... "

	echo "[$DIR] $FILES" >> $LOG_FILE
	echo $CMD >> $LOG_FILE

	if [[ $SKIP != "" ]]; then
		((LOCAL_SKIP+=1))
		printf "Skip ($SKIP)\n" | tee -a $LOG_FILE
	else
		if [[ $TZSET != "" ]]; then
			CMD="env TZ=US/Pacific $CMD";
		fi
		$($CMD \
			| sed 's/\[object global\]/[object Object]/g' \
			> $TEMPORARY_OUTPUT_FILE 2>> $LOG_FILE)
		$($DIFF_CMD $TEMPORARY_OUTPUT_FILE $BASELINE 2>&1 > $TEMPORARY_DIFF_FILE)
		DIFF_EXIT_CODE=$?
		if [[ $BASELINE == $BASELINE_PATH/baseline1 ]]; then
			if [[ "$DIFF_EXIT_CODE" != "0" ]]; then
				$($DIFF_CMD $TEMPORARY_OUTPUT_FILE $BASELINE_PATH/baseline2 2>&1 > $TEMPORARY_DIFF_FILE)
				DIFF_EXIT_CODE=$?
			fi
			if [[ "$DIFF_EXIT_CODE" != "0" ]]; then
				$($DIFF_CMD $TEMPORARY_OUTPUT_FILE $BASELINE_PATH/baseline3 2>&1 > $TEMPORARY_DIFF_FILE)
				DIFF_EXIT_CODE=$?
			fi
			if [[ "$DIFF_EXIT_CODE" != "0" ]]; then
				$($DIFF_CMD $TEMPORARY_OUTPUT_FILE $BASELINE_PATH/baseline4 2>&1 > $TEMPORARY_DIFF_FILE)
				DIFF_EXIT_CODE=$?
			fi
			if [[ "$DIFF_EXIT_CODE" != "0" ]]; then
				$($DIFF_CMD $TEMPORARY_OUTPUT_FILE $BASELINE_PATH/baseline5 2>&1 > $TEMPORARY_DIFF_FILE)
				DIFF_EXIT_CODE=$?
			fi
		fi
		if [[ $TZSET != "" ]]; then
			TZ=Asia/Seoul;
		fi

		if [[ "$DIFF_EXIT_CODE" != "0" ]]; then
			printf "Fail\n" | tee -a $LOG_FILE
			cat $TEMPORARY_DIFF_FILE >> $LOG_FILE
			((LOCAL_FAIL+=1))
		else
			printf "Pass\n" | tee -a $LOG_FILE
			((LOCAL_PASS+=1))
		fi
	fi
}

run_dir() {
	DIR=$1
	idx=$2
	FILES=
	BASELINE=
	SKIP=
	TZSET=

	LOCAL_COUNT=0
	LOCAL_PASS=0
	LOCAL_FAIL=0
	LOCAL_SKIP=0
	cd ${TEST_ROOT}/$DIR;
	while read_dom; do
		if [[ $ENTITY == test ]]; then
			FILES=
			BASELINE=
			SKIP=
			TZSET=
		fi
		if [[ $ENTITY == files ]]; then
			((LOCAL_COUNT+=1))
			FILES=$CONTENT
		fi
		if [[ $ENTITY == baseline ]]; then
			BASELINE=$CONTENT
		fi
		if [[ $ENTITY == escargot-skip ]]; then
			SKIP=$CONTENT
		fi
		if [[ $ENTITY == escargot-tizen-wearable-skip && "$ARM" == "1" ]]; then
			SKIP=$CONTENT
		fi
		if [[ $ENTITY == compile-flags ]]; then # overwrite some variables
			if [[ $CONTENT == *-dump:bytecode* ]]; then
				BASELINE=
			fi
			if [[ $CONTENT == *-trace:FunctionSourceInfoParse* ]]; then
				BASELINE=
			fi
			if [[ $CONTENT == *-ForceStrictMode* ]]; then
				SKIP=$CONTENT
			fi
		fi
		if [[ $ENTITY == *timezone-sensitive* ]]; then
			TZSET=Los_Angeles
		fi
		if [[ "$ENTITY" == "/test" && "$FILES" != "" ]]; then
			REAL_FILES=$(find . -iname $FILES -printf "%P\n")
			if [[ $BASELINE == "" ]]; then
				REAL_BASELINE=$BASELINE_PATH/baseline1
			else
				REAL_BASELINE=$(find . -iname $BASELINE -printf "%P\n")
			fi
			run_test "$REAL_FILES" "$REAL_BASELINE" "$SKIP" "$TZSET"
		fi
	done < rlexe.xml;

	ARRAY_KEYS+=($DIR)
	TABLE_COUNT[$idx]=$LOCAL_COUNT
	TABLE_PASS[$idx]=$LOCAL_PASS
	TABLE_FAIL[$idx]=$LOCAL_FAIL
	TABLE_SKIP[$idx]=$LOCAL_SKIP

	cd ..
}

main() {
    BIN=$1
	TESTDIR=$2
	idx=0

	echo "" > $LOG_FILE

	if [[ !(-x $BIN) ]]; then
		echo "JS Engine $BIN is not valid"
		print_usage
		exit 1
	fi

	if [[ $TESTDIR != "" ]]; then
		echo "Running $TESTDIR" | tee -a $LOG_FILE
		REAL_TESTDIR=$(find . -iname $TESTDIR -printf "%P\n")
		if [[ -d $REAL_TESTDIR ]]; then
			run_dir $REAL_TESTDIR $idx
		else
			echo "Invalid dir $TESTDIR"
			exit 1
		fi
	else
		echo "Running all tests" | tee -a $LOG_FILE
		TESTDIR=
		SKIP=
		while read_dom; do
			if [[ $ENTITY == dir ]]; then
				TESTDIR=
				SKIP=
			fi
			if [[ $ENTITY == files ]]; then
				TESTDIR=$CONTENT
			fi
			if [[ $ENTITY == escargot-skip ]]; then
				SKIP=$CONTENT
			fi
			if [[ $ENTITY == /dir ]]; then
                REAL_TESTDIR=$(find ${TEST_ROOT} -iname $TESTDIR -printf "%P\n")
				if [[ $SKIP == "" ]]; then
					run_dir $REAL_TESTDIR $idx
					((idx+=1))
				else
					echo "[$REAL_TESTDIR] Skipping whole directory ($SKIP)"
				fi
			fi
		done < ${TEST_ROOT}/rlexedirs.xml
	fi

	TOTAL_COUNT=0
	TOTAL_PASS=0
	TOTAL_FAIL=0
	TOTAL_SKIP=0
	echo "==========================================================" | tee -a $LOG_FILE
	echo "TESTNAME					TOTAL	PASS	FAIL	SKIP	PASS_RATIO"
	idx=0
	for key in ${ARRAY_KEYS[@]}; do
		print_count $key ${TABLE_COUNT[$idx]} ${TABLE_PASS[$idx]} ${TABLE_FAIL[$idx]} ${TABLE_SKIP[$idx]}
		((TOTAL_COUNT+=${TABLE_COUNT[$idx]}))
		((TOTAL_PASS+=${TABLE_PASS[$idx]}))
		((TOTAL_FAIL+=${TABLE_FAIL[$idx]}))
		((TOTAL_SKIP+=${TABLE_SKIP[$idx]}))
		((idx+=1))
	done
	echo "==========================================================" | tee -a $LOG_FILE
	print_count "Total" $TOTAL_COUNT $TOTAL_PASS $TOTAL_FAIL $TOTAL_SKIP
}

main $(pwd)/$1 $2
rm $TEMPORARY_OUTPUT_FILE
rm $TEMPORARY_DIFF_FILE

