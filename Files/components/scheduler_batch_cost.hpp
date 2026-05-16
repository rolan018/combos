#pragma once

#include "types.hpp"

/**
 * Scheduling-server batch budget: estimated "work units" (simulated CPU seconds) per result.
 * Central place to evolve policy (slow-host caps, per-WU flops, network-weighted cost, etc.).
 */
double single_result_add_work(const ProjectDatabaseValue &project, request_t req);

double balancer_per_result_work_cost(const ProjectDatabaseValue &project, request_t req,
                                     const AssignedResult *candidate);

/** Effective CPU-time budget for one scheduler reply (same units as per-result cost). Uses client shortfall
 *  (`percentage`) with a slow-host multiplier; when shortfall is below one task's CPU time, budget is capped at
 *  two tasks so shared current_results is not drained (large unconditional floors collapsed MIN busy). */
double balancer_request_budget(const ProjectDatabaseValue &project, request_t req);

/** True iff this reply should stop growing (after at least one result was already added). */
bool balancer_should_stop_batch(double sum_work, double next_result_cost, const ProjectDatabaseValue &project,
                                request_t req);