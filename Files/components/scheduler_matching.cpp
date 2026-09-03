#include "scheduler_matching.hpp"

#include "scheduler_batch_cost.hpp"
#include "scheduler_balancer_state.hpp"
#include "scheduler_group_aware.hpp"
#include "shared.hpp"

#include <cinttypes>
#include <limits>
#include <list>
#include <set>

namespace {

bool scheduling_result_locally_accessible(const ProjectDatabaseValue &project, const AssignedResult *result,
                                          request_t req)
{
    if (result == nullptr || req == nullptr || result->workunit == nullptr)
        return false;
    (void)project;
    for (const auto &input_file_holder : result->workunit->input_files)
    {
        if (the_same_client_group(input_file_holder, req->host_name))
            return true;
    }
    return false;
}

bool scheduling_tier_pick_allowed(const ProjectDatabaseValue &project, request_t req, bool allow_fallback_phase,
                                    int fallback_in_bag)
{
    if (!g_group_aware_matching_enabled)
        return true;
    if (group_aware_is_preferred_placement(project, req))
        return true;
    if (!allow_fallback_phase)
    {
        group_aware_record_preferred_skipped();
        return false;
    }
    if (fallback_in_bag >= kGroupAwareMaxFallbackPerReply)
    {
        group_aware_record_preferred_skipped();
        return false;
    }
    return true;
}

bool scheduling_result_accessible(const ProjectDatabaseValue &project, const AssignedResult *result, request_t req,
                                  bool allow_fallback_phase, int fallback_in_bag)
{
    return scheduling_result_locally_accessible(project, result, req) &&
           scheduling_tier_pick_allowed(project, req, allow_fallback_phase, fallback_in_bag);
}

AssignedResult *scheduling_pick_lowest_cost(const ProjectDatabaseValue &project, request_t req,
                                            const std::set<std::string> &unique_workunits,
                                            const std::list<AssignedResult *> &candidates, bool allow_fallback_phase,
                                            int fallback_in_bag)
{
    AssignedResult *best = nullptr;
    double best_cost = std::numeric_limits<double>::infinity();

    for (AssignedResult *cand : candidates)
    {
        if (!scheduling_result_accessible(project, cand, req, allow_fallback_phase, fallback_in_bag))
            continue;
        if (unique_workunits.count(cand->workunit->number) != 0)
            continue;

        const double cost = balancer_per_result_work_cost(project, req, cand);
        if (cost < best_cost)
        {
            best_cost = cost;
            best = cand;
        }
    }
    return best;
}

void scheduling_remove_from_pipeline(ProjectDatabaseValue &project, AssignedResult *result)
{
    project.current_results.remove(result);
    project.ncurrent_results--;
    if (project.ncurrent_results == 0)
        project.wg_full->notify_all();
}

void scheduling_materialize_task(ProjectDatabaseValue &project, request_t req, AssignedResult *result, bool fallback)
{
    TaskT *task = new TaskT();
    task->workunit = result->workunit->number;
    task->name = std::string(bprintf("%" PRId32, result->workunit->nsent_results++));
    task->duration = project.job_duration * ((double)req->group_power / req->power);
    task->deadline = project.delay_bound;
    task->sent_time = simgrid::s4u::Engine::get_clock();
    result->corresponding_tasks = task;
    result->workunit->times[result->number] = task->sent_time;

    project.ssdmutex->lock();
    project.nresults_sent++;
    project.ssdmutex->unlock();
    group_aware_record_assignment(project, req, fallback);
}

AssignedResult *scheduling_pick_fifo_head(const ProjectDatabaseValue &project, request_t req,
                                           const std::set<std::string> &unique_workunits,
                                           const std::list<AssignedResult *> &candidates, bool allow_fallback_phase,
                                           int fallback_in_bag)
{
    if (g_group_aware_matching_enabled)
        return scheduling_pick_lowest_cost(project, req, unique_workunits, candidates, allow_fallback_phase,
                                           fallback_in_bag);

    for (AssignedResult *cand : candidates)
    {
        if (!scheduling_result_accessible(project, cand, req, true, fallback_in_bag))
            continue;
        if (unique_workunits.count(cand->workunit->number) != 0)
            continue;
        return cand;
    }
    return nullptr;
}

AssignedResult *scheduling_pick_mct_result(const ProjectDatabaseValue &project, request_t req,
                                           const std::set<std::string> &unique_workunits,
                                           const std::list<AssignedResult *> &candidates, bool allow_fallback_phase,
                                           int fallback_in_bag)
{
    AssignedResult *best =
        scheduling_pick_lowest_cost(project, req, unique_workunits, candidates, allow_fallback_phase, fallback_in_bag);

    if (best != nullptr)
    {
        AssignedResult *fifo =
            scheduling_pick_fifo_head(project, req, unique_workunits, candidates, allow_fallback_phase, fallback_in_bag);
        g_scheduler_matching_stats.mct_picks++;
        if (fifo != nullptr && fifo != best)
            g_scheduler_matching_stats.mct_differs_from_fifo++;
    }
    return best;
}

struct ClientScheduleState
{
    request_t req = nullptr;
    std::vector<AssignedResult *> bag;
    double sum_work = 0;
    double virtual_load = 0;
    std::set<std::string> unique_workunits;
    bool allow_fallback = false;
    int fallback_in_bag = 0;
};

} // namespace

