node {
    def git_url = "https://github.com/Samsung/escargot.git"
    stage("Cleanup") {
        sh 'rm -rf ./*'
    }
    stage("Get source") {
        git git_url
    }
    stage('Submodule update') {
        sh 'git submodule update --init'
    }
    stage('Check tidy') {
        sh 'python tools/check_tidy.py'
    }
    stage('Prepare build') {
        parallel (
            'npm' : {
                sh 'npm install'
            },
            '32bit' : {
                sh 'LDFLAGS=" -L/usr/icu32/lib/  -Wl,-rpath=/usr/icu32/lib/" cmake  -H./ -Bbuild/out_linux -DESCARGOT_HOST=linux -DESCARGOT_ARCH=x86 -DESCARGOT_MODE=debug -DESCARGOT_OUTPUT=bin -DVENDORTEST=1 -GNinja'
                sh 'LDFLAGS=" -L/usr/icu32/lib/  -Wl,-rpath=/usr/icu32/lib/" cmake  -H./ -Bbuild/out_linux_release -DESCARGOT_HOST=linux -DESCARGOT_ARCH=x86 -DESCARGOT_MODE=release -DESCARGOT_OUTPUT=bin -DVENDORTEST=1 -GNinja'
                sh 'gcc -shared -m32 -fPIC -o backtrace-hooking-32.so tools/test/test262/backtrace-hooking.c'
            },
            '64bit' : {
                sh 'cmake  -H./ -Bbuild/out_linux64 -DESCARGOT_HOST=linux -DESCARGOT_ARCH=x64 -DESCARGOT_MODE=debug -DESCARGOT_OUTPUT=bin -DVENDORTEST=1 -GNinja'
                sh 'cmake  -H./ -Bbuild/out_linux64_release -DESCARGOT_HOST=linux -DESCARGOT_ARCH=x64 -DESCARGOT_MODE=release -DESCARGOT_OUTPUT=bin -DVENDORTEST=1 -GNinja'
                sh 'gcc -shared -fPIC -o backtrace-hooking-64.so tools/test/test262/backtrace-hooking.c'
            }
        )
    }
    stage('Build') {
        sh 'cd build/out_linux/; ninja '
        sh 'cd build/out_linux64/; ninja'
        sh 'cd build/out_linux_release/; ninja'
        sh 'cd build/out_linux64_release/; ninja'
    }
    stage('Running test') {
        parallel (
            'release-32bit-jetstream-1' : {
                sh 'tools/run-tests.py --arch=x86 --engine="${WORKSPACE}/build/out_linux_release/escargot" jetstream-only-simple-parallel-1'
            },
            'release-32bit-jetstream-2' : {
                sh 'tools/run-tests.py --arch=x86 --engine="${WORKSPACE}/build/out_linux_release/escargot" jetstream-only-simple-parallel-2'
            },
            'release-32bit-jetstream-3' : {
                sh 'tools/run-tests.py --arch=x86 --engine="${WORKSPACE}/build/out_linux_release/escargot" jetstream-only-simple-parallel-3'
            },
            'release-64bit-jetstream-1' : {
                sh 'tools/run-tests.py --arch=x86_64 --engine="${WORKSPACE}/build/out_linux64_release/escargot" jetstream-only-simple-parallel-1'
            },
            'release-64bit-jetstream-2' : {
                sh 'tools/run-tests.py --arch=x86_64 --engine="${WORKSPACE}/build/out_linux64_release/escargot" jetstream-only-simple-parallel-2'
            },
            'release-64bit-jetstream-3' : {
                sh 'tools/run-tests.py --arch=x86_64 --engine="${WORKSPACE}/build/out_linux64_release/escargot" jetstream-only-simple-parallel-3'
            },
            'release-32bit-octane' : {
                sh 'tools/run-tests.py --arch=x86 --engine="${WORKSPACE}/build/out_linux_release/escargot" octane'
            },
            'release-64bit-octane' : {
                sh 'tools/run-tests.py --arch=x86_64 --engine="${WORKSPACE}/build/out_linux64_release/escargot" octane'
            },
            'release-chakracore' : {
                sh 'tools/run-tests.py --arch=x86 --engine="${WORKSPACE}/build/out_linux_release/escargot" chakracore'
                sh 'tools/run-tests.py --arch=x86_64 --engine="${WORKSPACE}/build/out_linux64_release/escargot" chakracore'
            },
            '32bit' : {
                sh 'tools/run-tests.py --arch=x86 --engine="${WORKSPACE}/build/out_linux/escargot" internal regression-tests es2015 intl'
                sh 'GC_FREE_SPACE_DIVISOR=1 ESCARGOT_LD_PRELOAD=${WORKSPACE}/backtrace-hooking-32.so tools/run-tests.py --arch=x86 --engine="${WORKSPACE}/build/out_linux/escargot" test262'
                sh 'tools/run-tests.py --arch=x86 --engine="${WORKSPACE}/build/out_linux_release/escargot" jetstream-only-cdjs internal jsc-stress'
                sh 'tools/run-tests.py --arch=x86 --engine="${WORKSPACE}/build/out_linux_release/escargot" v8 spidermonkey regression-tests es2015 intl test262'
            },
            '64bit' : {
                sh 'tools/run-tests.py --arch=x86_64 --engine="${WORKSPACE}/build/out_linux64/escargot" internal regression-tests es2015 intl'
                sh 'GC_FREE_SPACE_DIVISOR=1 ESCARGOT_LD_PRELOAD=${WORKSPACE}/backtrace-hooking-64.so tools/run-tests.py --arch=x86_64 --engine="${WORKSPACE}/build/out_linux64/escargot" test262'
                sh 'tools/run-tests.py --arch=x86_64 --engine="${WORKSPACE}/build/out_linux64_release/escargot" jetstream-only-cdjs internal jsc-stress'
                sh 'tools/run-tests.py --arch=x86_64 --engine="${WORKSPACE}/build/out_linux64_release/escargot" v8 spidermonkey regression-tests es2015 intl test262'
            },
        )
    }
    stage("End") {
        echo "end"
    }
}
