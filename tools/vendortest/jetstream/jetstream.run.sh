#!/bin/bash

TEST_ROOT=$(pwd)
TOOL_PATH=$TEST_ROOT/../../../tools/vendortest/jetstream
PATH_TO_ENGINE=$1

DATE=$(date +%y%m%d_%H_%M_%S)
OUTFILE_WITH_TIME=$(echo $TOOL_PATH"/jetstream-result-raw_"$DATE".res")
OUTFILE_DEFAULT=$(echo $TOOL_PATH"/jetstream-result-raw.res")

i=0
echo "" > $OUTFILE_WITH_TIME;
if [[ $PATH_TO_ENGINE = *"escargot" ]]; then
    while [ $i -lt 37 ]; do
        cat ./runOnePlan.js | sed -e 's/arguments\[0\]/'$i'/g' > runOnePlan_for_escargot.js
        if [[ $i -lt 11 ]]; then
            k=0;
            while [ $k -lt 10 ]; do
                $PATH_TO_ENGINE ./runOnePlan_for_escargot.js > tmp;
                tail -1 tmp >> $OUTFILE_WITH_TIME;
                k=$(( k + 1 ))
            done
        else
            $PATH_TO_ENGINE ./runOnePlan_for_escargot.js > tmp;
            if [[ i -eq 27 || i -eq 30 ]]; then     # 27: splay / 30: mandreel
                tail -2 tmp >> $OUTFILE_WITH_TIME;  # splay, splay-latency / mandreel, mandreel-latency
            else
                tail -1 tmp >> $OUTFILE_WITH_TIME;
            fi
        fi
        i=$(( i + 1 ))
    done
    rm runOnePlan_for_escargot.js
    rm tmp;
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
