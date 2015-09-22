# Driveshaft - Gearman Worker Manager

# Getting Started

# Install
## Dependencies
* cmake 2.8.7 or later
* libgearman-devel
* log4cxx-devel
* boost-devel 1.5.3 or later
* gcc 4.8 or later

## Build
```
$ cmake .
$ make
```

## Install
```
$ sudo make install
```

# Configuration
gearshift takes two arguments:
```
$ driveshaft
Allowed options:
  --help                produce help message
  --jobsconfig arg      jobs config file path
  --logconfig arg       log config file path
```

## jobsconfig
A simple jobsconfig file looks like this:
```json
{
    "http_uri": "http://localhost/job.php",
    "server_timeout": 5000,
    "worker_count": 8,
    "servers_list":
    [
        "localhost"
    ],
    "pools_list":
    {
        "ShopStats":
        {
            "worker_count": 0,
            "jobs_list":
            [
                "ShopStats"
            ]
        },
        "Newsfeed":
        {
            "worker_count": 0,
            "jobs_list":
            [
                "Newsfeed"
            ]
        },
        "Regular":
        {
            "worker_count": 0,
            "jobs_list":
            [
                "Sum3",
                "Sum",
                "Sum2"
            ]
        }
    }
}
```

### Jobs Config Options
* `http_uri` - the uri to send the job payload to for execution
* `server_timeout` - timeout in milliseconds to wait for `http_uri` to respond
* `worker_count` - the total # of workers to register with gearmand
* `gearman_servers_list` - addresses of gearmand servers
* `pools list` - a list of named pools and corresponding configuration for every pool:
    * `worker_count` - Number of workers to reserve for jobs in this pool
    * `jobs_list` - Names of jobs that should be ran on the workers in this pool

## logconfig
An [example log config is
included](https://github.com/keyurdg/driveshaft/blob/master/logconfig.xml) in the repository. For more information, see
[the log4cxx documentation](https://logging.apache.org/log4cxx/usage.html).

# Design
1. Driveshaft uses a config file to know what jobs are runnable.
2. Jobs are grouped into pools and every pool has a max-workers setting in order
to support independent workers. There's a regular workers pool for the normal
jobs.
3. For every pool, it registers with GearmanD and maintains N threads with
persistent connections to fetch jobs and submit back results where N is the # of
workers configured for the pool.
4. When the config changes, it signals the appropriate pool threads to die. Any
currently running job on that thread has a 24 hour window to finish, otherwise
the job is considered failed and the thread is closed. New pool threads are
created as needed to match the configuration.
5. Jobs are run via an HTTP endpoint defined in the config. The endpoint will
receive the class name and all the args and will have to do the right thing and
return SUCCESS/FAILURE along with any response text. The thread that is
processing the job blocks waiting for a response.

By reusing connections and not re-registering potentially hundreds of jobs on
worker restart this will save GearmanD a lot of work that impacts latency.

By using an HTTP endpoint to actually do the heavy lifting, we will get the
benefits of a clean-sandbox and Opcache (and can even use HHVM!).
