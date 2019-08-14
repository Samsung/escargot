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
}
