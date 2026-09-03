#pragma once

#include "types.hpp"

bool balancer_feedback_enabled();

void balancer_state_init(BalancerState &state, double bootstrap_duration_hours_config,
                         double bootstrap_duration_hours_effective);

bool balancer_is_bootstrap(const ProjectDatabaseValue &project);

bool balancer_has_cpu_estimate(const ProjectDatabaseValue &project);

/** Learned mean CPU seconds per task (falls back to static estimate when unknown). */
double balancer_estimated_cpu_sec(const ProjectDatabaseValue &project, request_t req);

void balancer_record_batch(ProjectDatabaseValue &project, double budget, double sum_work, int batch_size);

void balancer_record_cpu_sample(ProjectDatabaseValue &project, double cpu_seconds);

void balancer_record_result_turnaround(ProjectDatabaseValue &project, double sent_time, double now);

void balancer_maybe_advance_phase(ProjectDatabaseValue &project, double sim_time);