std::vector<AssignedResult *> scheduling_select_results_mct(int project_number, request_t req)
{
    ProjectDatabaseValue &project = SharedDatabase::_pdatabase[project_number];
    std::vector<AssignedResult *> bag;
    if (req == nullptr)
        return bag;

    double sum_work = 0;
    std::set<std::string> unique_workunits;
    std::list<AssignedResult *> available(project.current_results.begin(), project.current_results.end());
    bool allow_fallback = false;
    int fallback_in_bag = 0;

    while (!available.empty())
    {
        AssignedResult *result =
            scheduling_pick_mct_result(project, req, unique_workunits, available, allow_fallback, fallback_in_bag);
        if (result == nullptr)
        {
            if (!allow_fallback && g_group_aware_matching_enabled)
            {
                allow_fallback = true;
                continue;
            }
            break;
        }

        const bool fallback = allow_fallback && !group_aware_is_preferred_placement(project, req);
        const double cost = balancer_per_result_work_cost(project, req, result);
        if (balancer_should_stop_batch(sum_work, cost, project, req))
            break;

        available.remove(result);
        scheduling_remove_from_pipeline(project, result);
        sum_work += cost;
        scheduling_materialize_task(project, req, result, fallback);
        unique_workunits.insert(result->workunit->number);
        bag.push_back(result);
        if (fallback)
            fallback_in_bag++;
    }

    if (bag.empty() && project.ncurrent_results == 0)
        project.wg_full->notify_all();

    const double budget = balancer_request_budget(project, req);
    balancer_record_batch(project, budget, sum_work, static_cast<int>(bag.size()));
    return bag;
}

std::vector<std::vector<AssignedResult *>> scheduling_min_min_batch(int project_number,
                                                                      const std::vector<request_t> &requests)
{
    const size_t n = requests.size();
    std::vector<std::vector<AssignedResult *>> out(n);
    if (n == 0)
        return out;

    if (n == 1)
    {
        out[0] = scheduling_select_results_mct(project_number, requests[0]);
        return out;
    }

    g_scheduler_matching_stats.minmin_multi_client_batches++;

    ProjectDatabaseValue &project = SharedDatabase::_pdatabase[project_number];
    std::vector<ClientScheduleState> clients(n);
    for (size_t i = 0; i < n; ++i)
        clients[i].req = requests[i];

    std::list<AssignedResult *> available(project.current_results.begin(), project.current_results.end());

    while (!available.empty())
    {
        int best_client = -1;
        AssignedResult *best_result = nullptr;
        double best_completion = std::numeric_limits<double>::infinity();
        bool best_fallback = false;

        for (size_t i = 0; i < n; ++i)
        {
            ClientScheduleState &client = clients[i];
            for (AssignedResult *cand : available)
            {
                if (!scheduling_result_accessible(project, cand, client.req, client.allow_fallback,
                                                  client.fallback_in_bag))
                    continue;
                if (client.unique_workunits.count(cand->workunit->number) != 0)
                    continue;

                const double cost = balancer_per_result_work_cost(project, client.req, cand);
                if (balancer_should_stop_batch(client.sum_work, cost, project, client.req))
                    continue;

                const double completion = client.virtual_load + cost;
                if (completion < best_completion)
                {
                    best_completion = completion;
                    best_client = static_cast<int>(i);
                    best_result = cand;
                    best_fallback = client.allow_fallback;
                }
            }
        }

        if (best_client < 0 || best_result == nullptr)
        {
            bool any_promoted = false;
            for (size_t i = 0; i < n; ++i)
            {
                if (!clients[i].allow_fallback && g_group_aware_matching_enabled)
                {
                    clients[i].allow_fallback = true;
                    any_promoted = true;
                }
            }
            if (any_promoted)
                continue;
            break;
        }

        ClientScheduleState &winner = clients[static_cast<size_t>(best_client)];
        const bool fallback = best_fallback && !group_aware_is_preferred_placement(project, winner.req);
        const double cost = balancer_per_result_work_cost(project, winner.req, best_result);

        available.remove(best_result);
        scheduling_remove_from_pipeline(project, best_result);
        winner.sum_work += cost;
        winner.virtual_load += cost;
        scheduling_materialize_task(project, winner.req, best_result, fallback);
        winner.unique_workunits.insert(best_result->workunit->number);
        winner.bag.push_back(best_result);
        if (fallback)
            winner.fallback_in_bag++;
        g_scheduler_matching_stats.minmin_assignments++;
    }

    for (size_t i = 0; i < n; ++i)
    {
        const double budget = balancer_request_budget(project, clients[i].req);
        balancer_record_batch(project, budget, clients[i].sum_work, static_cast<int>(clients[i].bag.size()));
        out[i] = std::move(clients[i].bag);
    }

    return out;
}

