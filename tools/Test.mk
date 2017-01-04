
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

run-test262:
	cp excludelist.orig.xml test/test262/test/config/excludelist.xml
	cp test262.py test/test262/tools/packaging/test262.py
	cd test/test262/; \
	TZ="US/Pacific" python tools/packaging/test262.py --command ../../escargot $(OPT) --full-summary

run-test262-master:
	cp test/test262-harness-py-excludelist.xml test/test262-harness-py/excludelist.xml
	cp test/test262-harness-py-test262.py ./test/test262-harness-py/src/test262.py
	python ./test/test262-harness-py/src/test262.py --command ./escargot --tests=test/test262-master $(OPT) --full-summary

run-test262-wearable:
	ln -sf excludelist.subset.xml test/test262/test/config/excludelist.xml
	cd test/test262/; \
	python tools/packaging/test262.py --command ../../escargot $(OPT) --summary | sed 's/RELEASE_ASSERT_NOT_REACHED.*//g' | tee test262log.wearable.gen.txt; \
	diff test262log.wearable.orig.txt test262log.wearable.gen.txt

run-spidermonkey:
	cd test/SpiderMonkey; \
	./jstests.py -s --xul-info=x86_64-gcc3:Linux:false ../../escargot --failure-file=mozilla.x64.interpreter.release.escargot.gen.txt -p "$(OPT)"; \
	diff mozilla.x64.interpreter.release.escargot.orig.txt mozilla.x64.interpreter.release.escargot.gen.txt

run-spidermonkey-for-32bit:
	cd test/SpiderMonkey; \
	./jstests.py -s --xul-info=x86-gcc3:Linux:false ../../escargot --failure-file=mozilla.x86.interpreter.release.escargot.gen.txt -p "$(OPT)"; \
	diff mozilla.x86.interpreter.release.escargot.orig.txt mozilla.x64.interpreter.release.escargot.gen.txt

run-jsc-stress:
	PYTHONPATH=. ./tools/driver.py -s stress;

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

run-v8-test:
	./test/v8/tool/run-tests.py --quickcheck --no-presubmit --no-variants --arch-and-mode=x64.release --escargot --report -p verbose --no-sorting mjsunit | tee test/v8/mjsunit.gen.txt; \
	diff test/v8/mjsunit.orig.txt test/v8/mjsunit.gen.txt

run-v8-test-for-32bit:
	./test/v8/tool/run-tests.py --quickcheck --no-presubmit --no-variants --arch-and-mode=x32.release --escargot --report -p verbose --no-sorting mjsunit | tee test/v8/mjsunit.gen.txt; \
	diff test/v8/mjsunit.orig.txt test/v8/mjsunit.gen.txt
