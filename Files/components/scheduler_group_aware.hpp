#pragma once

#include "types.hpp"

namespace parameters
{
struct Config;
}

/** Prefer ShortJob on fast tier, LongJob on slow tier; soft penalty + fallback when idle. */
extern bool g_group_aware_matching_enabled;

struct GroupAwareMatchingStats
{
    int64_t preferred_assignments = 0;
    int64_t fallback_assignments = 0;
    int64_t preferred_skipped = 0;
};

extern GroupAwareMatchingStats g_group_aware_stats;

/** Max tier-mismatch tasks per one client reply when fallback is active. */
constexpr int kGroupAwareMaxFallbackPerReply = 1;

void group_aware_matching_init(const parameters::Config &config);

void group_aware_stats_reset();

void group_aware_stats_print();

/** Ideal tier pairing (Short->fast group, Long->slow group). */
bool group_aware_is_preferred_placement(const ProjectDatabaseValue &project, request_t req);

/** Extra simulated CPU seconds added to work cost for non-preferred pairings. */
double group_aware_mismatch_penalty(const ProjectDatabaseValue &project, request_t req);

void group_aware_record_assignment(const ProjectDatabaseValue &project, request_t req, bool fallback);

void group_aware_record_preferred_skipped();
