var console = {};
console.log = print;
var JetStream = (function() {
    var isRunning;
    var currentPlan;
    //var currentIteration;
    //var numberOfIterations;
    var accumulator;

    var givenPlans = [];
    var givenReferences = {};
    var plans;
    var benchmarks;

    var filename = {
        "towers.c" : ["simple/towers.c.js"],
        "container.cpp" : ["simple/container.cpp.js"],
        "dry.c" : ["simple/dry.c.js"],
        "n-body.c" : ["simple/n-body.c.js"],
        "quicksort.c" : ["simple/quicksort.c.js"],
        "gcc-loops.cpp" : ["simple/gcc-loops.cpp.js"],
        "hash-map" : ["simple/hash-map.js"],
        "float-mm.c" : ["simple/float-mm.c.js"],
        "bigfib.cpp" : ["simple/bigfib.cpp.js"],
        "3d-cube" : ["sunspider/3d-cube.js"],
        "3d-raytrace" : ["sunspider/3d-raytrace.js"],
        "base64" : ["sunspider/base64.js"],
        "crypto-aes" : ["sunspider/crypto-aes.js"],
        "crypto-md5" : ["sunspider/crypto-md5.js"],
        "crypto-sha1" : ["sunspider/crypto-sha1.js"],
        "date-format-tofte" : ["sunspider/date-format-tofte.js"],
        "date-format-xparb" : ["sunspider/date-format-xparb.js"],
        "n-body" : ["sunspider/n-body.js"],
        "regex-dna" : ["sunspider/regex-dna.js"],
        "tagcloud" : ["sunspider/tagcloud.js"],

        "cdjs" : ["cdjs/constants.js", "cdjs/util.js", "cdjs/red_black_tree.js", "cdjs/call_sign.js", "cdjs/vector_2d.js", "cdjs/vector_3d.js",
                    "cdjs/motion.js", "cdjs/reduce_collision_set.js", "cdjs/simulator.js", "cdjs/collision.js",
                    "cdjs/collision_detector.js", "cdjs/benchmark.js"],

        "code-multi-load" : ["Octane/code-load.js"],

        "code-first-load" : ["Octane2/code-load.js"],
        "typescript" : ["Octane2/typescript.js", "Octane2/typescript-input.js", "Octane2/typescript-compiler.js"],
        "box2d" : ["Octane2/box2d.js"],
        "crypto" : ["Octane2/crypto.js"],
        "delta-blue" : ["Octane2/deltablue.js"],
        "earley-boyer" : ["Octane2/earley-boyer.js"],
        "gbemu" : ["Octane2/gbemu-part1.js", "Octane2/gbemu-part2.js"],
        "mandreel" : ["Octane2/mandreel.js"],
        "navier-stokes" : ["Octane2/navier-stokes.js"],
        "pdfjs" : ["Octane2/pdfjs.js"],
        "proto-raytracer" : ["Octane2/raytrace.js"],
        "regexp-2010" : ["Octane2/regexp.js"],
        "richards" : ["Octane2/richards.js"],        
        "splay" : ["Octane2/splay.js"],
        "zlib" : ["Octane2/zlib.js", "Octane2/zlib-data.js"],
    }
    function addPlan(plan)
    {
       givenPlans.push(plan); 
    }
    function addReferences(references)
    {
        for (var s in references)
            givenReferences[s] = references[s];
    }

    function reset()
    {
        benchmarks = []; 
  
        for (var i = 0; i < givenPlans.length; ++i) {
            var plan = givenPlans[i];
            for (var j = 0; j < plan.benchmarks.length; ++j) {
                var benchmark = plan.benchmarks[j];
                benchmarks.push(benchmark);
                benchmark.plan = plan;
                benchmark.reference = givenReferences[benchmark.name] || 1;
            }
        }
        // (in JetStream-1.1) # of plans : 37, # of benchmarks : 39

        plans = givenPlans;

        for (var i = benchmarks.length; i--;) {
            benchmarks[i].results = [];
            benchmarks[i].times = [];
        }
    }

    function runCode(plan)
    {
        var benchmarkSuiteType = plan.ancestor; // sunspider, octane, cdjs, ...

        if(benchmarkSuiteType == "simple") {
            var __time_before = JetStream.goodTime();
            load(filename[plan.name][0]);
            var __time_after = JetStream.goodTime();
            JetStream.reportResult(__time_after - __time_before);
        } else if(benchmarkSuiteType == "sunspider") {
            sunSpiderCPUWarmup();
            var __data = JetStream.getAccumulator() || {sum: 0, n: 0};
            var __time_before = JetStream.goodTime();

            load(filename[plan.name][0]);

            var __time_after = JetStream.goodTime();
            __data.sum += Math.max(__time_after - __time_before, 1);
            __data.n++;
            if (__data.n == 1) {
                JetStream.reportResult(__data.sum / __data.n);
            }
            else {
                JetStream.accumulate(__data);
            }
        
        } else if (benchmarkSuiteType == "cdjs") {
            for(var i = 0; i < filename[plan.name].length; i++){
                load(filename[plan.name][i]);
            }
            var __result = benchmark();
            JetStream.reportResult(__result);
        } else if (benchmarkSuiteType == "Octane") {
            load("Octane/base.js");
            for(var i = 0; i < filename[plan.name].length; i++){
                load(filename[plan.name][i]);
            }
            BenchmarkSuite.scores = [];
            var __suite = BenchmarkSuite.suites[0];
            for (var __thing = __suite.RunStep({}); __thing; __thing = __thing());
            JetStream.reportResult(BenchmarkSuite.GeometricMean(__suite.results) / 1000);
        
        } else if (benchmarkSuiteType == "Octane2") {
            load("Octane2/base.js");
            for(var i = 0; i < filename[plan.name].length; i++){
                load(filename[plan.name][i]);
            }
            BenchmarkSuite.scores = [];
            var __suite = BenchmarkSuite.suites[0];
            for (var __thing = __suite.RunStep({}); __thing; __thing = __thing());
            JetStream.reportResult(BenchmarkSuite.GeometricMeanTime(__suite.results) / 1000, BenchmarkSuite.GeometricMeanLatency(__suite.results) / 1000);
        }
    }

    function computeRawResults()
    {
        function rawResultsLine(values) {
            return {
                result: values,
                statistics: null//computeStatistics(values)
            };
        }
        
        var rawResults = {};
        for (var i = 0; i < benchmarks.length; ++i) {
            var line = rawResultsLine(benchmarks[i].results);
            line.times = benchmarks[i].times;
            line.category = benchmarks[i].category;
            line.reference = benchmarks[i].reference;
            rawResults[benchmarks[i].name] = line;
            if (line.result != undefined && line.result != null && line.result != "")
                print(benchmarks[i].name + " : " + line.result);
        }
      //  rawResults.geomean = rawResultsLine(computeGeomeans(allSelector));

        return rawResults;
    }

    function end()
    {
        computeRawResults();
    }

    function iterate()
    {
        accumulator = void 0;
        if(!isRunning){
            end();
            return;
        }
    
        var d = new Date();
        while(new Date() - d < 100) {} // sleep for 100ms
        isRunning = !isRunning;
        runCode(plans[currentPlan]);
    }
    function start()
    {
        reset();
        isRunning = true;
        iterate();
    }

    function initialize()
    {
        currentPlan = arguments[0];
        reset();
    }

    function reportResult()
    {   
        var plan = plans[currentPlan];
        for (var i = plan.benchmarks.length; i--;) {
            var benchmark = plan.benchmarks[i];
            benchmark.times.push(arguments[i]);
            benchmark.results.push(100 * benchmark.reference / arguments[i]);
        }
        iterate();
    }   

    function accumulate(data)
    {   
        accumulator = data;
        
        if (!isRunning)
            return;
        runCode(plans[currentPlan]);
    }   

    function getAccumulator()
    {   
        return accumulator;
    }   

    return {
        addPlan: addPlan,
        addReferences: addReferences,
        initialize: initialize,
        start: start,
        reportResult: reportResult,
//        reportError: reportError,
        accumulate: accumulate,
        getAccumulator: getAccumulator,
        goodTime: Date.now
    }; 
})();

// It is converted form of 'index.html'


load("SunSpiderPayload.js");
load("SimplePayload.js");
load("SunSpiderSetup.js"); // 1 ~ 10
load("SimpleSetup.js");
load("OctaneSetup.js");
load("Octane2Setup.js");

load("CDjsSetup.js");

load("Reference.js");

JetStream.initialize(arguments[0]);
JetStream.start();



