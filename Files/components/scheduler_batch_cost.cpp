#include "scheduler_batch_cost.hpp"

#include <algorithm>

double single_result_add_work(const ProjectDatabaseValue &project, request_t req)
{
    if (req == nullptr || req->power <= 0)
        return 0.0;
    return static_cast<double>(project.job_duration) / static_cast<double>(req->power);
}

double balancer_per_result_work_cost(const ProjectDatabaseValue &project, request_t req,
                                     const AssignedResult *candidate)
{
    const double cpu_seconds = single_result_add_work(project, req);
    if (candidate == nullptr)
        return cpu_seconds;
    const double bytes =
        static_cast<double>(std::max(0, candidate->ninput_files)) * static_cast<double>(project.input_file_size);
    const double bw = std::max(1.0, static_cast<double>(project.disk_bw));
    const double input_seconds = bytes / bw;
    return cpu_seconds + input_seconds;
}