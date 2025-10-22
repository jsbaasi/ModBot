pipeline {
    agent any

    stages {
        stage('Stop previous deployment') {
            steps {
                echo 'Stopping previous deployment..'
            }
        }
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
        stage('Deploy') {
            steps {
                echo 'Daemonising and deploying..'
                echo "Dont forget to launch with prod argument"`
            }
        } 
    }
}