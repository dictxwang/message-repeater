#!/usr/bin/env bash

function status() {
    process_name=$1
    pid_file=$2

    if [[ -e ${pid_file} ]]; then
        pid=`cat ${pid_file}`
    else
        pid=""
    fi

    if [[ -n ${pid} ]]; then
        exists_pid=$(ps -ax | grep ${process_name} | grep ${pid} | grep -v grep | awk '{print $1}')

        if [[ -n ${exists_pid} && ${exists_pid} = ${pid} ]]; then
            return 1
        else
            return 0
        fi
    else
        return 0
    fi
}

function start_daemon() {
    process_name=$1
    pid_file=$2
    config_file=$3
    cpu_index=$4

    if [[ ! -e ${process_name} || ! -x ${process_name} ]]; then
      echo "'${process_name}' is not exists or is not executable."
      exit 0
    fi

    status ${process_name} ${pid_file}
    s=$?
    if [[ $s == 1 ]]; then
        echo "process had started."
        exit 0
    fi

    # nohup ./${process_name} ${config_file} > /dev/null 2>&1 & disown
    if [[ -z ${cpu_index} ]]; then
      nohup ./${process_name} ${config_file} > /dev/null 2>&1 &
    else
      nohup taskset -c ${cpu_index} ./${process_name} ${config_file} > /dev/null 2>&1 &
    fi
    pid=$!
    echo ${pid} > ${pid_file}
    echo "process started with pid ${pid}."
    exit 0
}

function stop() {
    process_name=$1
    pid_file=$2

    status ${process_name} ${pid_file}
    s=$?
    if [[ $s != 1 ]]; then
        echo "process is not running."
        exit 0
    fi

    exists_pid=`cat ${pid_file}`
    kill -15 ${exists_pid}
    echo "process with pid ${exists_pid} has stopped."
}

function pause() {
    process_name=$1
    pid_file=$2

    status ${process_name} ${pid_file}
    s=$?
    if [[ $s != 1 ]]; then
        echo "process is not running."
        exit 0
    fi

    exists_pid=`cat ${pid_file}`
    kill -SIGUSR1 ${exists_pid}
    echo "process with pid ${exists_pid} has paused."
    echo "please wait at least 5 seconds if you want to stop the instance, for canceling orders may cost few seconds."
}

function resume() {
    process_name=$1
    pid_file=$2

    status ${process_name} ${pid_file}
    s=$?
    if [[ $s != 1 ]]; then
        echo "process is not running."
        exit 0
    fi

    exists_pid=`cat ${pid_file}`
    kill -SIGUSR2 ${exists_pid}
    echo "process with pid ${exists_pid} has resumed."
}

# goto project root
PROJECT_ROOT=$(cd `dirname $0`; cd ..; pwd;)

cd $PROJECT_ROOT

ACTION=$1
PROCESS_NAME=$2
PID_FILE=$3
CONFIG_FILE=$4
CPU_NO=$5

if [[ -z ${PROCESS_NAME} || -z ${ACTION} || -z ${PID_FILE} ]]; then
  echo "Usage Sample: bin/catalina.sh start test_main config.json"
  exit 0;
fi

if [[ ${ACTION} == "start" ]]; then
  if [[ -z ${CONFIG_FILE} ]]; then
    echo "should give config file, like 'config.json'"
    exit 0
  fi
  start_daemon ${PROCESS_NAME} ${PID_FILE} ${CONFIG_FILE} ${CPU_NO}
elif [[ ${ACTION} == "stop" ]]; then
  stop ${PROCESS_NAME} ${PID_FILE}
elif [[ ${ACTION} == "pause" ]]; then
  pause ${PROCESS_NAME} ${PID_FILE}
elif [[ ${ACTION} == "resume" ]]; then
  resume ${PROCESS_NAME} ${PID_FILE}
elif [[ ${ACTION} == "status" ]]; then
  status ${PROCESS_NAME} ${PID_FILE}
  s=$?
  if [[ $s == 1 ]];then
    echo "process is running."
  else
    echo "process had stopped."
  fi
fi
