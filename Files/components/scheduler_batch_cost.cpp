#include "scheduler_batch_cost.hpp"

#include "scheduler_group_aware.hpp"

#include <algorithm>

namespace {

/** ~10 GFlop/s reference; same order of magnitude as client power in parameters.xml (8–10 GFLOPS). */
constexpr double kReferenceHostFlopsPerSec = 10e9;
/** Max extra budget factor for slow CPUs vs reference (caps runaway batch sizes). */
constexpr double kMaxBudgetBoost = 1.5;
/** When client shortfall (`percentage`) is below one task's CPU time, allow at most this many tasks'
 *  worth of budget — avoids draining current_results (which collapsed MIN busy when set to 3+). */
constexpr double kMaxBatchCpuWhenShortfallTooSmall = 2.0;

double host_batch_budget_multiplier(request_t req)
{
    if (req == nullptr || req->power <= 0)
        return 1.0;
    const double p = static_cast<double>(req->power);
    const double m = kReferenceHostFlopsPerSec / p;
    return std::min(kMaxBudgetBoost, std::max(1.0, m));
}

} // namespace

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
    return cpu_seconds + input_seconds + group_aware_mismatch_penalty(project, req);
}

double balancer_request_budget(const ProjectDatabaseValue &project, request_t req)
{
    if (req == nullptr)
        return 0.0;
    const double mult = host_batch_budget_multiplier(req);
    const double cpu_one = single_result_add_work(project, req);
    const double from_client = req->percentage * mult;

    /* Normal case: client already asks for >= one CPU-bound task — respect budget (no multi-task drain spike). */
    if (from_client >= cpu_one)
        return from_client;

    /* Shortfall is smaller than one task's CPU time: allow at most two tasks' worth (capped) so we do not drain
     * current_results the way an unconditional 3× floor did (MIN busy dropped ~69% → 36%). */
    return kMaxBatchCpuWhenShortfallTooSmall * cpu_one + 1e-6;
}

bool balancer_should_stop_batch(double sum_work, double next_result_cost, const ProjectDatabaseValue &project,
                                request_t req)
{
    const double budget = balancer_request_budget(project, req);
    return sum_work > 0 && sum_work + next_result_cost >= budget;
}