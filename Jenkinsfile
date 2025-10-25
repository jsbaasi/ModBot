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
            }
        }
        stage('Stop previous deployment') {
            steps {
                echo 'Stopping previous deployment..'
            }
        }
        stage('Deploy') {
            steps {
                echo 'Daemonising and deploying..'
                echo 'Dont forget to launch with prod argument'
            }
        } 
    }
}