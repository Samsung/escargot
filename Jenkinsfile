def isPr() {
    env.CHANGE_ID != null
}
    node {
        try {
            stage("Get source") {
                def url = 'https://github.com/Samsung/escargot.git'

                if (isPr()) {
                    def refspec = "+refs/pull/${env.CHANGE_ID}/head:refs/remotes/origin/PR-${env.CHANGE_ID} +refs/heads/master:refs/remotes/origin/master"
                    def extensions = [[$class: 'PreBuildMerge', options: [mergeRemote: "refs/remotes/origin", mergeTarget: "PR-${env.CHANGE_ID}"]]]
                    checkout([
                        $class: 'GitSCM',
                        doGenerateSubmoduleConfigurations: false,
                        extensions: extensions,
                        submoduleCfg: [],
                        userRemoteConfigs: [[
                            refspec: refspec,
                            url: url
                        ]]
                    ])
                } else {
                    def refspec = "+refs/heads/master:refs/remotes/origin/master"
                    def extensions = []
                    checkout([
                        $class: 'GitSCM',
                        doGenerateSubmoduleConfigurations: false,
                        extensions: [[$class: 'WipeWorkspace']],
                        submoduleCfg: [],
                        userRemoteConfigs: [[
                            refspec: refspec,
                            url: url
                        ]]
                    ])
                }
            }

            stage('Check tidy') {
                sh 'python tools/check_tidy.py'
            }

            stage('Submodule update') {
                sh 'git submodule init test/'
                sh 'git submodule init third_party/GCutil'
                sh 'git submodule update'
            }

            stage('Prepare build(clang)') {
                sh 'CC=clang-6.0 CXX=clang++-6.0 LDFLAGS=" -L/usr/icu32/lib/  -Wl,-rpath=/usr/icu32/lib/" cmake  -H./ -Bbuild/out_linux_clang -DESCARGOT_HOST=linux -DESCARGOT_ARCH=x86 -DESCARGOT_MODE=debug -DESCARGOT_OUTPUT=shell_test -GNinja'
                sh 'CC=clang-6.0 CXX=clang++-6.0 cmake  -H./ -Bbuild/out_linux64_clang -DESCARGOT_HOST=linux -DESCARGOT_ARCH=x64 -DESCARGOT_MODE=debug -DESCARGOT_OUTPUT=shell_test -GNinja'
            }

            stage('Build(clang)') {
                sh 'cd build/out_linux_clang/; ninja '
                sh 'cd build/out_linux64_clang/; ninja'
            }

            stage('Prepare build(gcc)') {
            parallel (
                '32bit' : {
                    sh 'LDFLAGS=" -L/usr/icu32/lib/ -Wl,-rpath=/usr/icu32/lib/" cmake -H./ -Bbuild/out_linux -DESCARGOT_HOST=linux -DESCARGOT_ARCH=x86 -DESCARGOT_MODE=debug -DESCARGOT_OUTPUT=shell_test -GNinja'
                    sh 'LDFLAGS=" -L/usr/icu32/lib/ -Wl,-rpath=/usr/icu32/lib/" cmake -H./ -Bbuild/out_linux_release -DESCARGOT_HOST=linux -DESCARGOT_ARCH=x86 -DESCARGOT_MODE=release -DESCARGOT_OUTPUT=shell_test -GNinja'
                    sh 'gcc -shared -m32 -fPIC -o backtrace-hooking-32.so tools/test/test262/backtrace-hooking.c'
                },
                '64bit' : {
                    sh 'cmake -H./ -Bbuild/out_linux64 -DESCARGOT_HOST=linux -DESCARGOT_ARCH=x64 -DESCARGOT_MODE=debug -DESCARGOT_OUTPUT=shell_test -GNinja'
                    sh 'cmake -H./ -Bbuild/out_linux64_release -DESCARGOT_HOST=linux -DESCARGOT_ARCH=x64 -DESCARGOT_MODE=release -DESCARGOT_OUTPUT=shell_test -GNinja'
                    sh 'gcc -shared -fPIC -o backtrace-hooking-64.so tools/test/test262/backtrace-hooking.c'
                },
                'debugger-binary' : {
                  sh 'cmake -H./ -Bbuild/debugger_out_linux64 -DESCARGOT_HOST=linux -DESCARGOT_ARCH=x64 -DESCARGOT_MODE=debug -DESCARGOT_DEBUGGER=1 -DESCARGOT_OUTPUT=shell_test -GNinja'
                }
            )
            }

            stage('Build(gcc)') {
                sh 'cd build/out_linux/; ninja '
                sh 'cd build/out_linux64/; ninja'
                sh 'cd build/out_linux_release/; ninja'
                sh 'cd build/out_linux64_release/; ninja'
                sh 'cd build/debugger_out_linux64/; ninja'
            }

            stage('Running test') {
                timeout(30) {
                parallel (
                    'release-32bit-jetstream-1' : {
                        sh 'GC_FREE_SPACE_DIVISOR=1 tools/run-tests.py --arch=x86 --engine="${WORKSPACE}/build/out_linux_release/escargot" jetstream-only-simple-parallel-1'
                    },
                    'release-32bit-jetstream-2' : {
                        sh 'GC_FREE_SPACE_DIVISOR=1 tools/run-tests.py --arch=x86 --engine="${WORKSPACE}/build/out_linux_release/escargot" jetstream-only-simple-parallel-2'
                    },
                    'release-32bit-jetstream-3' : {
                        sh 'GC_FREE_SPACE_DIVISOR=1 tools/run-tests.py --arch=x86 --engine="${WORKSPACE}/build/out_linux_release/escargot" jetstream-only-simple-parallel-3'
                    },
                    'release-64bit-jetstream-1' : {
                        sh 'GC_FREE_SPACE_DIVISOR=1 tools/run-tests.py --arch=x86_64 --engine="${WORKSPACE}/build/out_linux64_release/escargot" jetstream-only-simple-parallel-1'
                    },
                    'release-64bit-jetstream-2' : {
                        sh 'GC_FREE_SPACE_DIVISOR=1 tools/run-tests.py --arch=x86_64 --engine="${WORKSPACE}/build/out_linux64_release/escargot" jetstream-only-simple-parallel-2'
                    },
                    'release-64bit-jetstream-3' : {
                        sh 'GC_FREE_SPACE_DIVISOR=1 tools/run-tests.py --arch=x86_64 --engine="${WORKSPACE}/build/out_linux64_release/escargot" jetstream-only-simple-parallel-3'
                    },
                    'release-octane' : {
                        sh 'GC_FREE_SPACE_DIVISOR=1 tools/run-tests.py --arch=x86 --engine="${WORKSPACE}/build/out_linux_release/escargot" octane'
                        sh 'GC_FREE_SPACE_DIVISOR=1 tools/run-tests.py --arch=x86_64 --engine="${WORKSPACE}/build/out_linux64_release/escargot" octane'
                    },
                    'release-chakracore' : {
                        sh 'tools/run-tests.py --arch=x86 --engine="${WORKSPACE}/build/out_linux_release/escargot" chakracore'
                        sh 'tools/run-tests.py --arch=x86_64 --engine="${WORKSPACE}/build/out_linux64_release/escargot" chakracore'
                    },
                    'debug-32bit-test262' : {
                        sh 'GC_FREE_SPACE_DIVISOR=1 ESCARGOT_LD_PRELOAD=${WORKSPACE}/backtrace-hooking-32.so tools/run-tests.py --arch=x86 --engine="${WORKSPACE}/build/out_linux/escargot" test262'
                    },
                    'debug-64bit-test262' : {
                        sh 'GC_FREE_SPACE_DIVISOR=1 ESCARGOT_LD_PRELOAD=${WORKSPACE}/backtrace-hooking-64.so tools/run-tests.py --arch=x86_64 --engine="${WORKSPACE}/build/out_linux64/escargot" test262'
                    },
                    '32bit' : {
                        sh 'tools/run-tests.py --arch=x86 --engine="${WORKSPACE}/build/out_linux/escargot" modifiedVendorTest regression-tests new-es intl sunspider-js'
                        sh 'tools/run-tests.py --arch=x86 --engine="${WORKSPACE}/build/out_linux_release/escargot" jetstream-only-cdjs modifiedVendorTest jsc-stress sunspider-js'
                        sh 'tools/run-tests.py --arch=x86 --engine="${WORKSPACE}/build/out_linux_release/escargot" v8 spidermonkey regression-tests new-es intl'
                    },
                    '64bit' : {
                        sh 'tools/run-tests.py --arch=x86_64 --engine="${WORKSPACE}/build/out_linux64/escargot" modifiedVendorTest regression-tests new-es intl sunspider-js'
                        sh 'tools/run-tests.py --arch=x86_64 --engine="${WORKSPACE}/build/out_linux64_release/escargot" jetstream-only-cdjs modifiedVendorTest jsc-stress sunspider-js'
                        sh 'tools/run-tests.py --arch=x86_64 --engine="${WORKSPACE}/build/out_linux64_release/escargot" v8 spidermonkey regression-tests new-es intl'
                    },
                    'Escargot-debugger-test-64bit' : {
                        sh 'tools/run-tests.py --arch=x86_64 --engine="${WORKSPACE}/build/debugger_out_linux64/escargot" escargot-debugger'
                    },
                )
            }
        }

        } catch (e) {
            throw e
        } finally {
            cleanWs()
        }
    }
