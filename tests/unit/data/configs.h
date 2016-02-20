#include <string>

const std::string testConfigOneServerOnePool(
    "{"
     "\"gearman_servers_list\": [\"foo\"],"
     "\"pools_list\": {"
          "\"test-pool-1\": {"
            "\"worker_count\": 5,"
            "\"jobs_list\": [\"Sum\"],"
            "\"job_processing_uri\": \"send.work.here\""
            "}"
        "}"
     "}"
);

const std::string testConfigOneServerOnePoolNewUri(
    "{\"gearman_servers_list\": [\"foo\"],"
     "\"pools_list\": {"
          "\"test-pool-1\": {"
            "\"worker_count\": 5,"
            "\"jobs_list\": [\"Sum\"],"
            "\"job_processing_uri\": \"now.send.here\""
            "}"
        "}"
     "}"
);

const std::string testConfigOneServerTwoPools(
    "{\"gearman_servers_list\": [\"foo\"],"
     "\"pools_list\": {"
          "\"test-pool-1\": {"
            "\"worker_count\": 5,"
            "\"jobs_list\": [\"Sum\"],"
            "\"job_processing_uri\": \"send.work.here\""
            "},"
          "\"test-pool-2\": {"
            "\"worker_count\": 5,"
            "\"jobs_list\": [\"Product\"],"
            "\"job_processing_uri\": \"send.work.here\""
            "}"
        "}"
     "}"
);

const std::string testConfigTwoServersTwoPools(
    "{\"gearman_servers_list\": [\"foo\", \"bar\"],"
     "\"pools_list\": {"
          "\"test-pool-1\": {"
            "\"worker_count\": 5,"
            "\"jobs_list\": [\"Sum\"],"
            "\"job_processing_uri\": \"send.work.here\""
            "},"
          "\"test-pool-2\": {"
            "\"worker_count\": 5,"
            "\"jobs_list\": [\"Product\"],"
            "\"job_processing_uri\": \"send.work.here\""
            "}"
        "}"
     "}"
);