std::vector<AssignedResult *> scheduling_select_results_fifo(int project_number, request_t req)
{
    ProjectDatabaseValue &project = SharedDatabase::_pdatabase[project_number];
    std::vector<AssignedResult *> bag;
    if (req == nullptr)
        return bag;

    double sum_work = 0;
    std::set<std::string> unique_workunits;
    bool allow_fallback = false;
    int fallback_in_bag = 0;
    auto current_results_it = project.current_results.begin();

    while (true)
    {
        AssignedResult *result = nullptr;

        if (g_group_aware_matching_enabled)
        {
            double best_cost = std::numeric_limits<double>::infinity();
            for (auto it = project.current_results.begin(); it != project.current_results.end(); ++it)
            {
                if (!scheduling_result_accessible(project, *it, req, allow_fallback, fallback_in_bag))
                    continue;
                if (unique_workunits.count((*it)->workunit->number) != 0)
                    continue;
                const double cost = balancer_per_result_work_cost(project, req, *it);
                if (cost < best_cost)
                {
                    best_cost = cost;
                    result = *it;
                    current_results_it = it;
                }
            }
        }
        else
        {
            for (; current_results_it != project.current_results.end(); ++current_results_it)
            {
                if (!scheduling_result_accessible(project, *current_results_it, req, true, fallback_in_bag))
                    continue;
                if (unique_workunits.count((*current_results_it)->workunit->number) == 0)
                {
                    result = *current_results_it;
                    break;
                }
            }
        }

        if (result == nullptr)
        {
            if (!allow_fallback && g_group_aware_matching_enabled)
            {
                allow_fallback = true;
                current_results_it = project.current_results.begin();
                continue;
            }
            if (project.ncurrent_results == 0)
                project.wg_full->notify_all();
            break;
        }

        const bool fallback = allow_fallback && !group_aware_is_preferred_placement(project, req);
        const double cost = balancer_per_result_work_cost(project, req, result);
        if (balancer_should_stop_batch(sum_work, cost, project, req))
            break;

        auto next_it = current_results_it;
        ++next_it;
        project.current_results.erase(current_results_it);
        current_results_it = next_it;

        project.ncurrent_results--;
        if (project.ncurrent_results == 0)
            project.wg_full->notify_all();

        sum_work += cost;
        scheduling_materialize_task(project, req, result, fallback);
        unique_workunits.insert(result->workunit->number);
        bag.push_back(result);
        if (fallback)
            fallback_in_bag++;
    }

    const double budget = balancer_request_budget(project, req);
    balancer_record_batch(project, budget, sum_work, static_cast<int>(bag.size()));
    return bag;
}

std::vector<std::vector<AssignedResult *>> scheduling_assign_work_batch(int project_number,
                                                                         const std::vector<request_t> &requests)
{
    const size_t n = requests.size();
    std::vector<std::vector<AssignedResult *>> out(n);
    if (n == 0)
        return out;

    if (g_min_min_scheduler_enabled)
        return scheduling_min_min_batch(project_number, requests);

    for (size_t i = 0; i < n; ++i)
        out[i] = scheduling_select_results_fifo(project_number, requests[i]);
    return out;
}
