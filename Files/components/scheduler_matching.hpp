#pragma once

#include "types.hpp"

#include <vector>

/** Level 1: MCT — fill one client reply by repeatedly picking the minimum-ECT result. */
std::vector<AssignedResult *> scheduling_select_results_mct(int project_number, request_t req);

/** Legacy FIFO — first accessible result in queue order. */
std::vector<AssignedResult *> scheduling_select_results_fifo(int project_number, request_t req);

/** Level 2: Min-Min across concurrent client requests (one client → MCT). */
std::vector<std::vector<AssignedResult *>> scheduling_min_min_batch(int project_number,
                                                                      const std::vector<request_t> &requests);

/** Dispatch batch using Min-Min/MCT or FIFO depending on g_min_min_scheduler_enabled. */
std::vector<std::vector<AssignedResult *>> scheduling_assign_work_batch(int project_number,
                                                                         const std::vector<request_t> &requests);
