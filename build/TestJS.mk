
# TODO this list should be maintained in a better way
OPT_MASTER_PROMISE=built-ins/Promise
OPT_MASTER_ARRAYBUFFER=#built-ins/ArrayBuffer
OPT_MASTER_DATAVIEW=#built-ins/DataView
OPT_MASTER_TYPEDARRAY=built-ins/TypedArray/prototype/set built-ins/TypedArray/prototype/indexOf built-ins/TypedArray/prototype/lastIndexOf #built-ins/TypedArray built-ins/TypedArrays

check:
	make tidy-update
	make x86.interpreter.release -j$(NPROCS)
	# make run-sunspider | tee out/sunspider_result
	make run-test262
	# make run-test262-master OPT="$(OPT_MASTER_PROMISE) $(OPT_MASTER_ARRAYBUFFER) $(OPT_MASTER_DATAVIEW) $(OPT_MASTER_TYPEDARRAY)"
	make run-spidermonkey-full
	make run-internal-test

tidy-install:
	apt-get install clang-format-3.8

tidy:
	python tools/check_tidy.py

tidy-update:
	python tools/check_tidy.py update

# Targets : benchmarks

run-sunspider:
	cd test/vendortest/SunSpider/; \
	./sunspider --shell=../../../escargot --suite=sunspider-1.0.2

