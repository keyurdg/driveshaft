#!/bin/bash

# This is an end-to-end test of driveshaft, gearmand, and the gearman extension
# for PHP. It implements a simple 'EchoWorkload' job which echoes back whatever
# workload it receives.
#
# The script starts gearmand, driveshaft, and a web server (php -S) which hosts
# the EchoWorkload code. A job is queued via GearmanClient in PHP, and the
# result data is verified to match the workload.
#

# Set vars from env or defaults
pushd $(dirname $0) >/dev/null 2>&1
this_dir=$(pwd)
popd > /dev/null
driveshaft_home=$(dirname $this_dir)
gearmand_bin=${GEARMAND_BIN:-gearmand}
driveshaft_bin=${DRIVESHAFT_BIN:-$driveshaft_home/src/driveshaft}
gearmand_port=47301
httpd_port=47302

# Do sanity checks
echo "Doing sanity checks..."
die() { echo "$@" >&2 ; exit 1; }
which $gearmand_bin >/dev/null 2>&1 || die "$gearmand_bin is not in PATH; override with GEARMAND_BIN"
which $driveshaft_bin >/dev/null 2>&1 || die "$driveshaft_bin is not in PATH; override with DRIVESHAFT_BIN"
which nc >/dev/null 2>&1 || die "nc not in PATH"
which php >/dev/null 2>&1 || die "php not in PATH"
( php -i | grep 'gearman.*enabled' ) >/dev/null 2>&1 || die "gearman php extension does not appear to be installed"

# Start gearmand
echo "Starting gearmand..."
gearmand_log=$(mktemp)
$gearmand_bin --port $gearmand_port --verbose INFO --log-file $gearmand_log &
gearmand_pid=$!
sleep 1

# Write driveshaft config
echo "Writing driveshaft config..."
read -r -d '' driveshaft_config <<EOD
{
    "gearman_server_timeout": 1000,
    "gearman_servers_list": [
        "localhost:$gearmand_port"
    ],
    "pools_list": {
        "Regular": {
            "job_processing_uri": "http://localhost:$httpd_port/",
            "worker_count": 10,
            "jobs_list": [
                "EchoWorkload"
            ]
        }
    }
}
EOD
driveshaft_config_path=$(mktemp)
echo $driveshaft_config > $driveshaft_config_path

# Start driveshaft
echo "Starting driveshaft..."
$driveshaft_bin --jobsconfig $driveshaft_config_path --logconfig "$driveshaft_home/logconfig.xml" &
driveshaft_pid=$!
sleep 1

# Write work script
echo "Writing work script..."
read -r -d '' work_script <<'EOD'
<?php
echo json_encode([
    'gearman_ret' => 0,
    'response_string' => unserialize($_POST['workload']),
]);
EOD
work_script_path=$(mktemp)
echo $work_script > $work_script_path

# Start httpd
echo "Starting httpd (php -S)..."
php -S localhost:$httpd_port $work_script_path &
httpd_pid=$!
sleep 1

# Queue job
echo "Queuing job..."
read -r -d '' php_script <<'EOD'
    $exit_code = 1;
    $workload = 'hello42';
    $client = new GearmanClient();
    $client->addServer('localhost', (int)$argv[1]);
    $client->setCompleteCallback(function($task) use (&$exit_code, $workload) {
        echo "CompleteCallback: {$task->data()}\n";
        if ($workload === $task->data()) {
            $exit_code = 0;
        }
    });
    if (!$client->addTask('EchoWorkload', serialize($workload))) {
        die("addTask failed\n");
    }
    $client->runTasks();
    exit($exit_code);
EOD
timeout 5 php -r "$php_script" $gearmand_port
exit_code=$?
if [ $exit_code -eq 0 ]; then
    echo -e "\e[32mSuccess! :)\e[0m"
else
    cat $gearmand_log
    echo -e "\e[31mFailed :(\e[0m"
fi

# Cleanup
echo "Cleaning up..."
rm -f $driveshaft_config_path
rm -f $work_script_path
rm -f $gearmand_log
kill -9 $gearmand_pid >/dev/null 2>&1
kill -9 $driveshaft_pid >/dev/null 2>&1
kill -9 $httpd_pid >/dev/null 2>&1

# Exit
exit $exit_code
