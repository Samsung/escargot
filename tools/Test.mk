OPT_CH10=ch10/10.1 ch10/10.2 ch10/10.4 ch10/10.5

OPT_CH11=ch11/11.1 ch11/11.10 ch11/11.11 ch11/11.12 ch11/11.13 ch11/11.14 ch11/11.5 ch11/11.6 ch11/11.7 ch11/11.8 ch11/11.9

OPT_CH15_1=ch15/15.1/15.1.1 ch15/15.1/15.1.2 # ch15/15.1/15.1.3 (take too long)

OPT_CH15_2_3=ch15/15.2/15.2.3/15.2.3.1/ ch15/15.2/15.2.3/15.2.3.2 ch15/15.2/15.2.3/15.2.3.3 ch15/15.2/15.2.3/15.2.3.4 ch15/15.2/15.2.3/15.2.3.5 ch15/15.2/15.2.3/15.2.3.8 ch15/15.2/15.2.3/15.2.3.9 ch15/15.2/15.2.3/15.2.3.10 ch15/15.2/15.2.3/15.2.3.11 ch15/15.2/15.2.3/15.2.3.12 ch15/15.2/15.2.3/15.2.3.13 ch15/15.2/15.2.3/15.2.3.14 # 15.2.3.6 15.2.3.7 (getter.setter can be undefined / arguments binding should be dynamically decided)
OPT_CH15_2=ch15/15.2/15.2.1 ch15/15.2/15.2.2 $(OPT_CH15_2_3) ch15/15.2/15.2.4

OPT_CH15_3_4=ch15/15.3/15.3.4/15.3.4.1 ch15/15.3/15.3.4/15.3.4.2 ch15/15.3/15.3.4/15.3.4.3 ch15/15.3/15.3.4/15.3.4.4 # ch15/15.3/15.3.4/15.3.4.5 (Function.prototype.bind)
OPT_CH15_3=ch15/15.3/S15.3.1 ch15/15.3/15.3.2 ch15/15.3/15.3.3 $(OPT_CH15_3_4) ch15/15.3/15.3.5

OPT_CH15=$(OPT_CH15_1) $(OPT_CH15_2) $(OPT_CH15_3)

check:
	make tidy-update
	make x64.interpreter.release -j$(NPROCS)
	# make run-sunspider | tee out/sunspider_result
	make run-test262 OPT="ch06 ch07 ch08 ch09 $(OPT_CH10) $(OPT_CH11) ch12 ch13 ch14 $(OPT_CH15)"

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
	cd test/test262/; \
	python tools/packaging/test262.py --command ../../escargot $(OPT) --full-summary

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
