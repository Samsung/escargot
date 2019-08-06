#set -x
NPROC="$(nproc)"
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
	DIFF_CMD="diff -Z -B -i -a"
fi

TOOL_PATH=$TEST_ROOT/../../../test/vendortest/driver/chakracore
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

declare -a TABLE_RT_FILES
declare -a TABLE_RT_BASELINE
declare -a TABLE_RT_SKIP
declare -a TABLE_RT_TZSET
declare -a TABLE_RT_WAIT

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

run_tcs () {
	TC_SIZ=$1
	FOR_MAX=$TC_SIZ
	((FOR_MAX--))

	for IDX in $(seq 0 $FOR_MAX); do
		RT_FILES=${TABLE_RT_FILES[$IDX]}
		RT_BASELINE=${TABLE_RT_BASELINE[$IDX]}
		RT_SKIP=${TABLE_RT_SKIP[$IDX]}
		RT_TZSET=${TABLE_RT_TZSET[$IDX]}
		RT_TEMP_OUTPUT_FILE=$TEMPORARY_OUTPUT_FILE"$IDX"
		RT_TEMP_DIFF_FILE=$TEMPORARY_DIFF_FILE"$IDX"
		RT_CMD="$BIN $INCLUDE $RT_FILES"

		if [[ $RT_SKIP == "" ]]; then
			if [[ $RT_TZSET != "" ]]; then
				RT_CMD="env TZ=US/Pacific $RT_CMD";
			fi
#			$($RT_CMD > $RT_TEMP_OUTPUT_FILE 2>> $LOG_FILE) &
			$RT_CMD &> $RT_TEMP_OUTPUT_FILE &
			PID=$!
			TABLE_RT_WAIT[$IDX]=$PID
		fi

	done

	for IDX in $(seq 0 $FOR_MAX); do
		RT_FILES=${TABLE_RT_FILES[$IDX]}
		RT_BASELINE=${TABLE_RT_BASELINE[$IDX]}
		RT_SKIP=${TABLE_RT_SKIP[$IDX]}
		RT_TZSET=${TABLE_RT_TZSET[$IDX]}
		RT_TEMP_OUTPUT_FILE=$TEMPORARY_OUTPUT_FILE"$IDX"
		RT_TEMP_DIFF_FILE=$TEMPORARY_DIFF_FILE"$IDX"


		RT_CMD="$BIN $INCLUDE $RT_FILES"
		echo "==========================================================" >> $LOG_FILE
		echo -n "[$DIR] $RT_FILES .......... "

		echo "[$DIR] $RT_FILES" >> $LOG_FILE
		echo $RT_CMD >> $LOG_FILE

		if [[ $RT_SKIP != "" ]]; then
			((LOCAL_SKIP+=1))
			printf "Skip ($RT_SKIP)\n" | tee -a $LOG_FILE
		else
			if [[ $RT_TZSET != "" ]]; then
				RT_CMD="env TZ=US/Pacific $RT_CMD";
			fi
			ESCARGOT_EXIT_CODE=0
			wait ${TABLE_RT_WAIT[$IDX]} || ESCARGOT_EXIT_CODE=1
			sed -i 's/\[object global\]/[object Object]/g' "$RT_TEMP_OUTPUT_FILE"

			$($DIFF_CMD $RT_TEMP_OUTPUT_FILE $RT_BASELINE 2>&1 > $RT_TEMP_DIFF_FILE)
			DIFF_EXIT_CODE=$?

			if [[ $RT_BASELINE == $BASELINE_PATH/baseline1 ]]; then
				#echo $ESCARGOT_EXIT_CODE
				if [[ $ESCARGOT_EXIT_CODE == 0 ]]; then
					#cat $TEMPORARY_OUTPUT_FILE
					grep FAIL "$RT_TEMP_OUTPUT_FILE" > /dev/null
					if [[ $? == 0 ]]
					then
						DIFF_EXIT_CODE=1
					else
						DIFF_EXIT_CODE=0
					fi
				else
					DIFF_EXIT_CODE=1
				fi
			fi
			if [[ $RT_TZSET != "" ]]; then
				TZ=Asia/Seoul;
			fi
			if [[ "$DIFF_EXIT_CODE" != "0" ]]; then
				printf "Fail\n" | tee -a $LOG_FILE
				cat $RT_TEMP_OUTPUT_FILE >> $LOG_FILE
				((LOCAL_FAIL+=1))
			else
				printf "Pass\n" | tee -a $LOG_FILE
				((LOCAL_PASS+=1))
			fi
			rm -f $RT_TEMP_OUTPUT_FILE
			rm -f $RT_TEMP_OUTPUT_PATH
		fi
	done
}

run_dir() {
	DIR=$1
	idx=$2
	FILES=
	BASELINE=
	SKIP=
	TZSET=

	TC_INDEX=0
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

			RT_FILES=$REAL_FILES
			RT_BASELINE=$REAL_BASELINE
			RT_SKIP=$SKIP
			RT_TZSET=$TZSET

			TABLE_RT_FILES[$TC_INDEX]=$RT_FILES
			TABLE_RT_BASELINE[$TC_INDEX]=$RT_BASELINE
			TABLE_RT_SKIP[$TC_INDEX]=$RT_SKIP
			TABLE_RT_TZSET[$TC_INDEX]=$RT_TZSET

			((TC_INDEX+=1))

			if [[ $TC_INDEX == $NPROC ]]; then
				run_tcs $TC_INDEX
				TC_INDEX=0
			fi
		fi
	done < rlexe.xml;

	run_tcs $TC_INDEX

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
