
# TODO this list should be maintained in a better way
OPT_MASTER_PROMISE=built-ins/Promise
OPT_MASTER_ARRAYBUFFER=#built-ins/ArrayBuffer
OPT_MASTER_DATAVIEW=#built-ins/DataView
OPT_MASTER_TYPEDARRAY=built-ins/TypedArray/prototype/set #built-ins/TypedArray built-ins/TypedArrays

check:
	make tidy-update
	make x64.interpreter.release -j$(NPROCS)
	# make run-sunspider | tee out/sunspider_result
	make run-test262
	make run-test262-master OPT="$(OPT_MASTER_PROMISE) $(OPT_MASTER_ARRAYBUFFER) $(OPT_MASTER_DATAVIEW) $(OPT_MASTER_TYPEDARRAY)"

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

run-octane:
	cd test/octane/; \
	../../escargot run.js

# Targets : Regression test

run-test262:
	cp excludelist.orig.xml test/test262/test/config/excludelist.xml
	cp test262.py test/test262/tools/packaging/test262.py
	cd test/test262/; \
	TZ="US/Pacific" python tools/packaging/test262.py --command ../../escargot $(OPT) --full-summary

run-test262-master:
	cp test/test262-harness-py-excludelist.xml test/test262-harness-py/excludelist.xml
	cp test/test262-harness-py-test262.py ./test/test262-harness-py/src/test262.py
	python ./test/test262-harness-py/src/test262.py --command ./escargot --tests=test/test262-master $(OPT) --full-summary

run-spidermonkey:
	rm test/vendortest/SpiderMonkey/shell.js
	ln -s `pwd`/tools/vendortest/spidermonkey.shell.js test/vendortest/SpiderMonkey/shell.js
	rm test/vendortest/SpiderMonkey/js1_8_1/jit/shell.js
	ln -s `pwd`/tools/vendortest/spidermonkey.js1_8_1.jit.shell.js test/vendortest/SpiderMonkey/js1_8_1/jit/shell.js
	rm test/vendortest/SpiderMonkey/ecma_6/shell.js
	ln -s `pwd`/tools/vendortest/spidermonkey.ecma_6.shell.js test/vendortest/SpiderMonkey/ecma_6/shell.js
	$(eval BIN_ARCH:=$(shell [[ "$(shell file escargot)" == *"32-bit"* ]] && echo "x86" || echo "x86_64"))
	(./test/vendortest/SpiderMonkey/jstests.py -s --xul-info=$(BIN_ARCH)-gcc3:Linux:false \
        --exclude-file=./tools/vendortest/spidermonkey.excludelist.txt ./escargot \
		--output-file=./tools/vendortest/spidermonkey.$(BIN_ARCH).log.txt \
		--failure-file=../../../tools/vendortest/spidermonkey.$(BIN_ARCH).gen.txt $(OPT) ) || (echo done)

run-spidermonkey-full:
	npm install
	nodejs node_modules/.bin/babel test/vendortest/SpiderMonkey/ecma_6/Promise --out-dir test/vendortest/SpiderMonkey/ecma_6/Promise
	make run-spidermonkey \
		OPT="ecma/ ecma_2/ ecma_3/ ecma_3_1/ ecma_5/ ecma_6/Promise ecma_6/TypedArray ecma_6/ArrayBuffer \
			js1_1/ js1_2/ js1_3/ js1_4/ js1_5/ js1_6/ js1_7/ js1_8/ js1_8_1/ js1_8_5/ shell/ supporting/"
	$(eval BIN_ARCH:=$(shell [[ "$(shell file escargot)" == *"32-bit"* ]] && echo "x86" || echo "x86_64"))
	sort tools/vendortest/spidermonkey.$(BIN_ARCH).orig.txt -o tools/vendortest/spidermonkey.$(BIN_ARCH).orig.txt
	sort tools/vendortest/spidermonkey.$(BIN_ARCH).gen.txt -o tools/vendortest/spidermonkey.$(BIN_ARCH).gen.txt
	diff tools/vendortest/spidermonkey.$(BIN_ARCH).orig.txt tools/vendortest/spidermonkey.$(BIN_ARCH).gen.txt

run-jsc-stress:
	cp tools/vendortest/jsc.stress.resource.typedarray-constructor-helper-functions.js test/vendortest/JavaScriptCore/stress/resources/typedarray-constructor-helper-functions.js
	$(eval BIN_ARCH:=$(shell [[ "$(shell file escargot)" == *"32-bit"* ]] && echo "x86" || echo "x86_64"))
	PYTHONPATH=. ./tools/vendortest/driver.py -s stress -a $(BIN_ARCH);

#run-jsc-mozilla:
#	cd test/JavaScriptCore/mozilla/; \
		perl jsDriver.pl -e escargot -s ../../../escargot

