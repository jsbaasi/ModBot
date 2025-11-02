pipeline {
    agent any

    stages {
        stage('Configure') {
            steps {
                sh 'cmake -B build'
            }
        }
        stage('Build') {
            steps {
                sh 'cmake --build build'
            }
        }
        stage('Test') {
            steps {
                echo 'Testing..'
            }
        }
        stage('Publish') {
            steps {
                archiveArtifacts artifacts: 'build/ModBot'
                sh 'cp build/ModBot'
            }
        }
        stage('Stop previous deployment') {
            steps {
                echo 'Stopping previous deployment..'
                sh 'sudo kill `cat /ModBot/lock`'
            }
        }
        stage('Deploy') {
            steps {
                echo 'Daemonising and deploying..'
                echo 'Dont forget to launch with prod argument'
                sh 'daemonize -c /ModBot -e /ModBot/error.log -o /ModBot/out.log -p /ModBot/lock -l /ModBot/lock -v /ModBot/build/ModBot prod'
            }
        } 
    }
}