
# TODO this list should be maintained in a better way
OPT_MASTER_PROMISE=built-ins/Promise
OPT_MASTER_ARRAYBUFFER=#built-ins/ArrayBuffer
OPT_MASTER_DATAVIEW=#built-ins/DataView
OPT_MASTER_TYPEDARRAY=#built-ins/TypedArray built-ins/TypedArrays

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
	cd test/SunSpider/; \
	./sunspider --shell=../../escargot --suite=sunspider-1.0.2

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
	npm install
	node_modules/.bin/babel test/vendortest/SpiderMonkey/ecma_6/Promise --out-dir test/vendortest/SpiderMonkey/ecma_6/Promise
	rm test/vendortest/SpiderMonkey/shell.js
	ln -s `pwd`/tools/vendortest/spidermonkey.shell.js test/vendortest/SpiderMonkey/shell.js
	rm test/vendortest/SpiderMonkey/ecma_6/shell.js
	ln -s `pwd`/tools/vendortest/spidermonkey.ecma_6.shell.js test/vendortest/SpiderMonkey/ecma_6/shell.js
	$(eval BIN_ARCH:=$(shell [ "$(shell file escargot)" == *"32-bit"* ] && echo "x86" || echo "x86_64"))
	(./test/vendortest/SpiderMonkey/jstests.py -s --xul-info=$(BIN_ARCH)-gcc3:Linux:false ./escargot \
		--output-file=./tools/vendortest/spidermonkey.log.txt \
		--failure-file=../../../tools/vendortest/spidermonkey.gen.txt $(OPT) ) || (echo done)
	sort tools/vendortest/spidermonkey.gen.txt -o tools/vendortest/spidermonkey.gen.txt

run-spidermonkey-full:
	make run-spidermonkey \
		OPT="ecma/ ecma_2/ ecma_3/ ecma_3_1/ ecma_5/ ecma_6/Promise ecma_6/TypedArray ecma_6/ArrayBuffer \
			js1_1/ js1_2/ js1_3/ js1_4/ js1_5/ js1_6/ js1_7/ js1_8/ js1_8_1/ js1_8_5/ shell/ supporting/"
	diff tools/vendortest/spidermonkey.orig.txt tools/vendortest/spidermonkey.gen.txt

run-jsc-stress:
	cp tools/vendortest/jsc.stress.resource.typedarray-constructor-helper-functions.js test/vendortest/JavaScriptCore/stress/resources/typedarray-constructor-helper-functions.js
	PYTHONPATH=. ./tools/vendortest/driver.py -s stress;

run-jsc-mozilla:
	cd test/JavaScriptCore/mozilla/; \
		perl jsDriver.pl -e escargot -s ../../../escargot

run-jetstream:
	cd test/JetStream-standalone-escargot/JetStream-1.1/; \
		./run.sh ../../../escargot; \
		python parsingResults.py jetstream-result-raw.res;

run-chakracore:
	cd test/chakracore/; \
	./run.sh ../../escargot $(OPT) | tee chakracorelog.gen.txt; \
	diff chakracorelog.orig.txt chakracorelog.gen.txt

run-v8-donotuse:
	cp tools/vendortest/v8/v8.mjsunit.status test/vendortest/v8/test/mjsunit/mjsunit.status
	cp tools/vendortest/v8/v8.mjsunit.js test/vendortest/v8/test/mjsunit/mjsunit.js
	cp tools/vendortest/v8/v8.run-tests.py test/vendortest/v8/tools/run-tests.py
	cp tools/vendortest/v8/v8.testsuite.py test/vendortest/v8/tools/testrunner/local/testsuite.py
	cp tools/vendortest/v8/v8.execution.py test/vendortest/v8/tools/testrunner/local/execution.py
	cp tools/vendortest/v8/v8.progress.py test/vendortest/v8/tools/testrunner/local/progress.py
	./test/vendortest/v8/tools/run-tests.py --quickcheck --no-presubmit --no-variants --arch-and-mode=$(ARCH).release --escargot --report -p verbose --no-sorting mjsunit | tee tools/vendortest/v8.$(ARCH).mjsunit.gen.txt; \
	diff tools/vendortest/v8.$(ARCH).mjsunit.orig.txt tools/vendortest/v8.$(ARCH).mjsunit.gen.txt

run-v8-64:
	make run-v8-donotuse ARCH=x64

run-v8-32:
	make run-v8-donotuse ARCH=x32