run-jetstream:
	cp tools/vendortest/jetstream/jetstream.CDjsSetup.js test/vendortest/JetStream-1.1/CDjsSetup.js
	cp tools/vendortest/jetstream/jetstream.OctaneSetup.js test/vendortest/JetStream-1.1/OctaneSetup.js
	cp tools/vendortest/jetstream/jetstream.Octane2Setup.js test/vendortest/JetStream-1.1/Octane2Setup.js
	cp tools/vendortest/jetstream/jetstream.SimpleSetup.js test/vendortest/JetStream-1.1/SimpleSetup.js
	cp tools/vendortest/jetstream/jetstream.SunSpiderSetup.js test/vendortest/JetStream-1.1/SunSpiderSetup.js
	cp tools/vendortest/jetstream/jetstream.cdjs.util.js test/vendortest/JetStream-1.1/cdjs/util.js
	cp tools/vendortest/jetstream/jetstream.runOnePlan.js test/vendortest/JetStream-1.1/runOnePlan.js
	cp tools/vendortest/jetstream/jetstream.run.sh test/vendortest/JetStream-1.1/run.sh
	cd test/vendortest/JetStream-1.1/; \
	./run.sh ../../../escargot $(TARGET_TEST); \
	cd -; \
	python tools/vendortest/jetstream/parsingResults.py tools/vendortest/jetstream/jetstream-result-raw.res $(TARGET_TEST);

run-jetstream-only-simple:
	make run-jetstream TARGET_TEST="simple"

run-jetstream-only-cdjs:
	make run-jetstream TARGET_TEST="cdjs"

run-jetstream-only-sunspider:
	make run-jetstream TARGET_TEST="sunspider"

run-jetstream-only-octane:
	make run-jetstream TARGET_TEST="octane"

run-chakracore:
	cp tools/vendortest/chakracore/chakracore.run.sh test/vendortest/ChakraCore/run.sh
	cp tools/vendortest/chakracore/chakracore.include.js test/vendortest/ChakraCore/include.js
	cp tools/vendortest/chakracore/chakracore.rlexedirs.xml test/vendortest/ChakraCore/rlexedirs.xml
	cp tools/vendortest/chakracore/Array.rlexe.xml test/vendortest/ChakraCore/Array/rlexe.xml
	cp tools/vendortest/chakracore/Error.rlexe.xml test/vendortest/ChakraCore/Error/rlexe.xml
	cp tools/vendortest/chakracore/Function.rlexe.xml test/vendortest/ChakraCore/Function/rlexe.xml
	cp tools/vendortest/chakracore/Miscellaneous.rlexe.xml test/vendortest/ChakraCore/Miscellaneous/rlexe.xml
	cp tools/vendortest/chakracore/Strings.rlexe.xml test/vendortest/ChakraCore/Strings/rlexe.xml
	cp tools/vendortest/chakracore/es6.rlexe.xml test/vendortest/ChakraCore/es6/rlexe.xml
	$(eval BIN_ARCH:=$(shell [[ "$(shell file escargot)" == *"32-bit"* ]] && echo "x86" || echo "x86_64"))
	test/vendortest/ChakraCore/run.sh ./escargot | tee tools/vendortest/chakracore.$(BIN_ARCH).gen.txt; \
	diff tools/vendortest/chakracore.$(BIN_ARCH).orig.txt tools/vendortest/chakracore.$(BIN_ARCH).gen.txt

run-v8:
	cp tools/vendortest/v8/v8.mjsunit.status test/vendortest/v8/test/mjsunit/mjsunit.status
	cp tools/vendortest/v8/v8.mjsunit.js test/vendortest/v8/test/mjsunit/mjsunit.js
	cp tools/vendortest/v8/v8.run-tests.py test/vendortest/v8/tools/run-tests.py
	cp tools/vendortest/v8/v8.testsuite.py test/vendortest/v8/tools/testrunner/local/testsuite.py
	cp tools/vendortest/v8/v8.execution.py test/vendortest/v8/tools/testrunner/local/execution.py
	cp tools/vendortest/v8/v8.progress.py test/vendortest/v8/tools/testrunner/local/progress.py
	$(eval ARCH:=$(shell [[ "$(shell file escargot)" == *"32-bit"* ]] && echo "x32" || echo "x64"))
	./test/vendortest/v8/tools/run-tests.py --quickcheck --no-presubmit --no-variants --arch-and-mode=$(ARCH).release --shell-dir ../../../ --escargot --report -p verbose --no-sorting mjsunit | tee tools/vendortest/v8.$(ARCH).mjsunit.gen.txt; \
	diff tools/vendortest/v8.$(ARCH).mjsunit.orig.txt tools/vendortest/v8.$(ARCH).mjsunit.gen.txt