run-sunspider-js:
	./escargot test/vendortest/SunSpider/tests/sunspider-1.0.2/*.js


run-octane:
	cd test/octane/; \
	../../escargot run.js | tail -1 > ../../out/octane_result; \
	if ! cat ../../out/octane_result | grep -c 'Score' > /dev/null; then exit 1; fi

# Targets : Regression test

run-internal-test:
	cp ./test/vendortest/driver/internal-test-cases.txt ./test/vendortest/internal/internal-test-cases.txt ; \
	cp ./test/vendortest/driver/internal-test-driver.py ./test/vendortest/internal/driver.py ; \
	cd ./test/vendortest/internal/ ; \
	python ./driver.py ../../../escargot internal-test-cases.txt \
	cd -

run-test262:
	cp test/excludelist.orig.xml test/test262/test/config/excludelist.xml
	cp test/test262.py test/test262/tools/packaging/test262.py
	cd test/test262/; \
	TZ="US/Pacific" python tools/packaging/test262.py --command ../../escargot $(OPT) --full-summary

run-test262-master:
	cp test/test262-harness-py-excludelist.xml test/test262-harness-py/excludelist.xml
	cp test/test262-harness-py-test262.py ./test/test262-harness-py/src/test262.py
	python ./test/test262-harness-py/src/test262.py --command ./escargot --tests=test/test262-master $(OPT) --full-summary

run-spidermonkey:
	cp /usr/local/lib/node_modules/vendortest/node_modules/ . -rf
	nodejs node_modules/.bin/babel test/vendortest/SpiderMonkey/ecma_6/Promise --out-dir test/vendortest/SpiderMonkey/ecma_6/Promise
	rm test/vendortest/SpiderMonkey/shell.js
	ln -s `pwd`/test/vendortest/driver/spidermonkey.shell.js test/vendortest/SpiderMonkey/shell.js
	rm test/vendortest/SpiderMonkey/js1_8_1/jit/shell.js
	ln -s `pwd`/test/vendortest/driver/spidermonkey.js1_8_1.jit.shell.js test/vendortest/SpiderMonkey/js1_8_1/jit/shell.js
	rm test/vendortest/SpiderMonkey/ecma_6/shell.js
	ln -s `pwd`/test/vendortest/driver/spidermonkey.ecma_6.shell.js test/vendortest/SpiderMonkey/ecma_6/shell.js
	rm test/vendortest/SpiderMonkey/ecma_6/TypedArray/shell.js
	ln -s `pwd`/test/vendortest/driver/spidermonkey.ecma_6.TypedArray.shell.js test/vendortest/SpiderMonkey/ecma_6/TypedArray/shell.js
	rm test/vendortest/SpiderMonkey/ecma_6/Math/shell.js
	ln -s `pwd`/test/vendortest/driver/spidermonkey.ecma_6.Math.shell.js test/vendortest/SpiderMonkey/ecma_6/Math/shell.js
	$(eval BIN_ARCH:=$(shell [[ "$(shell file escargot)" == *"32-bit"* ]] && echo "x86" || echo "x86_64"))
	(LOCALE="en_US" ./test/vendortest/SpiderMonkey/jstests.py -s --xul-info=$(BIN_ARCH)-gcc3:Linux:false \
        --exclude-file=./test/vendortest/driver/spidermonkey.excludelist.txt ./escargot \
		--output-file=./test/vendortest/driver/spidermonkey.$(BIN_ARCH).log.txt \
		--failure-file=../../../test/vendortest/driver/spidermonkey.$(BIN_ARCH).gen.txt \
		ecma/ ecma_2/ ecma_3/ ecma_3_1/ ecma_5/ ecma_6/Promise ecma_6/TypedArray ecma_6/ArrayBuffer ecma_6/Number ecma_6/Math \
		js1_1/ js1_2/ js1_3/ js1_4/ js1_5/ js1_6/ js1_7/ js1_8/ js1_8_1/ js1_8_5/ shell/ supporting/ ) || (echo done)
	sort test/vendortest/driver/spidermonkey.$(BIN_ARCH).orig.txt -o test/vendortest/driver/spidermonkey.$(BIN_ARCH).orig.sort.txt
	sort test/vendortest/driver/spidermonkey.$(BIN_ARCH).gen.txt -o test/vendortest/driver/spidermonkey.$(BIN_ARCH).gen.sort.txt
	diff test/vendortest/driver/spidermonkey.$(BIN_ARCH).orig.sort.txt test/vendortest/driver/spidermonkey.$(BIN_ARCH).gen.sort.txt

run-jsc-stress:
	$(eval BIN_ARCH:=$(shell [[ "$(shell file escargot)" == *"32-bit"* ]] && echo "x86" || echo "x86_64"))
	PYTHONPATH=. ./test/vendortest/driver/driver.py -s stress -a $(BIN_ARCH);

#run-jsc-mozilla:
#	cd test/JavaScriptCore/mozilla/; \
		perl jsDriver.pl -e escargot -s ../../../escargot

run-jetstream:
	cp test/vendortest/driver/jetstream/jetstream.CDjsSetup.js test/vendortest/JetStream-1.1/CDjsSetup.js
	cp test/vendortest/driver/jetstream/jetstream.OctaneSetup.js test/vendortest/JetStream-1.1/OctaneSetup.js
	cp test/vendortest/driver/jetstream/jetstream.Octane2Setup.js test/vendortest/JetStream-1.1/Octane2Setup.js
	cp test/vendortest/driver/jetstream/jetstream.SimpleSetup.js test/vendortest/JetStream-1.1/SimpleSetup.js
	cp test/vendortest/driver/jetstream/jetstream.SunSpiderSetup.js test/vendortest/JetStream-1.1/SunSpiderSetup.js
	cp test/vendortest/driver/jetstream/jetstream.cdjs.util.js test/vendortest/JetStream-1.1/cdjs/util.js
	cp test/vendortest/driver/jetstream/jetstream.runOnePlan.js test/vendortest/JetStream-1.1/runOnePlan.js
	cp test/vendortest/driver/jetstream/jetstream.run.sh test/vendortest/JetStream-1.1/run.sh
	cd test/vendortest/JetStream-1.1/; \
	./run.sh ../../../escargot $(TARGET_TEST); \
	cd -; \
	python test/vendortest/driver/jetstream/parsingResults.py test/vendortest/driver/jetstream/jetstream-result-raw.res $(TARGET_TEST);

run-jetstream-only-simple:
	make run-jetstream TARGET_TEST="simple"
	if cat test/vendortest/driver/jetstream/jetstream-result-raw.res | grep -c 'NaN' > /dev/null; then exit 1; fi

run-jetstream-only-cdjs:
	make run-jetstream TARGET_TEST="cdjs"
	if cat test/vendortest/driver/jetstream/jetstream-result-raw.res | grep -c 'NaN' > /dev/null; then exit 1; fi

run-jetstream-only-sunspider:
	make run-jetstream TARGET_TEST="sunspider"

run-jetstream-only-octane:
	make run-jetstream TARGET_TEST="octane"

run-chakracore:
	cp test/vendortest/driver/chakracore/chakracore.run.sh test/vendortest/ChakraCore/run.sh
	cp test/vendortest/driver/chakracore/chakracore.include.js test/vendortest/ChakraCore/include.js
	cp test/vendortest/driver/chakracore/chakracore.rlexedirs.xml test/vendortest/ChakraCore/rlexedirs.xml
	for i in test/vendortest/driver/chakracore/*.rlexe.xml; do dir=`echo $$i | cut -d'/' -f5 | cut -d'.' -f1`; \
		echo "cp test/vendortest/driver/chakracore/$$dir.rlexe.xml test/vendortest/ChakraCore/$$dir/rlexe.xml"; \
		cp test/vendortest/driver/chakracore/$$dir.rlexe.xml test/vendortest/ChakraCore/$$dir/rlexe.xml; done
	echo "WScript.Echo('PASS');" >> test/vendortest/ChakraCore/DynamicCode/eval-nativecodedata.js
	echo "WScript.Echo('PASS');" >> test/vendortest/ChakraCore/utf8/unicode_digit_as_identifier_should_work.js
	$(eval BIN_ARCH:=$(shell [[ "$(shell file escargot)" == *"32-bit"* ]] && echo "x86" || echo "x86_64"))
	test/vendortest/ChakraCore/run.sh ./escargot | tee test/vendortest/driver/chakracore.$(BIN_ARCH).gen.txt; \
	diff test/vendortest/driver/chakracore.$(BIN_ARCH).orig.txt test/vendortest/driver/chakracore.$(BIN_ARCH).gen.txt

run-v8:
	cp test/vendortest/driver/v8/v8.mjsunit.status test/vendortest/v8/test/mjsunit/mjsunit.status
	cp test/vendortest/driver/v8/v8.mjsunit.js test/vendortest/v8/test/mjsunit/mjsunit.js
	cp test/vendortest/driver/v8/v8.run-tests.py test/vendortest/v8/tools/run-tests.py
	cp test/vendortest/driver/v8/v8.testsuite.py test/vendortest/v8/tools/testrunner/local/testsuite.py
	cp test/vendortest/driver/v8/v8.execution.py test/vendortest/v8/tools/testrunner/local/execution.py
	cp test/vendortest/driver/v8/v8.progress.py test/vendortest/v8/tools/testrunner/local/progress.py
	$(eval ARCH:=$(shell [[ "$(shell file escargot)" == *"32-bit"* ]] && echo "x32" || echo "x64"))
	./test/vendortest/v8/tools/run-tests.py --quickcheck --no-presubmit --no-variants --arch-and-mode=$(ARCH).release --shell-dir ../../../ --escargot --report -p verbose --no-sorting mjsunit | tee test/vendortest/driver/v8.$(ARCH).mjsunit.gen.txt; \
	diff test/vendortest/driver/v8.$(ARCH).mjsunit.orig.txt test/vendortest/driver/v8.$(ARCH).mjsunit.gen.txt
