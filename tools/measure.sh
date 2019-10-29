#!/bin/bash

#make full
echo "======================================================="
REPO_BASE=$(pwd)
if [[ -z "$TESTDIR_BASE" ]]; then
    TESTDIR_BASE="test/"
fi
if [[ -z "$MODE" ]]; then
    MODE="release"
fi
TESTDIR_BASE=$(realpath $TESTDIR_BASE)
TESTBIN="$TESTDIR_BASE/bin/"
MEMPS="$TESTBIN/memps"
OCTANE_BASE="test/octane"
SUNSPIDER_BASE="test/vendortest/SunSpider"
tests=("3d-cube" "3d-morph" "3d-raytrace" "access-binary-trees" "access-fannkuch" "access-nbody" "access-nsieve" "bitops-3bit-bits-in-byte" "bitops-bits-in-byte" "bitops-bitwise-and" "bitops-nsieve-bits" "controlflow-recursive" "crypto-aes" "crypto-md5" "crypto-sha1" "date-format-tofte" "date-format-xparb" "math-cordic" "math-partial-sums" "math-spectral-norm" "regexp-dna" "string-base64" "string-fasta" "string-tagcloud" "string-unpack-code" "string-validate-input")
ARCH="x64"
if [[ $1 == *"32"* ]]; then
  ARCH="x86"
fi

if [[ $1 == duk* ]]; then
  export LD_LIBRARY_PATH="$TESTBIN/$ARCH/duk/"
  cmd="$TESTBIN/$ARCH/duk_1.6.0/duk"
  tc="duktape.$ARCH"
elif [[ $1 == v8* ]]; then
  cmd="$TESTBIN/$ARCH/v8_5.7.477/d8"
  if [[ $1 == *.full* ]]; then
    args="--nocrankshaft"
    tc="v8.$ARCH.full"
  else
    tc="v8.$ARCH"
  fi
  if [[ $2 == time ]]; then
      cp test/vendortest/driver/sunspider-standalone-driver-v8.js $SUNSPIDER_BASE/resources/sunspider-standalone-driver.js
  fi
elif [[ $1 == jsc* ]]; then
  JSC_PATH="$TESTBIN/$ARCH/jsc_170222"
  if [[ $1 == jsc*.interp* ]]; then
    export LD_LIBRARY_PATH="$JSC_PATH/interp/lib:$JSC_PATH/libicu"
    cmd="$JSC_PATH/interp/jsc"
    tc="jsc.$ARCH.interp"
  elif [[ $1 == jsc*.full.jit ]] || [[ $1 == jsc*.base ]]; then
    export LD_LIBRARY_PATH="$JSC_PATH/baseline/lib:$JSC_PATH/libicu"
    cmd="$JSC_PATH/baseline/jsc"
    args="--useDFGJIT=false"
    tc="jsc.$ARCH.baselineJIT"
  elif [[ $1 == jsc*.jit ]]; then
    export LD_LIBRARY_PATH="$JSC_PATH/baseline/lib:$JSC_PATH/libicu"
    cmd="$JSC_PATH/baseline/jsc"
    tc="jsc.$ARCH.jit"
  fi
elif [[ $1 == escargot*.interp* ]]; then
  make $ARCH.interpreter.$MODE -j8
  cmd="$REPO_BASE/out/linux/$ARCH/interpreter/$MODE/escargot"
  tc="escargot.$ARCH.interp"
#elif [[ $1 == escargot*.jit ]]; then
#  make $ARCH.jit.release -j8
#  cmd="$REPO_BASE/out/linux/$ARCH/jit/release/escargot"
#  tc="escargot.$ARCH.jit"
else
  echo "choose one between ([escargot|jsc|v8](32)?.(full)?.[interp|jit|base])|duk"
  exit 1
fi
echo "== REPO BASE PATH: "$REPO_BASE
echo "== TEST BASE PATH: "$TESTDIR_BASE
echo "== BINARY PATH: "$cmd $args
echo "======================================================="
git submodule deinit -f test/octane
git submodule deinit -f test/vendortest
git submodule init
git submodule update
testpath="$SUNSPIDER_BASE/tests/sunspider-1.0.2/"

