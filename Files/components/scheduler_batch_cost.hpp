#pragma once

#include "types.hpp"

/**
 * Scheduling-server batch budget: estimated "work units" (simulated CPU seconds) per result.
 * Central place to evolve policy (slow-host caps, per-WU flops, network-weighted cost, etc.).
 */
double single_result_add_work(const ProjectDatabaseValue &project, request_t req);

double balancer_per_result_work_cost(const ProjectDatabaseValue &project, request_t req,
                                     const AssignedResult *candidate);