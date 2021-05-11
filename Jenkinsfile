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
                sh 'git submodule init third_party/GCutil/'
                sh 'git submodule update'
            }

            stage('Prepare build(gcc)') {
                sh 'LDFLAGS=" -L/usr/icu32/lib/ -Wl,-rpath=/usr/icu32/lib/" PKG_CONFIG_PATH="/usr/icu32/lib/pkgconfig/" cmake -H./ -Bbuild/out_linux -DESCARGOT_HOST=linux -DESCARGOT_ARCH=x86 -DESCARGOT_MODE=debug -DESCARGOT_OUTPUT=shell_test -GNinja'
                sh 'LDFLAGS=" -L/usr/icu32/lib/ -Wl,-rpath=/usr/icu32/lib/" PKG_CONFIG_PATH="/usr/icu32/lib/pkgconfig/" cmake -H./ -Bbuild/out_linux_release -DESCARGOT_HOST=linux -DESCARGOT_ARCH=x86 -DESCARGOT_MODE=release -DESCARGOT_OUTPUT=shell_test -GNinja'
                sh 'gcc -shared -m32 -fPIC -o backtrace-hooking-32.so tools/test/test262/backtrace-hooking.c'
                sh 'LDFLAGS=" -L/usr/icu64/lib/ -Wl,-rpath=/usr/icu64/lib/" PKG_CONFIG_PATH="/usr/icu64/lib/pkgconfig/" cmake -H./ -Bbuild/out_linux64 -DESCARGOT_HOST=linux -DESCARGOT_ARCH=x64 -DESCARGOT_MODE=debug -DESCARGOT_OUTPUT=shell_test -GNinja'
                sh 'LDFLAGS=" -L/usr/icu64/lib/ -Wl,-rpath=/usr/icu64/lib/" PKG_CONFIG_PATH="/usr/icu64/lib/pkgconfig/" cmake -H./ -Bbuild/out_linux64_release -DESCARGOT_HOST=linux -DESCARGOT_ARCH=x64 -DESCARGOT_MODE=release -DESCARGOT_OUTPUT=shell_test -GNinja'
            }

            stage('Build(gcc)') {
                parallel (
                    'release-build' : {
                        sh 'cd build/out_linux/; ninja '
                        sh 'cd build/out_linux64/; ninja'
                    },
                    'debug-build' : {
                        sh 'cd build/out_linux_release/; ninja'
                        sh 'cd build/out_linux64_release/; ninja'
                    },
                )
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
                        sh 'GC_FREE_SPACE_DIVISOR=1 tools/run-tests.py --arch=x86 --engine="${WORKSPACE}/build/out_linux_release/escargot" jetstream-only-cdjs'
                    },
                    'release-32bit-octane' : {
                        sh 'GC_FREE_SPACE_DIVISOR=1 tools/run-tests.py --arch=x86 --engine="${WORKSPACE}/build/out_linux_release/escargot" octane'
                    },
                    '32bit-tc-test' : {
                        sh 'tools/run-tests.py --arch=x86 --engine="${WORKSPACE}/build/out_linux/escargot" modifiedVendorTest regression-tests new-es intl sunspider-js'
                        sh 'tools/run-tests.py --arch=x86 --engine="${WORKSPACE}/build/out_linux_release/escargot" modifiedVendorTest jsc-stress sunspider-js regression-tests new-es intl'
                        sh 'tools/run-tests.py --arch=x86 --engine="${WORKSPACE}/build/out_linux_release/escargot" v8 chakracore spidermonkey'
                    },
                    'debug-32bit-test262' : {
                        sh 'GC_FREE_SPACE_DIVISOR=1 ESCARGOT_LD_PRELOAD=${WORKSPACE}/backtrace-hooking-32.so tools/run-tests.py --arch=x86 --engine="${WORKSPACE}/build/out_linux/escargot" test262'
                    },
                    'release-64bit-test262' : {
                        sh 'GC_FREE_SPACE_DIVISOR=1 tools/run-tests.py --arch=x86_64 --engine="${WORKSPACE}/build/out_linux64_release/escargot" test262'
                    },
                    'kangax test-suites' : {
                        sh 'python tools/kangax/run-kangax.py --engine="${WORKSPACE}/build/out_linux64/escargot"'
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
