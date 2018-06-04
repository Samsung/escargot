CMAKE_MINIMUM_REQUIRED (VERSION 2.8)
 

ADD_CUSTOM_TARGET (check
                   COMMENT "check"
	               COMMAND @make tidy
                  )

ADD_CUSTOM_TARGET (tidy-install
                   COMMENT "tidy-install"
                   COMMAND @apt-get install clang-format-3.8
                  )

ADD_CUSTOM_TARGET (tidy
                   COMMENT "tidy"
                   COMMAND @python tools/check_tidy.py
                  )

ADD_CUSTOM_TARGET (tidy-update
                   COMMENT "tidy-update"
                   COMMAND @python tools/check_tidy.py update
                  )


# Targets : benchmarks

ADD_CUSTOM_TARGET (run-sunspider
                   COMMENT "run-sunspider"
                   COMMAND @cd test/vendortest/SunSpider/ && ./sunspider --shell=../../../escargot --suite=sunspider-1.0.2
                  )


ADD_CUSTOM_TARGET (run-sunspider-js
                   COMMENT "run-sunspider-js"
                   COMMAND @./escargot test/vendortest/SunSpider/tests/sunspider-1.0.2/*.js
                  )


ADD_CUSTOM_TARGET (run-octane
                   COMMENT "run-octane"
                   COMMAND @cd test/octane/ && ../../escargot run.js | tee > ../../test/octane_result 
                   COMMAND @cat test/octane_result | grep -c 'Score' > /dev/null;
                  )


# Targets : Regression test

ADD_CUSTOM_TARGET (run-internal-test
                   COMMENT "run-internal-test"	
                   COMMAND @cp ./test/vendortest/driver/internal-test-cases.txt ./test/vendortest/internal/internal-test-cases.txt 
	               COMMAND @cp ./test/vendortest/driver/internal-test-driver.py ./test/vendortest/internal/driver.py 
                   COMMAND @cd ./test/vendortest/internal/ && python ./driver.py ../../../escargot internal-test-cases.txt && cd -
                  )

ADD_CUSTOM_TARGET (run-test262
                   COMMENT "run-test262"
                   COMMAND @cp test/excludelist.orig.xml test/test262/test/config/excludelist.xml
                   COMMAND @cp test/test262.py test/test262/tools/packaging/test262.py
                   COMMAND @cd test/test262/ && TZ="US/Pacific" python tools/packaging/test262.py --command ../../escargot $(OPT) --full-summary
                  )

ADD_CUSTOM_TARGET (run-test262-master
                   COMMENT "run-test262-master"
                   COMMAND @cp test/test262-harness-py-excludelist.xml test/test262-harness-py/excludelist.xml
                   COMMAND @cp test/test262-harness-py-test262.py ./test/test262-harness-py/src/test262.py
                   COMMAND @python ./test/test262-harness-py/src/test262.py --command ./escargot --tests=test/test262-master $(OPT) --full-summary
                  )

#TODO
ADD_CUSTOM_TARGET (run-spidermonkey-x86
                   COMMENT "run-spidermonkey-x86"
                   COMMAND @cp /usr/local/lib/node_modules/vendortest/node_modules/ . -rf
                   COMMAND @nodejs node_modules/.bin/babel test/vendortest/SpiderMonkey/ecma_6/Promise --out-dir test/vendortest/SpiderMonkey/ecma_6/Promise
                   COMMAND @rm test/vendortest/SpiderMonkey/shell.js
                   COMMAND @ln -s `pwd`/test/vendortest/driver/spidermonkey.shell.js test/vendortest/SpiderMonkey/shell.js
                   COMMAND @rm test/vendortest/SpiderMonkey/js1_8_1/jit/shell.js
                   COMMAND @ln -s `pwd`/test/vendortest/driver/spidermonkey.js1_8_1.jit.shell.js test/vendortest/SpiderMonkey/js1_8_1/jit/shell.js
                   COMMAND @rm test/vendortest/SpiderMonkey/ecma_6/shell.js
                   COMMAND @ln -s `pwd`/test/vendortest/driver/spidermonkey.ecma_6.shell.js test/vendortest/SpiderMonkey/ecma_6/shell.js
                   COMMAND @rm test/vendortest/SpiderMonkey/ecma_6/TypedArray/shell.js
                   COMMAND @ln -s `pwd`/test/vendortest/driver/spidermonkey.ecma_6.TypedArray.shell.js test/vendortest/SpiderMonkey/ecma_6/TypedArray/shell.js
                   COMMAND @(LOCALE="en_US" ./test/vendortest/SpiderMonkey/jstests.py -s --xul-info=x86-gcc3:Linux:false --exclude-file=./test/vendortest/driver/spidermonkey.excludelist.txt ./escargot --output-file=./test/vendortest/driver/spidermonkey.x86.log.txt --failure-file=../../../test/vendortest/driver/spidermonkey.x86.gen.txt ecma/ ecma_2/ ecma_3/ ecma_3_1/ ecma_5/ ecma_6/Promise ecma_6/TypedArray ecma_6/ArrayBuffer ecma_6/Number ecma_6/Math js1_1/ js1_2/ js1_3/ js1_4/ js1_5/ js1_6/ js1_7/ js1_8/ js1_8_1/ js1_8_5/ shell/ supporting/ ) || (echo done)
                   COMMAND @sort test/vendortest/driver/spidermonkey.x86.orig.txt -o test/vendortest/driver/spidermonkey.x86.orig.sort.txt
                   COMMAND @sort test/vendortest/driver/spidermonkey.x86.gen.txt -o test/vendortest/driver/spidermonkey.x86.gen.sort.txt
                   COMMAND @diff test/vendortest/driver/spidermonkey.x86.orig.sort.txt test/vendortest/driver/spidermonkey.x86.gen.sort.txt
                 )

ADD_CUSTOM_TARGET (run-spidermonkey-x64
                   COMMENT "run-spidermonkey-x64"
                   COMMAND @cp /usr/local/lib/node_modules/vendortest/node_modules/ . -rf
                   COMMAND @nodejs node_modules/.bin/babel test/vendortest/SpiderMonkey/ecma_6/Promise --out-dir test/vendortest/SpiderMonkey/ecma_6/Promise
                   COMMAND @rm test/vendortest/SpiderMonkey/shell.js
                   COMMAND @ln -s `pwd`/test/vendortest/driver/spidermonkey.shell.js test/vendortest/SpiderMonkey/shell.js
                   COMMAND @rm test/vendortest/SpiderMonkey/js1_8_1/jit/shell.js
                   COMMAND @ln -s `pwd`/test/vendortest/driver/spidermonkey.js1_8_1.jit.shell.js test/vendortest/SpiderMonkey/js1_8_1/jit/shell.js
                   COMMAND @rm test/vendortest/SpiderMonkey/ecma_6/shell.js
                   COMMAND @ln -s `pwd`/test/vendortest/driver/spidermonkey.ecma_6.shell.js test/vendortest/SpiderMonkey/ecma_6/shell.js
                   COMMAND @rm test/vendortest/SpiderMonkey/ecma_6/TypedArray/shell.js
                   COMMAND @ln -s `pwd`/test/vendortest/driver/spidermonkey.ecma_6.TypedArray.shell.js test/vendortest/SpiderMonkey/ecma_6/TypedArray/shell.js
                   COMMAND @(LOCALE="en_US" ./test/vendortest/SpiderMonkey/jstests.py -s --xul-info=x86_64-gcc3:Linux:false --exclude-file=./test/vendortest/driver/spidermonkey.excludelist.txt ./escargot --output-file=./test/vendortest/driver/spidermonkey.x86_64.log.txt --failure-file=../../../test/vendortest/driver/spidermonkey.x86_64.gen.txt ecma/ ecma_2/ ecma_3/ ecma_3_1/ ecma_5/ ecma_6/Promise ecma_6/TypedArray ecma_6/ArrayBuffer ecma_6/Number ecma_6/Math js1_1/ js1_2/ js1_3/ js1_4/ js1_5/ js1_6/ js1_7/ js1_8/ js1_8_1/ js1_8_5/ shell/ supporting/ ) || (echo done)
                   COMMAND @sort test/vendortest/driver/spidermonkey.x86_64.orig.txt -o test/vendortest/driver/spidermonkey.x86_64.orig.sort.txt
                   COMMAND @sort test/vendortest/driver/spidermonkey.x86_64.gen.txt -o test/vendortest/driver/spidermonkey.x86_64.gen.sort.txt
                   COMMAND @diff test/vendortest/driver/spidermonkey.x86_64.orig.sort.txt test/vendortest/driver/spidermonkey.x86_64.gen.sort.txt
                 )

ADD_CUSTOM_TARGET (run-jsc-stress-x86
                   COMMENT "run-jsc-stress-x86"
                   COMMAND @PYTHONPATH=. ./test/vendortest/driver/driver.py -s stress -a x86;
                  )

ADD_CUSTOM_TARGET (run-jsc-stress-x64
                   COMMENT "run-jsc-stress-x64"
                   COMMAND @PYTHONPATH=. ./test/vendortest/driver/driver.py -s stress -a x86_64;
                  )


ADD_CUSTOM_TARGET (run-jetstream
                   COMMENT "run-jetstream"
                   COMMAND cp test/vendortest/driver/jetstream/jetstream.CDjsSetup.js test/vendortest/JetStream-1.1/CDjsSetup.js
                   COMMAND cp test/vendortest/driver/jetstream/jetstream.OctaneSetup.js test/vendortest/JetStream-1.1/OctaneSetup.js
                   COMMAND cp test/vendortest/driver/jetstream/jetstream.Octane2Setup.js test/vendortest/JetStream-1.1/Octane2Setup.js
                   COMMAND cp test/vendortest/driver/jetstream/jetstream.SimpleSetup.js test/vendortest/JetStream-1.1/SimpleSetup.js
                   COMMAND cp test/vendortest/driver/jetstream/jetstream.SunSpiderSetup.js test/vendortest/JetStream-1.1/SunSpiderSetup.js
                   COMMAND cp test/vendortest/driver/jetstream/jetstream.cdjs.util.js test/vendortest/JetStream-1.1/cdjs/util.js
                   COMMAND cp test/vendortest/driver/jetstream/jetstream.runOnePlan.js test/vendortest/JetStream-1.1/runOnePlan.js
                   COMMAND cp test/vendortest/driver/jetstream/jetstream.run.sh test/vendortest/JetStream-1.1/run.sh
                   COMMAND cd test/vendortest/JetStream-1.1/; && ./run.sh ../../../escargot $(TARGET_TEST); && cd -;
                   COMMAND python test/vendortest/driver/jetstream/parsingResults.py test/vendortest/driver/jetstream/jetstream-result-raw.res $(TARGET_TEST);
                  )   


ADD_CUSTOM_TARGET (run-jetstream-only-simple
                   COMMENT "run-jetstream-only-simple"
                   COMMAND @make run-jetstream TARGET_TEST="simple"
                   COMMAND @! cat test/vendortest/driver/jetstream/jetstream-result-raw.res | grep -c 'NaN' > /dev/null;
                  )

ADD_CUSTOM_TARGET (run-jetstream-only-cdjs
                   COMMENT "run-jetstream-only-cdjs"
                   COMMAND @make run-jetstream TARGET_TEST="cdjs"
                   COMMAND @! cat test/vendortest/driver/jetstream/jetstream-result-raw.res | grep -c 'NaN' > /dev/null;
                  )

ADD_CUSTOM_TARGET (run-jetstream-only-sunspider
                   COMMENT "run-jetstream-only-sunspider"
                   COMMAND @make run-jetstream TARGET_TEST="sunspider"
                  )

ADD_CUSTOM_TARGET (run-jetstream-only-octane
                   COMMENT "run-jetstream-only-octane"
                   COMMAND @make run-jetstream TARGET_TEST="octane"
                  )

ADD_CUSTOM_TARGET (run-chakracore-x86
                   COMMENT "run-chakracore-x86"
                   COMMAND @cp test/vendortest/driver/chakracore/chakracore.run.sh test/vendortest/ChakraCore/run.sh
                   COMMAND @cp test/vendortest/driver/chakracore/chakracore.include.js test/vendortest/ChakraCore/include.js
                   COMMAND @cp test/vendortest/driver/chakracore/chakracore.rlexedirs.xml test/vendortest/ChakraCore/rlexedirs.xml
                   COMMAND @/bin/bash -c "./build/command_chakracore_x86.sh"
                  )

ADD_CUSTOM_TARGET (run-chakracore-x64
                   COMMENT "run-chakracore-x64"
                   COMMAND @cp test/vendortest/driver/chakracore/chakracore.run.sh test/vendortest/ChakraCore/run.sh
                   COMMAND @cp test/vendortest/driver/chakracore/chakracore.include.js test/vendortest/ChakraCore/include.js
                   COMMAND @cp test/vendortest/driver/chakracore/chakracore.rlexedirs.xml test/vendortest/ChakraCore/rlexedirs.xml
                   COMMAND @/bin/bash -c "./build/command_chakracore_x86_64.sh"
                  )

ADD_CUSTOM_TARGET (run-v8-x86
                   COMMENT "run-v8-x86"
                   COMMAND @cp test/vendortest/driver/v8/v8.mjsunit.status test/vendortest/v8/test/mjsunit/mjsunit.status
                   COMMAND @cp test/vendortest/driver/v8/v8.mjsunit.js test/vendortest/v8/test/mjsunit/mjsunit.js
                   COMMAND @cp test/vendortest/driver/v8/v8.run-tests.py test/vendortest/v8/tools/run-tests.py
                   COMMAND @cp test/vendortest/driver/v8/v8.testsuite.py test/vendortest/v8/tools/testrunner/local/testsuite.py
                   COMMAND @cp test/vendortest/driver/v8/v8.execution.py test/vendortest/v8/tools/testrunner/local/execution.py
                   COMMAND @cp test/vendortest/driver/v8/v8.progress.py test/vendortest/v8/tools/testrunner/local/progress.py
	               COMMAND @./test/vendortest/v8/tools/run-tests.py --quickcheck --no-presubmit --no-variants --arch-and-mode=x32.release --shell-dir ../../../ --escargot --report -p verbose --no-sorting mjsunit | tee test/vendortest/driver/v8.x32.mjsunit.gen.txt && diff test/vendortest/driver/v8.x32.mjsunit.orig.txt test/vendortest/driver/v8.x32.mjsunit.gen.txt
                  )

ADD_CUSTOM_TARGET (run-v8-x64
                   COMMENT "run-v8-x64"
                   COMMAND @cp test/vendortest/driver/v8/v8.mjsunit.status test/vendortest/v8/test/mjsunit/mjsunit.status
                   COMMAND @cp test/vendortest/driver/v8/v8.mjsunit.js test/vendortest/v8/test/mjsunit/mjsunit.js
                   COMMAND @cp test/vendortest/driver/v8/v8.run-tests.py test/vendortest/v8/tools/run-tests.py
                   COMMAND @cp test/vendortest/driver/v8/v8.testsuite.py test/vendortest/v8/tools/testrunner/local/testsuite.py
                   COMMAND @cp test/vendortest/driver/v8/v8.execution.py test/vendortest/v8/tools/testrunner/local/execution.py
                   COMMAND @cp test/vendortest/driver/v8/v8.progress.py test/vendortest/v8/tools/testrunner/local/progress.py
	               COMMAND @./test/vendortest/v8/tools/run-tests.py --quickcheck --no-presubmit --no-variants --arch-and-mode=x64.release --shell-dir ../../../ --escargot --report -p verbose --no-sorting mjsunit | tee test/vendortest/driver/v8.x64.mjsunit.gen.txt && diff test/vendortest/driver/v8.x64.mjsunit.orig.txt test/vendortest/driver/v8.x64.mjsunit.gen.txt
                  )