TEST_RESULT_PATH="$TESTDIR_BASE/out/"
mkdir -p $TEST_RESULT_PATH
rm $TEST_RESULT_PATH/*.out -f
num=$(date +%y%m%d_%H_%M_%S)
memresfile=$(echo $TEST_RESULT_PATH$tc'_mem_'$num'.res')

function measure(){
  finalcmd=$1" "$2" "$3
  eval $finalcmd
  PID=$!
  #echo $PID
  (while [ "$PID" ]; do
    [ -f "/proc/$PID/smaps" ] || { exit 1;};
    $MEMPS -p $PID 2> /dev/null
    echo \"=========\"; sleep 0.0001;
  done ) >> $outfile &
  sleep 1s;
  echo $t >> $memresfile
  MAXV=$(grep 'PSS:' $outfile | sed -e 's/,//g' | awk '{ if(max < $2) max=$2} END { print max}')
  MAXR=$(grep 'RSS:' $outfile | sed -e 's/,//g' | awk '{ if(max < $2) max=$2} END { print max}')
  echo 'MaxPSS:'$MAXV', MaxRSS:'$MAXR >> $memresfile
  rm $outfile
  echo $MAXV
}

timeresfile=$(echo $TEST_RESULT_PATH$tc'_time_'$num'.res')
echo '' > $timeresfile
if [[ $2 == octane ]]; then
  if [[ $3 != time ]]; then
    echo "== Measure Octane Memory =="
    outfile=$(echo $TEST_RESULT_PATH$1"_octane_memory.out")
    cd $OCTANE_BASE
    /usr/bin/time -v $cmd $args run.js &> $outfile
    grep "^\s" $outfile > $TEST_RESULT_PATH/octane_mem.res
    grep "Maximum resident set size" $outfile | sed -e 's/^[[:space:]]*//'
    cd -
  fi

  if [[ $3 != mem* ]]; then
      echo "== Measure Octane Score =="
      cd $OCTANE_BASE
      $cmd $args run.js | tee $timeresfile
      cp $timeresfile $TEST_RESULT_PATH/octane_score.res
      cd -
  fi
else
    if [[ $2 != mem* ]]; then
      echo "== Measure Sunspider Time =="
      cd $SUNSPIDER_BASE
      ./sunspider --shell=$cmd --suite=sunspider-1.0.2 --args="$args" | tee $timeresfile
      tmpresult=$(grep "Results are located at" $timeresfile | cut -d' ' -f5)
      cp $tmpresult $TEST_RESULT_PATH/sunspider_time.res
      cd -
    fi

    if [[ $2 != time ]]; then
      echo "== Measure Sunspider Memory =="
      echo '' > tmp
      for t in "${tests[@]}"; do
        sleep 1s;
        filename=$(echo $testpath$t'.js')
        outfile=$(echo $TEST_RESULT_PATH$t".out")
        echo '-----'$t
        finalcmd="$cmd $filename &"
        summem=""
        echo '===================' >> $memresfile
        for j in {1..5}; do
          MAXV='Error'
          measure $finalcmd
          summem=$summem$MAXV"\\n"
          sleep 0.5s;
        done
        echo $(echo -e $summem | awk '{s+=$1;if(NR==1||max<$1){max=$1};if(NR==1||($1!="" && $1<min)){min=$1}} END {printf("Avg. MaxPSS: %.4f", (s-max-min)/3)}')
        echo $t $(echo -e $summem | awk '{s+=$1;if(NR==1||max<$1){max=$1};if(NR==1||($1!="" && $1<min)){min=$1}} END {printf(": %.4f", (s-max-min)/3)}') >> tmp
      done
      awk '{s+=$3} END {print s/26}' tmp >> tmp
      cat tmp
      cat tmp > $TEST_RESULT_PATH/memory.res
      rm tmp
    fi
fi
echo '-------------------------------------------------finish exe'
