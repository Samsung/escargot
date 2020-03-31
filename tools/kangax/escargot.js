/*
 *  Node.js test runner for running data-*.js tests with escargot 'escargot' command.
 *
 *  Reports discrepancies to console; fix them manually in data-*.js files.
 *  Expects 'escargot' to be already built.  Example:
 *
 *    $ node escargot.js /path/to/escargot [suitename]
 */

var fs = require('fs');
var child_process = require('child_process');
var console = require('console');

var testCount = 0;
var testSuccess = 0;
var testOutOfDate = 0;

var escargotCommand = process.argv[2];
var suites = process.argv.slice(3);

var environments = JSON.parse(fs.readFileSync('environments.json').toString());

// Key for .res (e.g. test.res.escargotscript1_0), automatic based on `escargot --version`.
var escargotKey = (function () {
        return '1';
})();
console.log('escargot result key is: test.res.' + escargotKey);
// escargotKey = "escargot2_3_0" // uncomment this line to test pre 2.3.0

// List of keys for inheriting results from previous versions.
var escargotKeyList = (function () {
    var res = [];
    for (var k in environments) {
        var env = environments[k];
        if (env.family !== 'Escargot') {
            continue;
        }
        res.push(k);
        if (k === escargotKey) {
            // Include versions up to 'escargotKey' but not newer.
            break;
        }
    }
    return res;
})();
console.log('escargot key list for inheriting results is:', escargotKeyList);

var createIterableHelper =
'var global = this;\n' +
'global.__createIterableObject = function (arr, methods) {\n' +
'    methods = methods || {};\n' +
'    if (typeof Symbol !== "function" || !Symbol.iterator)\n' +
'      return {};\n' +
'    arr.length++;\n' +
'    var iterator = {\n' +
'      next: function() {\n' +
'        return { value: arr.shift(), done: arr.length <= 0 };\n' +
'      },\n' +
'      "return": methods["return"],\n' +
'      "throw": methods["throw"]\n' +
'    };\n' +
'    var iterable = {};\n' +
'    iterable[Symbol.iterator] = function(){ return iterator; };\n' +
'    return iterable;\n' +
'  };\n';

var asyncTestHelperHead =
'var asyncPassed = false;\n' +
'\n' +
'function asyncTestPassed() {\n' +
'  asyncPassed = true;\n' +
'}\n' +
'\n' +
'function setTimeout(cb, time, ...cbarg) {\n' +
'  if (!jobqueue[time]) {\n' +
'    jobqueue[time] = [];\n' +
'  }\n' +
'  jobqueue[time].push({cb, cbarg});\n' +
'}\n' +
'\n' +
'var jobqueue = [];\n';

var asyncTestHelperTail =
'jobqueue.forEach(function(jobs) {\n' +
'  for (var job of jobs) {\n' +
'    job.cb(...job.cbarg);\n' +
'  }\n' +
'});\n' +
'\n' +
'function onCloseAsyncCheck() {\n' +
'  if (!asyncPassed) {\n' +
'    print("Async[FAILURE]");\n' +
'    throw "Async check failed";\n' +
'  }\n' +
'  print("[SUCCESS]");\n' +
'}\n';

// Run test / subtests, recursively.  Report results, indicate data files
// which are out of date.
function runTest(parents, test, sublevel) {
    var testPath = parents.join(' -> ') + ' -> ' + test.name;

    if (typeof test.exec === 'function') {
        var src = test.exec.toString();
        var m = /^function\s*\w*\s*\(.*?\)\s*\{\s*\/\*([\s\S]*?)\*\/\s*\}$/m.exec(src);
        var evalcode;
        var processArgs = ['escargottest.js'];
        var script = '';

        if (src.includes('__createIterableObject')) {
            script += createIterableHelper;
        } else if (src.includes('global')) {
            script += 'var global = this;\n';
        }

        if (src.includes('asyncTestPassed')) {
            script += asyncTestHelperHead + m[1] + asyncTestHelperTail;
            processArgs += '-e' + 'onCloseAsyncCheck()';
        } else {
            if (m) {
                evalcode = '(function test() {' + m[1] + '})();';
            } else {
                evalcode = '(' + src + ')()';
            }

            script += 'var evalcode = ' + JSON.stringify(evalcode) + ';\n' +
                     'try {\n' +
                     '    var res = eval(evalcode);\n' +
                     '    if (res !== true && res !== 1) { throw new Error("failed: " + res); }\n' +
                     '    print("[SUCCESS]");\n' +
                     '} catch (e) {\n' +
                     '    print("[FAILURE]", e);\n' +
                     '    /*throw e;*/\n' +
                     '}\n';
        }

        fs.writeFileSync(processArgs[processArgs.length - 1], script);
        var success = false;
        try {
            var stdout = child_process.execFileSync(escargotCommand, processArgs, {
                encoding: 'utf-8'
            });
            //console.log(stdout);

            if (/^\[SUCCESS\]$/gm.test(stdout)) {
                success = true;
                testSuccess++;
            } else {
                //console.log(stdout);
            }
        } catch (e) {
            //console.log(e);
        }
        testCount++;

        if (test.res) {
            // Take expected result from newest escargot version not newer
            // than current version.
            var expect = void 0;
            escargotKeyList.forEach(function (k) {
                if (test.res[k] !== void 0) {
                    expect = test.res[k];
                }
            });

            if (expect === success) {
                // Matches.
            } else if (expect === void 0 && !success) {
                testOutOfDate++;
                console.log(testPath + ': test result missing, res: ' + expect + ', actual: ' + success);
            } else {
                testOutOfDate++;
                console.log(testPath + ': test result out of date, res: ' + expect + ', actual: ' + success);
            }
        } else {
            testOutOfDate++;
            console.log(testPath + ': test.res missing');
        }
    }
    if (test.subtests) {
        var newParents = parents.slice(0);
        newParents.push(test.name);
        test.subtests.forEach(function (v) { runTest(newParents, v, sublevel + 1); });
    }
}

fs.readdirSync('.').forEach(function (filename) {
    var m = /^data-(.*)\.js$/.exec(filename);
    if (!m) {
        return;
    }
    var suitename = m[1];
    if (suites.length != 0 && !suites.includes(suitename)) {
        return;
    }

    console.log('');
    console.log('**** ' + suitename + ' ****');
    console.log('');
    var testsuite = require('./data-' + suitename + '.js');
    testsuite.tests.forEach(function (v) { runTest([ suitename ], v, 0); });
});

console.log(testCount + ' tests executed: ' + testSuccess + ' success, ' + (testCount - testSuccess) + ' fail');
console.log(testOutOfDate + ' tests are out of date (data-*.js file .res)');
