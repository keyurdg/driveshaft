#!/bin/bash

# Set vars from env or defaults
pushd $(dirname $0) >/dev/null 2>&1
this_dir=$(pwd)
popd > /dev/null
driveshaft_home=$(dirname $(dirname $this_dir))
gearmand_bin=${GEARMAND_BIN:-gearmand}
driveshaft_bin=${DRIVESHAFT_BIN:-$driveshaft_home/src/driveshaft}

# Do sanity checks
echo "Doing sanity checks..."
die() { echo "$@" >&2 ; exit 1; }
[ -f $gearmand_bin ] || die "$gearmand_bin is not a file; override with GEARMAND_BIN"
[ -f $driveshaft_bin ] || die "$driveshaft_bin is not a file; override with DRIVESHAFT_BIN"
which nc >/dev/null 2>&1 || die "nc not in PATH"
which php >/dev/null 2>&1 || die "php not in PATH"
( php -i | grep 'gearman.*enabled' ) >/dev/null 2>&1 || die "gearman php extension does not appear to be installed"

# Start gearmand
echo "Starting gearmand..."
gearmand_log=$(mktemp)
$gearmand_bin --port 47301 --verbose INFO --log-file $gearmand_log &
gearmand_pid=$!
sleep 1

# Write driveshaft config
echo "Writing driveshaft config..."
read -r -d '' driveshaft_config <<'EOD'
{
    "gearman_server_timeout": 1000,
    "gearman_servers_list": [
        "localhost:47301"
    ],
    "pools_list": {
        "Regular": {
            "job_processing_uri": "http://localhost:47302/",
            "worker_count": 10,
            "jobs_list": [
                "GetTime"
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
    'response_string' => strval(time()),
]);
EOD
work_script_path=$(mktemp)
echo $work_script > $work_script_path

# Start httpd
echo "Starting httpd (php -S)..."
php -S localhost:47302 $work_script_path &
httpd_pid=$!
sleep 1

# Queue job
echo "Queuing job..."
read -r -d '' php_script <<'EOD'
    $exit_code = 1;
    $client = new GearmanClient();
    $client->addServer('localhost', 47301);
    $client->setCompleteCallback(function($task) use (&$exit_code) {
        echo "setCompleteCallback: {$task->data()}\n";
        $exit_code = 0;
    });
    if (!$client->addTask('GetTime', serialize(''))) {
        die("addTask failed\n");
    }
    $client->runTasks();
    exit($exit_code);
EOD
timeout 5 php -r "$php_script"
exit_code=$?
if [ $exit_code -eq 0 ]; then
    echo -e "\e[32mSuccess! :)\e[0m"
else
    echo -e "\e[31mFailed :(\e[0m"
    cat $gearmand_log
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
