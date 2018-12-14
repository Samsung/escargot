CMAKE_MINIMUM_REQUIRED (VERSION 2.8)


# Targets : benchmarks

ADD_CUSTOM_TARGET (run-sunspider
                   COMMENT "run-sunspider"
                   COMMAND @cd ${PROJECT_SOURCE_DIR}/test/vendortest/SunSpider/ && ./sunspider --shell=${PROJECT_SOURCE_DIR}/escargot --suite=sunspider-1.0.2
                  )


ADD_CUSTOM_TARGET (run-sunspider-js
                   COMMENT "run-sunspider-js"
                   COMMAND @${PROJECT_SOURCE_DIR}/escargot ${PROJECT_SOURCE_DIR}/test/vendortest/SunSpider/tests/sunspider-1.0.2/*.js
                  )


ADD_CUSTOM_TARGET (run-octane
                   COMMENT "run-octane"
                   COMMAND @cd ${PROJECT_SOURCE_DIR}/test/octane/ && ${PROJECT_SOURCE_DIR}/escargot run.js | tee > ${PROJECT_SOURCE_DIR}/test/octane_result
                   COMMAND @cat ${PROJECT_SOURCE_DIR}/test/octane_result | grep -c 'Score' > /dev/null;
                  )


# Targets : Regression test

ADD_CUSTOM_TARGET (run-internal-test
                   COMMENT "run-internal-test"
                   COMMAND @cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/internal-test-cases.txt ${PROJECT_SOURCE_DIR}/test/vendortest/internal/internal-test-cases.txt
                   COMMAND @cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/internal-test-driver.py ${PROJECT_SOURCE_DIR}/test/vendortest/internal/driver.py
                   COMMAND @cd ${PROJECT_SOURCE_DIR}/test/vendortest/internal/ && python ./driver.py ${PROJECT_SOURCE_DIR}/escargot internal-test-cases.txt && cd -
                  )

ADD_CUSTOM_TARGET (run-test262
                   COMMENT "run-test262"
                   COMMAND @cp ${PROJECT_SOURCE_DIR}/test/excludelist.orig.xml ${PROJECT_SOURCE_DIR}/test/test262/test/config/excludelist.xml
                   COMMAND @cp ${PROJECT_SOURCE_DIR}/test/test262.py ${PROJECT_SOURCE_DIR}/test/test262/tools/packaging/test262.py
                   COMMAND @cd ${PROJECT_SOURCE_DIR}/test/test262/ && TZ="US/Pacific" python ./tools/packaging/test262.py --command ${PROJECT_SOURCE_DIR}/escargot $(OPT) --full-summary
                  )

ADD_CUSTOM_TARGET (run-test262-master
                   COMMENT "run-test262-master"
                   COMMAND @cp ${PROJECT_SOURCE_DIR}/test/test262-harness-py-excludelist.xml ${PROJECT_SOURCE_DIR}/test/test262-harness-py/excludelist.xml
                   COMMAND @cp ${PROJECT_SOURCE_DIR}/test/test262-harness-py-test262.py ${PROJECT_SOURCE_DIR}/test/test262-harness-py/src/test262.py
                   COMMAND @python ${PROJECT_SOURCE_DIR}/test/test262-harness-py/src/test262.py --command ${PROJECT_SOURCE_DIR}/escargot --tests=${PROJECT_SOURCE_DIR}/test/test262-master $(OPT) --full-summary
                  )

ADD_CUSTOM_TARGET (run-spidermonkey-x86
                   COMMENT "run-spidermonkey-x86"
                   COMMAND @nodejs ${PROJECT_SOURCE_DIR}/node_modules/.bin/babel ${PROJECT_SOURCE_DIR}/test/vendortest/SpiderMonkey/ecma_6/Promise --out-dir ${PROJECT_SOURCE_DIR}/test/vendortest/SpiderMonkey/ecma_6/Promise
                   COMMAND @rm ${PROJECT_SOURCE_DIR}/test/vendortest/SpiderMonkey/shell.js
                   COMMAND @ln -s ${PROJECT_SOURCE_DIR}/test/vendortest/driver/spidermonkey.shell.js ${PROJECT_SOURCE_DIR}/test/vendortest/SpiderMonkey/shell.js
                   COMMAND @rm ${PROJECT_SOURCE_DIR}/test/vendortest/SpiderMonkey/js1_8_1/jit/shell.js
                   COMMAND @ln -s ${PROJECT_SOURCE_DIR}/test/vendortest/driver/spidermonkey.js1_8_1.jit.shell.js ${PROJECT_SOURCE_DIR}/test/vendortest/SpiderMonkey/js1_8_1/jit/shell.js
                   COMMAND @rm ${PROJECT_SOURCE_DIR}/test/vendortest/SpiderMonkey/ecma_6/shell.js
                   COMMAND @ln -s ${PROJECT_SOURCE_DIR}/test/vendortest/driver/spidermonkey.ecma_6.shell.js ${PROJECT_SOURCE_DIR}/test/vendortest/SpiderMonkey/ecma_6/shell.js
                   COMMAND @rm ${PROJECT_SOURCE_DIR}/test/vendortest/SpiderMonkey/ecma_6/TypedArray/shell.js
                   COMMAND @ln -s ${PROJECT_SOURCE_DIR}/test/vendortest/driver/spidermonkey.ecma_6.TypedArray.shell.js ${PROJECT_SOURCE_DIR}/test/vendortest/SpiderMonkey/ecma_6/TypedArray/shell.js
                   COMMAND @(LOCALE="en_US" ${PROJECT_SOURCE_DIR}/test/vendortest/SpiderMonkey/jstests.py --no-progress -s --xul-info=x86-gcc3:Linux:false --exclude-file=${PROJECT_SOURCE_DIR}/test/vendortest/driver/spidermonkey.excludelist.txt ${PROJECT_SOURCE_DIR}/escargot --output-file=${PROJECT_SOURCE_DIR}/test/vendortest/driver/spidermonkey.x86.log.txt --failure-file=${PROJECT_SOURCE_DIR}/test/vendortest/driver/spidermonkey.x86.gen.txt ecma/ ecma_2/ ecma_3/ ecma_3_1/ ecma_5/ ecma_6/Promise ecma_6/TypedArray ecma_6/ArrayBuffer ecma_6/Number ecma_6/Math js1_1/ js1_2/ js1_3/ js1_4/ js1_5/ js1_6/ js1_7/ js1_8/ js1_8_1/ js1_8_5/ shell/ supporting/ ) || (echo done)
                   COMMAND @sort ${PROJECT_SOURCE_DIR}/test/vendortest/driver/spidermonkey.x86.orig.txt -o ${PROJECT_SOURCE_DIR}/test/vendortest/driver/spidermonkey.x86.orig.sort.txt
                   COMMAND @sort ${PROJECT_SOURCE_DIR}/test/vendortest/driver/spidermonkey.x86.gen.txt -o ${PROJECT_SOURCE_DIR}/test/vendortest/driver/spidermonkey.x86.gen.sort.txt
                   COMMAND @diff ${PROJECT_SOURCE_DIR}/test/vendortest/driver/spidermonkey.x86.orig.sort.txt ${PROJECT_SOURCE_DIR}/test/vendortest/driver/spidermonkey.x86.gen.sort.txt
                 )

ADD_CUSTOM_TARGET (run-spidermonkey-x64
                   COMMENT "run-spidermonkey-x64"
                   COMMAND @nodejs ${PROJECT_SOURCE_DIR}/node_modules/.bin/babel ${PROJECT_SOURCE_DIR}/test/vendortest/SpiderMonkey/ecma_6/Promise --out-dir ${PROJECT_SOURCE_DIR}/test/vendortest/SpiderMonkey/ecma_6/Promise
                   COMMAND @rm ${PROJECT_SOURCE_DIR}/test/vendortest/SpiderMonkey/shell.js
                   COMMAND @ln -s ${PROJECT_SOURCE_DIR}/test/vendortest/driver/spidermonkey.shell.js ${PROJECT_SOURCE_DIR}/test/vendortest/SpiderMonkey/shell.js
                   COMMAND @rm ${PROJECT_SOURCE_DIR}/test/vendortest/SpiderMonkey/js1_8_1/jit/shell.js
                   COMMAND @ln -s ${PROJECT_SOURCE_DIR}/test/vendortest/driver/spidermonkey.js1_8_1.jit.shell.js ${PROJECT_SOURCE_DIR}/test/vendortest/SpiderMonkey/js1_8_1/jit/shell.js
                   COMMAND @rm ${PROJECT_SOURCE_DIR}/test/vendortest/SpiderMonkey/ecma_6/shell.js
                   COMMAND @ln -s ${PROJECT_SOURCE_DIR}/test/vendortest/driver/spidermonkey.ecma_6.shell.js ${PROJECT_SOURCE_DIR}/test/vendortest/SpiderMonkey/ecma_6/shell.js
                   COMMAND @rm ${PROJECT_SOURCE_DIR}/test/vendortest/SpiderMonkey/ecma_6/TypedArray/shell.js
                   COMMAND @ln -s ${PROJECT_SOURCE_DIR}/test/vendortest/driver/spidermonkey.ecma_6.TypedArray.shell.js ${PROJECT_SOURCE_DIR}/test/vendortest/SpiderMonkey/ecma_6/TypedArray/shell.js
                   COMMAND @(LOCALE="en_US" ${PROJECT_SOURCE_DIR}/test/vendortest/SpiderMonkey/jstests.py --no-progress -s --xul-info=x86_64-gcc3:Linux:false --exclude-file=${PROJECT_SOURCE_DIR}/test/vendortest/driver/spidermonkey.excludelist.txt ${PROJECT_SOURCE_DIR}/escargot --output-file=${PROJECT_SOURCE_DIR}/test/vendortest/driver/spidermonkey.x86_64.log.txt --failure-file=${PROJECT_SOURCE_DIR}/test/vendortest/driver/spidermonkey.x86_64.gen.txt ecma/ ecma_2/ ecma_3/ ecma_3_1/ ecma_5/ ecma_6/Promise ecma_6/TypedArray ecma_6/ArrayBuffer ecma_6/Number ecma_6/Math js1_1/ js1_2/ js1_3/ js1_4/ js1_5/ js1_6/ js1_7/ js1_8/ js1_8_1/ js1_8_5/ shell/ supporting/ ) || (echo done)
                   COMMAND @sort ${PROJECT_SOURCE_DIR}/test/vendortest/driver/spidermonkey.x86_64.orig.txt -o ${PROJECT_SOURCE_DIR}/test/vendortest/driver/spidermonkey.x86_64.orig.sort.txt
                   COMMAND @sort ${PROJECT_SOURCE_DIR}/test/vendortest/driver/spidermonkey.x86_64.gen.txt -o ${PROJECT_SOURCE_DIR}/test/vendortest/driver/spidermonkey.x86_64.gen.sort.txt
                   COMMAND @diff ${PROJECT_SOURCE_DIR}/test/vendortest/driver/spidermonkey.x86_64.orig.sort.txt ${PROJECT_SOURCE_DIR}/test/vendortest/driver/spidermonkey.x86_64.gen.sort.txt
                 )

ADD_CUSTOM_TARGET (run-jsc-stress-x86
                   COMMENT "run-jsc-stress-x86"
                   COMMAND @cd ${PROJECT_SOURCE_DIR} && PYTHONPATH=. ./test/vendortest/driver/driver.py -s stress -a x86;
                  )

ADD_CUSTOM_TARGET (run-jsc-stress-x64
                   COMMENT "run-jsc-stress-x64"
                   COMMAND @cd ${PROJECT_SOURCE_DIR} && PYTHONPATH=. ./test/vendortest/driver/driver.py -s stress -a x86_64;
                  )


ADD_CUSTOM_TARGET (run-jetstream
                   COMMENT "run-jetstream"
                   COMMAND cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/jetstream/jetstream.CDjsSetup.js ${PROJECT_SOURCE_DIR}/test/vendortest/JetStream-1.1/CDjsSetup.js
                   COMMAND cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/jetstream/jetstream.OctaneSetup.js ${PROJECT_SOURCE_DIR}/test/vendortest/JetStream-1.1/OctaneSetup.js
                   COMMAND cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/jetstream/jetstream.Octane2Setup.js ${PROJECT_SOURCE_DIR}/test/vendortest/JetStream-1.1/Octane2Setup.js
                   COMMAND cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/jetstream/jetstream.SimpleSetup.js ${PROJECT_SOURCE_DIR}/test/vendortest/JetStream-1.1/SimpleSetup.js
                   COMMAND cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/jetstream/jetstream.SunSpiderSetup.js ${PROJECT_SOURCE_DIR}/test/vendortest/JetStream-1.1/SunSpiderSetup.js
                   COMMAND cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/jetstream/jetstream.cdjs.util.js ${PROJECT_SOURCE_DIR}/test/vendortest/JetStream-1.1/cdjs/util.js
                   COMMAND cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/jetstream/jetstream.runOnePlan.js ${PROJECT_SOURCE_DIR}/test/vendortest/JetStream-1.1/runOnePlan.js
                   COMMAND cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/jetstream/jetstream.run.sh ${PROJECT_SOURCE_DIR}/test/vendortest/JetStream-1.1/run.sh
                   COMMAND cd ${PROJECT_SOURCE_DIR}/test/vendortest/JetStream-1.1/; && ./run.sh ${PROJECT_SOURCE_DIR}/escargot $(TARGET_TEST); && cd -;
                   COMMAND python ${PROJECT_SOURCE_DIR}/test/vendortest/driver/jetstream/parsingResults.py ${PROJECT_SOURCE_DIR}/test/vendortest/driver/jetstream/jetstream-result-raw.res $(TARGET_TEST);
                  )


ADD_CUSTOM_TARGET (run-jetstream-only-simple
                   COMMENT "run-jetstream-only-simple"
                   COMMAND @make run-jetstream TARGET_TEST="simple"
                   COMMAND @! cat ${PROJECT_SOURCE_DIR}/test/vendortest/driver/jetstream/jetstream-result-raw.res | grep -c 'NaN' > /dev/null;
                  )

ADD_CUSTOM_TARGET (run-jetstream-only-cdjs
                   COMMENT "run-jetstream-only-cdjs"
                   COMMAND @make run-jetstream TARGET_TEST="cdjs"
                   COMMAND @! cat ${PROJECT_SOURCE_DIR}/test/vendortest/driver/jetstream/jetstream-result-raw.res | grep -c 'NaN' > /dev/null;
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
                   COMMAND @cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/chakracore/chakracore.run.sh ${PROJECT_SOURCE_DIR}/test/vendortest/ChakraCore/run.sh
                   COMMAND @cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/chakracore/chakracore.include.js ${PROJECT_SOURCE_DIR}/test/vendortest/ChakraCore/include.js
                   COMMAND @cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/chakracore/chakracore.rlexedirs.xml ${PROJECT_SOURCE_DIR}/test/vendortest/ChakraCore/rlexedirs.xml
                   COMMAND @cd ${PROJECT_SOURCE_DIR} && /bin/bash -c "./build/command_chakracore_x86.sh"
                  )

ADD_CUSTOM_TARGET (run-chakracore-x64
                   COMMENT "run-chakracore-x64"
                   COMMAND @cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/chakracore/chakracore.run.sh ${PROJECT_SOURCE_DIR}/test/vendortest/ChakraCore/run.sh
                   COMMAND @cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/chakracore/chakracore.include.js ${PROJECT_SOURCE_DIR}/test/vendortest/ChakraCore/include.js
                   COMMAND @cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/chakracore/chakracore.rlexedirs.xml ${PROJECT_SOURCE_DIR}/test/vendortest/ChakraCore/rlexedirs.xml
                   COMMAND @cd ${PROJECT_SOURCE_DIR} && /bin/bash -c "./build/command_chakracore_x86_64.sh"
                  )

ADD_CUSTOM_TARGET (run-v8-x86
                   COMMENT "run-v8-x86"
                   COMMAND @cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/v8/v8.mjsunit.status ${PROJECT_SOURCE_DIR}/test/vendortest/v8/test/mjsunit/mjsunit.status
                   COMMAND @cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/v8/v8.mjsunit.js ${PROJECT_SOURCE_DIR}/test/vendortest/v8/test/mjsunit/mjsunit.js
                   COMMAND @cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/v8/v8.run-tests.py ${PROJECT_SOURCE_DIR}/test/vendortest/v8/tools/run-tests.py
                   COMMAND @cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/v8/v8.testsuite.py ${PROJECT_SOURCE_DIR}/test/vendortest/v8/tools/testrunner/local/testsuite.py
                   COMMAND @cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/v8/v8.execution.py ${PROJECT_SOURCE_DIR}/test/vendortest/v8/tools/testrunner/local/execution.py
                   COMMAND @cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/v8/v8.progress.py ${PROJECT_SOURCE_DIR}/test/vendortest/v8/tools/testrunner/local/progress.py
                   COMMAND @${PROJECT_SOURCE_DIR}/test/vendortest/v8/tools/run-tests.py --quickcheck --no-presubmit --no-variants --arch-and-mode=x32.release --shell-dir ../../../ --escargot --report -p verbose --no-sorting mjsunit | tee ${PROJECT_SOURCE_DIR}/test/vendortest/driver/v8.x32.mjsunit.gen.txt && diff ${PROJECT_SOURCE_DIR}/test/vendortest/driver/v8.x32.mjsunit.orig.txt ${PROJECT_SOURCE_DIR}/test/vendortest/driver/v8.x32.mjsunit.gen.txt
                  )

ADD_CUSTOM_TARGET (run-v8-x64
                   COMMENT "run-v8-x64"
                   COMMAND @cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/v8/v8.mjsunit.status ${PROJECT_SOURCE_DIR}/test/vendortest/v8/test/mjsunit/mjsunit.status
                   COMMAND @cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/v8/v8.mjsunit.js ${PROJECT_SOURCE_DIR}/test/vendortest/v8/test/mjsunit/mjsunit.js
                   COMMAND @cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/v8/v8.run-tests.py ${PROJECT_SOURCE_DIR}/test/vendortest/v8/tools/run-tests.py
                   COMMAND @cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/v8/v8.testsuite.py ${PROJECT_SOURCE_DIR}/test/vendortest/v8/tools/testrunner/local/testsuite.py
                   COMMAND @cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/v8/v8.execution.py ${PROJECT_SOURCE_DIR}/test/vendortest/v8/tools/testrunner/local/execution.py
                   COMMAND @cp ${PROJECT_SOURCE_DIR}/test/vendortest/driver/v8/v8.progress.py ${PROJECT_SOURCE_DIR}/test/vendortest/v8/tools/testrunner/local/progress.py
                   COMMAND @${PROJECT_SOURCE_DIR}/test/vendortest/v8/tools/run-tests.py --quickcheck --no-presubmit --no-variants --arch-and-mode=x64.release --shell-dir ../../../ --escargot --report -p verbose --no-sorting mjsunit | tee ${PROJECT_SOURCE_DIR}/test/vendortest/driver/v8.x64.mjsunit.gen.txt && diff ${PROJECT_SOURCE_DIR}/test/vendortest/driver/v8.x64.mjsunit.orig.txt ${PROJECT_SOURCE_DIR}/test/vendortest/driver/v8.x64.mjsunit.gen.txt
                  )
