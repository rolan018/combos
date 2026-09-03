#include "scheduler_balancer_state.hpp"

#include "shared.hpp"

#include <simgrid/s4u.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace sg4 = simgrid::s4u;

namespace {

void ema_update(double &ema, double sample, double alpha)
{
    if (ema <= 0)
        ema = sample;
    else
        ema = alpha * sample + (1.0 - alpha) * ema;
}

void record_gate_met_at(double &gate_met_at, bool now_ok, double sim_time)
{
    if (now_ok && gate_met_at < 0)
        gate_met_at = sim_time;
}

const char *steady_limiting_gate_name(double time_at, double cpu_at, double batches_at, double pipeline_at)
{
    struct Gate
    {
        const char *name;
        double at;
    };
    Gate gates[] = {{"time", time_at},
                    {"cpu_observations", cpu_at},
                    {"batches_sent", batches_at},
                    {"pipeline_results", pipeline_at}};
    const Gate *best = &gates[0];
    for (const Gate &gate : gates)
    {
        if (gate.at > best->at)
            best = &gate;
    }
    return best->at >= 0 ? best->name : "unknown";
}

void balancer_maybe_advance_phase_unlocked(ProjectDatabaseValue &project, double sim_time)
{
    BalancerState &state = project.balancer;
    if (state.phase != BalancerPhase::BOOTSTRAP)
        return;

    const bool time_ok = sim_time >= state.bootstrap_duration_sec;
    const bool cpu_ok = state.cpu_observations >= state.bootstrap_min_cpu_observations;
    const bool batches_ok = state.batches_sent >= state.bootstrap_min_batches;
    const bool pipeline_ok = project.ncurrent_results >= state.min_pipeline_results;

    record_gate_met_at(state.steady_time_gate_met_at, time_ok, sim_time);
    record_gate_met_at(state.steady_cpu_gate_met_at, cpu_ok, sim_time);
    record_gate_met_at(state.steady_batches_gate_met_at, batches_ok, sim_time);
    record_gate_met_at(state.steady_pipeline_gate_met_at, pipeline_ok, sim_time);

    if (time_ok && cpu_ok && batches_ok && pipeline_ok)
    {
        const char *limiting = steady_limiting_gate_name(state.steady_time_gate_met_at, state.steady_cpu_gate_met_at,
                                                         state.steady_batches_gate_met_at,
                                                         state.steady_pipeline_gate_met_at);
        std::strncpy(state.steady_limiting_gate, limiting, sizeof(state.steady_limiting_gate) - 1);
        state.steady_limiting_gate[sizeof(state.steady_limiting_gate) - 1] = '\0';

        state.phase = BalancerPhase::STEADY;
        state.steady_phase_started_at = sim_time;
    }
}

} // namespace

bool balancer_feedback_enabled()
{
    return g_balancer_feedback_enabled;
}

void balancer_state_init(BalancerState &state, double bootstrap_duration_hours_config,
                         double bootstrap_duration_hours_effective)
{
    state.phase = BalancerPhase::BOOTSTRAP;
    state.mutex = sg4::Mutex::create();
    state.batches_sent = 0;
    state.cpu_observations = 0;
    state.result_observations = 0;
    state.ema_cpu_sec = 0;
    state.ema_wall_turnaround_sec = 0;
    state.ema_batch_fill_ratio = 0;
    state.bootstrap_duration_config_hours = bootstrap_duration_hours_config;
    state.bootstrap_duration_effective_hours = bootstrap_duration_hours_effective;
    state.bootstrap_duration_sec = bootstrap_duration_hours_effective * 3600.0;
    state.steady_phase_started_at = -1;
    state.steady_time_gate_met_at = -1;
    state.steady_cpu_gate_met_at = -1;
    state.steady_batches_gate_met_at = -1;
    state.steady_pipeline_gate_met_at = -1;
    state.steady_limiting_gate[0] = '\0';

    if (!balancer_feedback_enabled())
        state.phase = BalancerPhase::STEADY;
}

bool balancer_is_bootstrap(const ProjectDatabaseValue &project)
{
    if (!balancer_feedback_enabled())
        return false;
    return project.balancer.phase == BalancerPhase::BOOTSTRAP;
}

bool balancer_has_cpu_estimate(const ProjectDatabaseValue &project)
{
    if (!balancer_feedback_enabled())
        return false;
    return project.balancer.cpu_observations > 0 && project.balancer.ema_cpu_sec > 0;
}

double balancer_estimated_cpu_sec(const ProjectDatabaseValue &project, request_t req)
{
    if (balancer_has_cpu_estimate(project))
        return project.balancer.ema_cpu_sec;
    if (req == nullptr || req->power <= 0)
        return 0.0;
    return static_cast<double>(project.job_duration) / static_cast<double>(req->power);
}

void balancer_record_batch(ProjectDatabaseValue &project, double budget, double sum_work, int batch_size)
{
    if (!balancer_feedback_enabled())
        return;

    BalancerState &state = project.balancer;
    std::lock_guard lock(*state.mutex);

    if (batch_size > 0)
        state.batches_sent++;

    if (budget > 0 && sum_work > 0)
        ema_update(state.ema_batch_fill_ratio, sum_work / budget, state.ema_alpha);

    balancer_maybe_advance_phase_unlocked(project, sg4::Engine::get_clock());
}

void balancer_record_cpu_sample(ProjectDatabaseValue &project, double cpu_seconds)
{
    if (!balancer_feedback_enabled() || cpu_seconds <= 0)
        return;

    BalancerState &state = project.balancer;
    std::lock_guard lock(*state.mutex);

    ema_update(state.ema_cpu_sec, cpu_seconds, state.ema_alpha);
    state.cpu_observations++;

    balancer_maybe_advance_phase_unlocked(project, sg4::Engine::get_clock());
}

void balancer_record_result_turnaround(ProjectDatabaseValue &project, double sent_time, double now)
{
    if (!balancer_feedback_enabled())
        return;

    const double turnaround = now - sent_time;
    if (turnaround <= 0)
        return;

    BalancerState &state = project.balancer;
    std::lock_guard lock(*state.mutex);

    ema_update(state.ema_wall_turnaround_sec, turnaround, state.ema_alpha);
    state.result_observations++;

    balancer_maybe_advance_phase_unlocked(project, now);
}

void balancer_maybe_advance_phase(ProjectDatabaseValue &project, double sim_time)
{
    if (!balancer_feedback_enabled())
        return;
    std::lock_guard lock(*project.balancer.mutex);
    balancer_maybe_advance_phase_unlocked(project, sim_time);
}
