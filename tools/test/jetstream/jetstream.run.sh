#!/bin/bash

TEST_ROOT=$(pwd)
TOOL_PATH=$TEST_ROOT/../../../tools/test/jetstream
PATH_TO_ENGINE=$1

DATE=$(date +%y%m%d_%H_%M_%S)
OUTFILE_WITH_TIME=$(echo $TOOL_PATH"/jetstream-result-raw_"$DATE"_"$$".res")
OUTFILE_DEFAULT=$(echo $TOOL_PATH"/jetstream-result-raw.res")

TARGET_TEST=$2
SUNSPIDER_MIN=0
SUNSPIDER_MAX=10

SIMPLE_MIN=11
SIMPLE_MAX=19

SIMPLE_1_MIN=11
SIMPLE_1_MAX=13
SIMPLE_2_MIN=14
SIMPLE_2_MAX=16
SIMPLE_3_MIN=17
SIMPLE_3_MAX=19

OCTANE_MIN=20
OCTANE_MAX=35

CDJS_MIN=36
CDJS_MAX=36

i=0
echo "" > $OUTFILE_WITH_TIME;
if [[ $PATH_TO_ENGINE = *"escargot" ]]; then
    for ((i=0; i<37; i++)); do
        if [[ $TARGET_TEST = "sunspider" ]]; then
            if [ "$i" -lt "$SUNSPIDER_MIN" -o "$i" -gt "$SUNSPIDER_MAX" ]; then
                continue
            fi
        elif [[ $TARGET_TEST = "octane" ]]; then
            if [ "$i" -lt "$OCTANE_MIN" -o "$i" -gt "$OCTANE_MAX" ]; then
                continue
            fi
        elif [[ $TARGET_TEST = "simple" ]]; then
            if [ "$i" -lt "$SIMPLE_MIN" -o "$i" -gt "$SIMPLE_MAX" ]; then
                continue
            fi
        elif [[ $TARGET_TEST = "simple-1" ]]; then
            if [ "$i" -lt "$SIMPLE_1_MIN" -o "$i" -gt "$SIMPLE_1_MAX" ]; then
                continue
            fi
        elif [[ $TARGET_TEST = "simple-2" ]]; then
            if [ "$i" -lt "$SIMPLE_2_MIN" -o "$i" -gt "$SIMPLE_2_MAX" ]; then
                continue
            fi
        elif [[ $TARGET_TEST = "simple-3" ]]; then
            if [ "$i" -lt "$SIMPLE_3_MIN" -o "$i" -gt "$SIMPLE_3_MAX" ]; then
                continue
            fi
        elif [[ $TARGET_TEST = "cdjs" ]]; then
            if [ "$i" -lt "$CDJS_MIN" -o "$i" -gt "$CDJS_MAX" ]; then
                continue
            fi
        fi
        cat ./runOnePlan.js | sed -e 's/arguments\[0\]/'$i'/g' > runOnePlan_for_escargot_$$.js
        if [[ $i -lt 11 ]]; then
            k=0;
            while [ $k -lt 10 ]; do
                $PATH_TO_ENGINE ./runOnePlan_for_escargot_$$.js > tmp_$$;
                tail -1 tmp_$$ >> $OUTFILE_WITH_TIME;
                k=$(( k + 1 ))
            done
        else
            $PATH_TO_ENGINE ./runOnePlan_for_escargot_$$.js > tmp_$$;
            if [[ i -eq 27 || i -eq 30 ]]; then     # 27: splay / 30: mandreel
                tail -2 tmp_$$ >> $OUTFILE_WITH_TIME;  # splay, splay-latency / mandreel, mandreel-latency
            else
                tail -1 tmp_$$ >> $OUTFILE_WITH_TIME;
            fi
        fi
    done
    rm runOnePlan_for_escargot_$$.js
    rm tmp_$$;
else
    while [ $i -lt 37 ]; do
        $PATH_TO_ENGINE ./runOnePlan.js -- $i > tmp;
        if [[ i -eq 27 || i -eq 30 ]]; then
#            tail -2 tmp;
            tail -2 tmp >> $OUTFILE_WITH_TIME;
        else
#            tail -1 tmp;
            tail -1 tmp >> $OUTFILE_WITH_TIME;
        fi
        i=$(( i + 1 ))
    done
   rm tmp;
fi

cp $OUTFILE_WITH_TIME $OUTFILE_DEFAULT;
#cat $OUTFILE_WITH_TIME;
