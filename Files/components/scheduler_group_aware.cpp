#include "scheduler_group_aware.hpp"

#include "../parameters_struct_from_yaml.hpp"
#include "shared.hpp"

#include <algorithm>
#include <cmath>
#include <cinttypes>
#include <iostream>
#include <limits>

bool g_group_aware_matching_enabled = false;

GroupAwareMatchingStats g_group_aware_stats;

namespace {

int g_fast_client_group_id = 1;
int g_slow_client_group_id = 2;
int g_short_project_number = 0;
int g_long_project_number = 1;
bool g_tiers_configured = false;

/** Penalty multipliers on top of native CPU seconds for mismatched tier pairs. */
constexpr double kLongOnFastPenaltyMult = 100.0;
constexpr double kShortOnSlowPenaltyMult = 50.0;

int host_group_id_from_request(request_t req)
{
    if (req == nullptr || req->host_name.size() < 2 || req->host_name[0] != 'c')
        return -1;
    return get_client_group(req->host_name);
}

int project_index(const ProjectDatabaseValue &project)
{
    return static_cast<int>(static_cast<unsigned char>(project.project_number));
}

double base_cpu_seconds(const ProjectDatabaseValue &project, request_t req)
{
    if (req == nullptr || req->power <= 0)
        return 0.0;
    return static_cast<double>(project.job_duration) / static_cast<double>(req->power);
}

} // namespace

void group_aware_matching_init(const parameters::Config &config)
{
    g_group_aware_matching_enabled = config.experiment_run.balancer.group_aware_matching_enabled;
    g_tiers_configured = false;

    if (!g_group_aware_matching_enabled)
        return;

    const auto &groups = config.client_side.groups;
    if (groups.size() < 2 || config.server_side.sprojects.size() < 2)
    {
        std::cerr << "Group-aware matching: need >=2 groups and >=2 projects; disabled.\n";
        g_group_aware_matching_enabled = false;
        return;
    }

    size_t fast_idx = 0;
    size_t slow_idx = 0;
    double best_avg = -1.0;
    double worst_avg = std::numeric_limits<double>::infinity();
    for (size_t i = 0; i < groups.size(); ++i)
    {
        const double avg = (groups[i].max_speed + groups[i].min_speed) / 2.0;
        if (avg > best_avg)
        {
            best_avg = avg;
            fast_idx = i;
        }
        if (avg < worst_avg)
        {
            worst_avg = avg;
            slow_idx = i;
        }
    }

    size_t short_idx = 0;
    size_t long_idx = 0;
    int64_t min_fpops = std::numeric_limits<int64_t>::max();
    int64_t max_fpops = 0;
    for (size_t i = 0; i < config.server_side.sprojects.size(); ++i)
    {
        const int64_t fpops = config.server_side.sprojects[i].task_fpops;
        if (fpops < min_fpops)
        {
            min_fpops = fpops;
            short_idx = i;
        }
        if (fpops > max_fpops)
        {
            max_fpops = fpops;
            long_idx = i;
        }
    }

    g_fast_client_group_id = static_cast<int>(fast_idx) + 1;
    g_slow_client_group_id = static_cast<int>(slow_idx) + 1;
    g_short_project_number = static_cast<int>(short_idx);
    g_long_project_number = static_cast<int>(long_idx);
    g_tiers_configured = true;

    std::cout << "Group-aware matching: ENABLED soft+fallback (penalty 100x/50x, max "
              << kGroupAwareMaxFallbackPerReply << " fallback/reply, cost-based pick)\n";
}

void group_aware_stats_reset()
{
    g_group_aware_stats = {};
}

void group_aware_stats_print()
{
    if (!g_group_aware_matching_enabled)
        return;

    printf("  Group-aware preferred assigns: \t%'" PRId64 "\n", g_group_aware_stats.preferred_assignments);
    printf("  Group-aware fallback assigns: \t%'" PRId64 "\n", g_group_aware_stats.fallback_assignments);
    printf("  Group-aware preferred skipped: \t%'" PRId64 "\n\n", g_group_aware_stats.preferred_skipped);
}

bool group_aware_is_preferred_placement(const ProjectDatabaseValue &project, request_t req)
{
    if (!g_group_aware_matching_enabled || !g_tiers_configured || req == nullptr)
        return true;

    const int host_group = host_group_id_from_request(req);
    if (host_group < 0)
        return true;

    const int pnum = project_index(project);
    if (pnum == g_short_project_number)
        return host_group == g_fast_client_group_id;
    if (pnum == g_long_project_number)
        return host_group == g_slow_client_group_id;
    return true;
}

double group_aware_mismatch_penalty(const ProjectDatabaseValue &project, request_t req)
{
    if (!g_group_aware_matching_enabled || !g_tiers_configured || req == nullptr)
        return 0.0;
    if (group_aware_is_preferred_placement(project, req))
        return 0.0;

    const int pnum = project_index(project);
    const int host_group = host_group_id_from_request(req);
    const double cpu = base_cpu_seconds(project, req);

    if (pnum == g_long_project_number && host_group == g_fast_client_group_id)
        return cpu * kLongOnFastPenaltyMult;
    if (pnum == g_short_project_number && host_group == g_slow_client_group_id)
        return cpu * kShortOnSlowPenaltyMult;
    return cpu * 10.0;
}

void group_aware_record_assignment(const ProjectDatabaseValue &project, request_t req, bool fallback)
{
    if (!g_group_aware_matching_enabled)
        return;
    (void)project;
    (void)req;
    if (fallback)
        g_group_aware_stats.fallback_assignments++;
    else
        g_group_aware_stats.preferred_assignments++;
}

void group_aware_record_preferred_skipped()
{
    if (g_group_aware_matching_enabled)
        g_group_aware_stats.preferred_skipped++;
}
