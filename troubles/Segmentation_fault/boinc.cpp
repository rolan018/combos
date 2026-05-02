
/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

/* BOINC architecture simulator */

#include <iostream>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <string>
#include <xbt/utility.hpp>
#include <set>
#include <queue>
#include <stdio.h>
#include <climits>
#include <csignal>
#include <stdexcept>
#include <locale.h> // Big numbers nice output
#include <cmath>
#include <math.h>
#include <unistd.h>
#include <inttypes.h>
#include <simgrid/cond.h>
#include <simgrid/engine.h>
#include <simgrid/s4u.hpp>
#include <boost/intrusive/list.hpp>
#include <boost/random/linear_congruential.hpp>

#include "components/types.hpp"
#include "components/shared.hpp"
#include "components/scheduler.hpp"
#include "tools/execution_state.hpp"
#include "client_side/data_client.hpp"
#include "client_side/fetch_work.hpp"
#include "client_side/execute_task.hpp"
#include "parameters_struct_from_yaml.hpp"

/* Create a log channel to have nice outputs. */
#include "xbt/asserts.h"
#include "rand.hpp"

typedef struct request s_request_t, *request_t;                   // Client request to scheduling server
typedef struct reply s_reply_t, *reply_t;                         // Client reply to scheduling server
typedef struct dsmessage s_dsmessage_t, *dsmessage_t;             // Message to data server
typedef struct dcsrequest s_dcsrequest_t, *dcsrequest_t;          // Data client request to data client server
typedef struct dcsreply s_dcsreply_t, *dcsreply_t;                // Message to data server
typedef struct dcsmessage s_dcsmessage_t, *dcsmessage_t;          // Message to data client server
typedef struct dcmessage s_dcmessage_t, *dcmessage_t;             // Message to data server
typedef struct dcworkunit s_dcWorkunitT, *dcWorkunitT;            // Data client workunit
typedef struct client s_client_t, *client_t;                      // Client
typedef struct data_server s_dserver_t, *dserver_t;               // Data server
typedef struct data_client_server s_dcserver_t, *dcserver_t;      // Data client server
typedef struct data_client s_dclient_t, *dclient_t;               // Data client
typedef struct client_group s_group_t, *group_t;                  // Client group
typedef struct ask_for_files s_ask_for_files_t, *ask_for_files_t; // Ask for files params

namespace sg4 = simgrid::s4u;
namespace intrusive = boost::intrusive;
namespace xbt = simgrid::xbt;

XBT_LOG_NEW_DEFAULT_CATEGORY(boinc_simulator, "Messages specific for this boinc simulator");

#if defined(__linux__)
#include <execinfo.h>
static void on_segv(int signum)
{
    void *frames[64];
    int n = backtrace(frames, 64);
    fprintf(stderr, "\n[BOINC_DEBUG] Caught signal %d (SIGSEGV). Backtrace follows:\n", signum);
    backtrace_symbols_fd(frames, n, STDERR_FILENO);
    _Exit(128 + signum);
}
#endif

#define MAX_SHORT_TERM_DEBT 86400
#define MAX_TIMEOUT_SERVER 86400 * 365 // One year without client activity, only to finish simulation for a while
#define MAX_BUFFER 300000              // Max buffer

int g_number_projects = 1;                  // Number of projects
int g_total_number_scheduling_servers = 1;  // Number of scheduling servers
int g_total_number_data_servers = 1;        // Number of data servers
int g_total_number_data_client_servers = 1; // Number of data client servers
int g_number_client_groups = 1;             // Number of client groups
// int g_total_number_clients = 1000;          // Number of clients
// int g_total_number_data_clients = 100;      // Number of data clients
// int g_total_number_ordinary_clients = (g_total_number_clients - g_total_number_data_clients);

/* Project back end */
int init_database(int argc, char *argv[]);
int work_generator(int argc, char *argv[]);
int validator(int argc, char *argv[]);
int assimilator(int argc, char *argv[]);

/* Data server */
int data_server_requests(int argc, char *argv[]);
int data_server_dispatcher(int argc, char *argv[]);

/* Data client server */
int data_client_server_requests(int argc, char *argv[]);
int data_client_server_dispatcher(int argc, char *argv[]);

/* Client */
int client(int argc, char *argv[]);

/* Test all */
void test_all(int argc, char *argv[], sg4::Engine &e);

/* Availability statistics */
int64_t _total_power;       // Total clients power (maximum 2⁶³-1)
double _total_available;    // Total time clients available
double _total_notavailable; // Total time clients notavailable

/*
 *	Memory usage in KB (Linux VmRSS only). Previous parseLine() mixed strlen() with an advanced pointer and wrote
 *	line[i-3] past the substring — corrupting memory and crashing later during unrelated actors (e.g. sleep_for).
 */
int memoryUsage()
{
    FILE *file = fopen("/proc/self/status", "r");

    if (file == NULL)
    {
        fprintf(stderr, "[BOINC_DEBUG] memoryUsage: fopen(/proc/self/status) failed (not Linux or no permission); "
                        "RSS unavailable\n");
        fflush(stderr);
        return -1;
    }

    int result = -1;
    char line[128];

    while (fgets(line, sizeof(line), file) != NULL)
    {
        if (strncmp(line, "VmRSS:", 6) == 0)
        {
            unsigned long kb = 0;
            if (sscanf(line, "VmRSS: %lu", &kb) >= 1 || sscanf(line, "VmRSS:%lu", &kb) >= 1)
                result = kb > static_cast<unsigned long>(INT_MAX) ? INT_MAX : static_cast<int>(kb);
            break;
        }
    }
    fclose(file);
    return result;
}

static void free_project(ProjectInstanceOnClient *proj)
{

    auto clean_queue = []<typename T>(T &q)
    {
        while (!q.empty())
            q.pop();
    };
    clean_queue(proj->tasks_ready);
    clean_queue(proj->number_executed_task);
    clean_queue(proj->workunit_executed_task);

    proj->tasks_swag.clear();
    proj->sim_tasks.clear();
    proj->run_list.clear();
}

/*
 *	Process has done i out of n rounds,
 *	and we want a bar of width w and resolution r.
 */
static inline void loadBar(int x, int n, int r, int w)
{
    if (n <= 0 || r <= 0 || w <= 0)
        return;
    const int denom = n / r + 1;
    if (denom <= 0 || x % denom != 0)
        return;

    const float ratio = std::max(0.f, std::min(1.f, x / static_cast<float>(n)));
    int c = static_cast<int>(ratio * static_cast<float>(w));
    if (c < 0)
        c = 0;
    if (c > w)
        c = w;

    printf("Progress: %3d%% [", static_cast<int>(ratio * 100.f));

    for (int i = 0; i < c; i++)
        printf("=");
    for (int i = c; i < w; i++)
        printf(" ");

    printf("]\n");
    if (isatty(STDOUT_FILENO))
        printf("\033[F\033[J");
    fflush(stdout);
}

/*
 *	Print server results
 */
int g_memory = 0; // memory usage

void show_progress(int, char **)
{
    int memoryAux; // memory aux
    double sleep;  // Sleep time

    // Init variables
    sleep = maxtt / 3600.0; // update interval (simulated seconds); equals sim hours when maxtt is in seconds
    fprintf(stderr, "[BOINC_DEBUG] show_progress: start maxtt=%g sleep=%g actor=%s\n",
            maxtt, sleep, sg4::this_actor::get_cname());
    fflush(stderr);

    // Print progress
    for (int progress = 0; ceil(sg4::Engine::get_clock()) < maxtt;)
    {
        if (maxtt <= 0.0)
            break;
        progress = std::min(100, static_cast<int>(std::round(sg4::Engine::get_clock() / maxtt * 100.0)));
        fprintf(stderr, "[BOINC_DEBUG] show_progress: clock=%.6g maxtt=%.6g\n", sg4::Engine::get_clock(), maxtt);
        fflush(stderr);
        loadBar(progress, 100, 200, 50);
        memoryAux = memoryUsage();
        fprintf(stderr, "[BOINC_DEBUG] show_progress: memoryAux=%d g_memory=%d\n", memoryAux, g_memory);
        fflush(stderr);
        if (memoryAux > g_memory)
            g_memory = memoryAux;
        fprintf(stderr, "[BOINC_DEBUG] show_progress: before sleep_for sleep=%.9g\n", sleep);
        fflush(stderr);
        // Chunk simulated sleep: one large sleep_for() has triggered SIGSEGV mid-run here
        // (likely interaction with other actors at the same instant); same total time advance.
        {
            const double max_chunk = 24.0; // simulated seconds per yield
            const double target = sg4::Engine::get_clock() + sleep;
            while (sg4::Engine::get_clock() < target && sg4::Engine::get_clock() < maxtt)
            {
                const double now = sg4::Engine::get_clock();
                if (!std::isfinite(now) || !std::isfinite(target) || !std::isfinite(maxtt))
                {
                    fprintf(stderr, "[BOINC_DEBUG] show_progress: abort chunk loop non-finite clock (now=%g target=%g maxtt=%g)\n",
                            now, target, maxtt);
                    fflush(stderr);
                    break;
                }
                double step = std::min(max_chunk, target - now);
                step = std::min(step, maxtt - now);
                if (!std::isfinite(step) || step <= 1e-9)
                    break;
                if (step > max_chunk)
                    step = max_chunk;
                fprintf(stderr, "[BOINC_DEBUG] show_progress: chunk now=%.6g step=%.6g target=%.6g\n", now, step, target);
                fflush(stderr);
                sg4::this_actor::sleep_for(step);
                fprintf(stderr, "[BOINC_DEBUG] show_progress: chunk-resume now=%.6g\n", sg4::Engine::get_clock());
                fflush(stderr);
            }
        }
        fprintf(stderr, "[BOINC_DEBUG] show_progress: after sleep_for\n");
        fflush(stderr);
    }
    fprintf(stderr, "[BOINC_DEBUG] show_progress: exit\n");
    fflush(stderr);
}

void print_results()
{
    // Init variables
    int k = 0, l = 0, m = 0;

    setlocale(LC_NUMERIC, "en_US.UTF-8");

    printf("\n Memory usage: %'d KB\n", g_memory);

    printf("\n Total number of clients: %'d\n", g_total_number_clients);
    printf(" Total number of ordinary clients: %'d\n", g_total_number_clients - g_total_number_data_clients);
    printf(" Total number of data clients: %'d\n\n", g_total_number_data_clients);

    // Iterate servers information
    for (int i = 0; i < g_number_projects; i++)
    {
        ProjectDatabaseValue &project = SharedDatabase::_pdatabase[i]; // Server info pointer

        // Print results
        printf("\n ####################  %s  ####################\n", project.project_name.c_str());
        printf("\n Simulation ends in %'g h (%'g sec)\n\n", sg4::Engine::get_clock() / 3600.0 - WARM_UP_TIME, sg4::Engine::get_clock() - maxwt);

        double ocload = 0, dcload = 0;
        for (int j = 0; j < project.dsreplication + project.dcreplication; j++)
        {
            printf("  OC. Number of downloads from data server %d: %" PRId64 "\n", j, project.rfiles[j]);
            if (j >= project.dcreplication)
                ocload += project.rfiles[j];
        }

        printf("\n");

        for (int j = 0; j < project.dsreplication + project.dcreplication; j++)
        {
            printf("  DC. Number of downloads from data server %d: %" PRId64 "\n", j, project.dcrfiles[j]);
            if (j >= project.dcreplication)
                dcload += project.dcrfiles[j];
        }

        // printf("OC: %f\n DC: %f\n", ocload, dcload);

        printf("\n");
        for (int j = 0; j < (int64_t)project.nscheduling_servers; j++, l++)
            printf("  Scheduling server %d Busy: %0.1f%%\n", j, SharedDatabase::_sserver_info[l].time_busy / maxst * 100);
        for (int j = 0; j < (int64_t)project.ndata_servers; j++, k++)
            printf("  Data server %d Busy: %0.1f%% (OC: %0.1f%%, DC: %0.1f%%)\n", j, SharedDatabase::_dserver_info[k].time_busy / maxst * 100, (ocload * project.input_file_size + project.dsuploads * project.output_file_size) / ((ocload + dcload) * project.input_file_size + project.dsuploads * project.output_file_size) * 100 * (SharedDatabase::_dserver_info[k].time_busy / maxst), (dcload * project.input_file_size) / ((ocload + dcload) * project.input_file_size + project.dsuploads * project.output_file_size) * 100 * (SharedDatabase::_dserver_info[k].time_busy / maxst));
        printf("\n  Number of clients: %'d\n", project.nclients);
        printf("  Number of ordinary clients: %'d\n", project.nordinary_clients);
        printf("  Number of data clients: %'d\n\n", project.ndata_clients);

        double time_busy = 0;
        int64_t storage = 0;
        double tnavailable = 0;
        for (int j = 0; j < (int64_t)project.ndata_clients; j++, m++)
        {
            time_busy += (SharedDatabase::_dclient_info[m].time_busy);
            storage += (int64_t)(SharedDatabase::_dclient_info[m]).total_storage;
            tnavailable += SharedDatabase::_dclient_info[m].navailable;
        }

        // printf("time busy : %f\n", time_busy);
        if (project.ndata_clients <= 0)
        {
            fprintf(stderr, "[BOINC_DEBUG] print_results: project %s ndata_clients=%d skip DC averages\n",
                    project.project_name.c_str(), (int)project.ndata_clients);
            fflush(stderr);
            time_busy = 0;
        }
        else
        {
            time_busy = time_busy / project.ndata_clients / maxst * 100;
            storage /= (double)project.ndata_clients;
        }

        printf("\n  Data clients average load: %0.1f%%\n", time_busy);
        printf("  Data clients average storage: %'" PRId64 " MB\n", storage);
        if (project.ndata_clients > 0)
            printf("  Data clients availability: %0.1f%%\n\n",
                   (maxst - (tnavailable / project.ndata_clients)) / maxtt * 100);
        else
            printf("  Data clients availability: n/a\n\n");

        printf("\n  Messages received: \t\t%'" PRId64 " (work requests received + results received)\n", project.nmessages_received);
        printf("  Work requests received: \t%'" PRId64 "\n", project.nwork_requests);
        printf("  Results created: \t\t%'" PRId64 " (%0.1f%%)\n", project.nresults, (double)project.nresults / project.nwork_requests * 100);
        printf("  Results sent: \t\t%'" PRId64 " (%0.1f%%)\n", project.nresults_sent, (double)project.nresults_sent / project.nresults * 100);
        printf("  Results cancelled: \t\t%'" PRId64 " (%0.1f%%)\n", project.ncancelled_results, (double)project.ncancelled_results / project.nresults_sent * 100);
        printf("  Results received: \t\t%'" PRId64 " (%0.1f%%)\n", project.nresults_received, (double)project.nresults_received / project.nresults * 100);
        printf("  Results analyzed: \t\t%'" PRId64 " (%0.1f%%)\n", project.nresults_analyzed, (double)project.nresults_analyzed / project.nresults_received * 100);
        printf("  Results success: \t\t%'" PRId64 " (%0.1f%%)\n", project.nsuccess_results, (double)project.nsuccess_results / project.nresults_analyzed * 100);
        printf("  Results failed: \t\t%'" PRId64 " (%0.1f%%)\n", project.nerror_results, (double)project.nerror_results / project.nresults_analyzed * 100);
        printf("  Results too late: \t\t%'" PRId64 " (%0.1f%%)\n", project.ndelay_results, (double)project.ndelay_results / project.nresults_analyzed * 100);
        printf("  Results valid: \t\t%'" PRId64 " (%0.1f%%)\n", project.nvalid_results, (double)project.nvalid_results / project.nresults_analyzed * 100);
        printf("  Workunits total: \t\t%'" PRId64 "\n", project.nworkunits);
        printf("  Workunits completed: \t\t%'" PRId64 " (%0.1f%%)\n", project.nvalid_workunits + project.nerror_workunits, (double)(project.nvalid_workunits + project.nerror_workunits) / project.nworkunits * 100);
        printf("  Workunits not completed: \t%'" PRId64 " (%0.1f%%)\n", (project.nworkunits - project.nvalid_workunits - project.nerror_workunits), (double)(project.nworkunits - project.nvalid_workunits - project.nerror_workunits) / project.nworkunits * 100);
        printf("  Workunits valid: \t\t%'" PRId64 " (%0.1f%%)\n", project.nvalid_workunits, (double)project.nvalid_workunits / project.nworkunits * 100);
        printf("  Workunits error: \t\t%'" PRId64 " (%0.1f%%)\n", project.nerror_workunits, (double)project.nerror_workunits / project.nworkunits * 100);
        printf("  Throughput: \t\t\t%'0.1f mens/s\n", (double)project.nmessages_received / maxst);
        printf("  Credit granted: \t\t%'" PRId64 " credits\n", (long int)project.total_credit);
        printf("  FLOPS in split: \t\t %0.1f and %0.1f and %0.1f end\n\n", (double)project.nvalid_results, (double)project.job_duration, maxst);

        printf("  FLOPS average: \t\t%'" PRId64 " GFLOPS\n\n", (int64_t)((double)project.nvalid_results * (double)project.job_duration / maxst / 1000000000.0));
    }

    fflush(stdout);
}

void server_side_termination()
{
    fprintf(stderr, "[BOINC_DEBUG] server_side_termination: begin t=%.6g projects=%zu\n", sg4::Engine::get_clock(),
            SharedDatabase::_pdatabase.size());
    fflush(stderr);
    for (size_t pi = 0; pi < SharedDatabase::_pdatabase.size(); ++pi)
    {
        auto &pdatabase = SharedDatabase::_pdatabase[pi];
        for (int i = 0; i < pdatabase.nscheduling_servers; i++)
        {
            SchedulingServerMessage *msg = new SchedulingServerMessage();
            msg->type = TERMINATION;
            auto ser_n_mb = pdatabase.scheduling_servers[i];
            fprintf(stderr, "[BOINC_DEBUG] server_side_termination: TERMINATION -> scheduling mb=%s proj=%zu\n",
                    ser_n_mb.c_str(), pi);
            fflush(stderr);
            sg4::Mailbox::by_name(ser_n_mb)->put(msg, 1);
        }

        for (int i = 0; i < pdatabase.ndata_client_servers; i++)
        {
            dcsmessage *msg = new dcsmessage();
            msg->type = TERMINATION;
            auto ser_n_mb = pdatabase.data_client_servers[i];
            fprintf(stderr, "[BOINC_DEBUG] server_side_termination: TERMINATION -> dc_server mb=%s proj=%zu\n",
                    ser_n_mb.c_str(), pi);
            fflush(stderr);
            sg4::Mailbox::by_name(ser_n_mb)->put(msg, 1);
        }
    }
    fprintf(stderr, "[BOINC_DEBUG] server_side_termination: done t=%.6g\n", sg4::Engine::get_clock());
    fflush(stderr);
}

/*
 *	Init database
 */
int init_database(int argc, char *argv[])
{
    int i, project_number;

    if (argc != 20)
    {
        fprintf(stderr, "[BOINC_DEBUG] init_database: invalid argc=%d (expected 20)\n", argc);
        fflush(stderr);
        printf("Invalid number of parameter in init_database()\n");
        return 0;
    }

    project_number = atoi(argv[1]);
    ProjectDatabaseValue &project = SharedDatabase::_pdatabase[project_number];

    // Init database
    project.project_number = project_number;               // Project number
    project.project_name = argv[2];                        // Project name
    project.output_file_size = (int64_t)atoll(argv[3]);    // Answer size
    project.job_duration = (int64_t)atoll(argv[4]);        // Workunit duration
    project.ifgl_percentage = (char)atoi(argv[5]);         // Percentage of input files generated locally
    project.ifcd_percentage = (char)atoi(argv[6]);         // Number of workunits that share the same input files
    project.averagewpif = (char)atoi(argv[7]);             // Average workunits per input files
    project.min_quorum = (int32_t)atoi(argv[8]);           // Quorum
    project.target_nresults = (int32_t)atoi(argv[9]);      // target_nresults
    project.max_error_results = (int32_t)atoi(argv[10]);   // max_error_results
    project.max_total_results = (int32_t)atoi(argv[11]);   // Maximum number of times a task must be sent
    project.max_success_results = (int32_t)atoi(argv[12]); // max_success_results
    project.delay_bound = (int64_t)atoll(argv[13]);        // Workunit deadline
    project.input_file_size = (int64_t)atoll(argv[14]);    // Input file size
    project.disk_bw = (int64_t)atoll(argv[15]);            // Disk bandwidth
    project.ndata_servers = (char)atoi(argv[16]);          // Number of data servers
    project.output_file_storage = (int32_t)atoi(argv[17]); // Output file storage [0 -> data servers, 1 -> data clients]
    project.dsreplication = (int32_t)atoi(argv[18]);       // File replication in data servers
    project.dcreplication = (int32_t)atoi(argv[19]);       // File replication in data clients
    project.nmessages_received = 0;                        // Store number of messages rec.
    project.nresults = 0;                                  // Number of results created
    project.nresults_sent = 0;                             // Number of results sent
    project.nwork_requests = 0;                            // Store number of requests rec.
    project.nvalid_results = 0;                            // Number of valid results (with a consensus)
    project.nresults_received = 0;                         // Number of results received (replies)
    project.nresults_analyzed = 0;                         // Number of results analyzed
    project.nsuccess_results = 0;                          // Number of success results
    project.nerror_results = 0;                            // Number of erroneous results
    project.ndelay_results = 0;                            // Number of delayed results
    project.total_credit = 0;                              // Total credit granted
    project.nworkunits = 0;                                // Number of workunits created
    project.nvalid_workunits = 0;                          // Number of valid workunits
    project.nerror_workunits = 0;                          // Number of erroneous workunits
    project.ncurrent_deleted_workunits = 0;                // Number of current deleted workunits
    project.nfinished_scheduling_servers = 0;              // Number of finished scheduling servers

    // File input file requests
    project.dsuploads = 0;
    project.rfiles.resize(project.dsreplication + project.dcreplication);
    project.dcrfiles.resize(project.dsreplication + project.dcreplication);
    for (i = 0; i < project.dsreplication + project.dcreplication; i++)
    {
        project.rfiles[i] = 0;
        project.dcrfiles[i] = 0;
    }

    // Fill with data server names
    project.data_servers.reserve(project.ndata_servers);
    for (i = 0; i < project.ndata_servers; i++)
    {
        auto Mal = bprintf("d%" PRId32 "%" PRId32, project_number + 1, i);
        project.data_servers.push_back(Mal);
    }

    project.barrier->wait();

    return 0;
}

/*
 *	Generate workunit
 */
WorkunitT *generate_workunit(ProjectDatabaseValue &project)
{
    WorkunitT *workunit = new WorkunitT();
    workunit->number = std::to_string(project.nworkunits);
    workunit->status = IN_PROGRESS;
    workunit->ndata_clients = 0;
    workunit->ndata_clients_confirmed = 0;
    workunit->ntotal_results = 0;
    workunit->nsent_results = 0;
    workunit->nresults_received = 0;
    workunit->nvalid_results = 0;
    workunit->nsuccess_results = 0;
    workunit->nerror_results = 0;
    workunit->ncurrent_error_results = 0;
    workunit->credits = -1;
    workunit->times.reserve(project.max_total_results);
    workunit->ninput_files = project.dcreplication + project.dsreplication;
    workunit->input_files.resize(workunit->ninput_files);
    project.ncurrent_workunits++;

    int i;
    for (i = 0; i < project.dcreplication; i++)
        workunit->input_files[i] = "";
    for (; i < workunit->ninput_files; i++)
        workunit->input_files[i] = project.data_servers[uniform_int(0, project.ndata_servers - 1, *g_rndg)];

    project.nworkunits++;

    return workunit;
}

/*
 *	Work generator
 */
int work_generator(int argc, char *argv[])
{

    if (argc != 2)
    {
        fprintf(stderr, "[BOINC_DEBUG] work_generator: invalid argc=%d (expected 2)\n", argc);
        fflush(stderr);
        printf("Invalid number of parameter in work_generator()\n");
        return 0;
    }

    int project_number = atoi(argv[1]);
    ProjectDatabaseValue &project = SharedDatabase::_pdatabase[project_number];

    // Wait until the database is initiated

    project.barrier->wait();

    while (!project.wg_end)
    {

        // this is strange - double lock. probably meant w_mutex
        std::unique_lock lock(*(project.w_mutex));

        // todo: search what this variables mean and extend the naming
        while (project.ncurrent_workunits >= MAX_BUFFER && !project.wg_end && !project.wg_dcs)
            project.wg_full->wait(lock);

        if (project.wg_end)
        {
            fprintf(stderr, "[BOINC_DEBUG] work_generator: wg_end set, exiting proj=%d t=%.6g\n", project_number,
                    sg4::Engine::get_clock());
            fflush(stderr);
            lock.unlock();
            break;
        }

        project.wg_dcs = 0;

        // Check if there are error results
        project.er_mutex->lock();

        // Regenerate result when error result
        if (project.ncurrent_error_results > 0)
        {
            while (project.ncurrent_error_results > 0)
            {
                // Get workunit associated with the error result
                auto workunit = project.current_error_results.front();
                project.current_error_results.pop();
                project.ncurrent_error_results--;
                project.er_mutex->unlock();

                // Generate new instance from the workunit
                project.r_mutex->lock();
                auto result = generate_result(project, workunit, 1);
                project.current_results.push_back(result);
                project.r_mutex->unlock();

                project.er_mutex->lock();
            }
        }
        // Create new workunit
        else
        {
            // Generate workunit
            WorkunitT *workunit = generate_workunit(project);
            project.current_workunits[workunit->number] = workunit;
            // todo: don't I need to notify someone here...
        }

        project.er_mutex->unlock();
        lock.unlock();
    }

    return 0;
}

/*
 *	Validator
 */
int validator(int argc, char *argv[])
{
    reply_t reply = NULL;

    if (argc != 2)
    {
        fprintf(stderr, "[BOINC_DEBUG] validator: invalid argc=%d (expected 2)\n", argc);
        fflush(stderr);
        printf("Invalid number of parameter in validator()\n");
        return 0;
    }

    int project_number = atoi(argv[1]);
    ProjectDatabaseValue &project = SharedDatabase::_pdatabase[project_number];

    fprintf(stderr, "[BOINC_DEBUG] validator: actor started proj=%d t=%.6g\n", project_number,
            sg4::Engine::get_clock());
    fflush(stderr);

    // Wait until the database is initiated
    project.barrier->wait();

    while (!project.v_end)
    {

        std::unique_lock lock(*project.v_mutex);

        while (project.ncurrent_validations == 0 && !project.v_end)
            project.v_empty->wait(lock);

        if (project.v_end)
        {
            fprintf(stderr, "[BOINC_DEBUG] validator: v_end set, exiting proj=%d t=%.6g\n", project_number,
                    sg4::Engine::get_clock());
            fflush(stderr);
            break;
        }

        // Get received result
        reply = project.current_validations.front();
        project.current_validations.pop();
        project.ncurrent_validations--;
        lock.unlock();

        // Serialize with assimilator/file_deleter and work_generator on current_workunits.
        std::unique_lock wlk(*(project.w_mutex));

        WorkunitT *workunit = nullptr;
        try
        {
            workunit = project.current_workunits.at(reply->workunit);
        }
        catch (const std::out_of_range &)
        {
            fprintf(stderr, "[BOINC_DEBUG] validator: unknown workunit=%s t=%.6g\n", reply->workunit.c_str(),
                    sg4::Engine::get_clock());
            fflush(stderr);
            delete reply;
            reply = NULL;
            continue;
        }

        workunit->nresults_received++;

        // Defensive guard: malformed/late replies may carry an out-of-range result_number.
        // Never index times[] without validating bounds.
        const auto result_index = static_cast<size_t>(reply->result_number);
        if (reply->result_number < 0 || result_index >= workunit->times.size())
        {
            fprintf(stderr,
                    "[BOINC_DEBUG] validator: out-of-range result_number workunit=%s idx=%d times_size=%zu t=%.6g\n",
                    workunit->number.c_str(), reply->result_number, workunit->times.size(), sg4::Engine::get_clock());
            fflush(stderr);
            reply->status = FAIL;
            workunit->nerror_results++;
            project.nerror_results++;
            project.nresults_analyzed++;
            delete reply;
            reply = NULL;
            continue;
        }
        if (sg4::Engine::get_clock() - workunit->times[result_index] >= project.delay_bound)
        {
            fprintf(stderr,
                    "[BOINC_DEBUG] validator: delay_bound FAIL workunit=%s result_idx=%d age=%.6g bound=%" PRId64
                    " t=%.6g\n",
                    workunit->number.c_str(), reply->result_number,
                    sg4::Engine::get_clock() - workunit->times[result_index], (int64_t)project.delay_bound,
                    sg4::Engine::get_clock());
            fflush(stderr);
            reply->status = FAIL;
            workunit->nerror_results++;
            project.ndelay_results++;
        }
        // Success result
        else if (reply->status == SUCCESS)
        {
            workunit->nsuccess_results++;
            project.nsuccess_results++;
            if (reply->value == CORRECT)
            {
                workunit->nvalid_results++;
                if (workunit->credits == -1)
                    workunit->credits = reply->credits;
                else
                    workunit->credits = workunit->credits > reply->credits ? reply->credits : workunit->credits;
            }
        }
        // Error result
        else
        {
            workunit->nerror_results++;
            project.nerror_results++;
        }
        project.nresults_analyzed++;

        // Check workunit
        project.er_mutex->lock();
        if (workunit->status == IN_PROGRESS)
        {
            if (workunit->nvalid_results >= project.min_quorum)
            {
                workunit->status = VALID;
                project.nvalid_results += (int64_t)(workunit->nvalid_results);
                project.total_credit += (int64_t)(workunit->credits * workunit->nvalid_results);
            }
            else if (workunit->ntotal_results >= project.max_total_results ||
                     workunit->nerror_results >= project.max_error_results ||
                     workunit->nsuccess_results >= project.max_success_results)
            {
                workunit->status = ERROR;
            }
        }
        else if (workunit->status == VALID && reply->status == SUCCESS && reply->value == CORRECT)
        {
            project.nvalid_results++;
            project.total_credit += (int64_t)(workunit->credits);
        }

        // If result is an error and task is not completed, call work generator in order to create a new instance
        if (reply->status == FAIL)
        {
            if (workunit->status == IN_PROGRESS &&
                workunit->nsuccess_results < project.max_success_results &&
                workunit->nerror_results < project.max_error_results &&
                workunit->ntotal_results < project.max_total_results)
            {
                project.current_error_results.push(workunit);
                project.ncurrent_error_results++;
                workunit->ncurrent_error_results++;
            }
        }

        // Call assimilator if workunit has been completed (exactly once: multiple validator passes can see the same
        // terminal state before the assimilator actor runs; duplicate queue entries corrupt file_deleter / TSQueue).
        {
            const int nr = static_cast<unsigned char>(workunit->nresults_received);
            const int nt = static_cast<unsigned char>(workunit->ntotal_results);
            if ((workunit->status != IN_PROGRESS) && nr == nt && (workunit->ncurrent_error_results == 0))
            {
                project.a_mutex->lock();
                bool expected = false;
                if (workunit->assimilation_queued.compare_exchange_strong(expected, true))
                {
                    fprintf(stderr,
                            "[BOINC_DEBUG] validator: queue assimilator workunit=%s status=%d recv=%d/%d t=%.6g\n",
                            workunit->number.c_str(), (int)workunit->status, nr, nt, sg4::Engine::get_clock());
                    fflush(stderr);
                    project.current_assimilations.push(workunit->number);
                    project.ncurrent_assimilations++;
                    project.a_empty->notify_all();
                }
                project.a_mutex->unlock();
            }
        }
        project.er_mutex->unlock();

        delete reply;
        reply = NULL;
    }

    return 0;
}

/*
 *	File deleter
 *
 *  current_deletions is std::queue protected only under project.w_mutex (same as current_workunits).
 *  Avoid TSQueue here: its SimGrid mutex + w_mutex interaction still faulted after assimilate (drain_count=0 path).
 */
int file_deleter(ProjectDatabaseValue &project, std::string workunit_number)
{
    auto can_delete_condition = [](WorkunitT *workunit)
    {
        return workunit->ndata_clients == workunit->ndata_clients_confirmed &&
               workunit->ndata_clients_confirmed == workunit->number_past_through_assimilator.load();
    };

    std::unique_lock wlk(*(project.w_mutex));

    WorkunitT *workunit = nullptr;
    try
    {
        workunit = project.current_workunits.at(workunit_number);
    }
    catch (const std::out_of_range &)
    {
        fprintf(stderr, "[BOINC_DEBUG] file_deleter: missing workunit key=%s (first lookup) t=%.6g\n",
                workunit_number.c_str(), sg4::Engine::get_clock());
        fflush(stderr);
        return 0;
    }

    bool erased_immediately = false;
    if (can_delete_condition(workunit))
    {
        project.current_workunits.erase(workunit_number);
        project.dcmutex->lock();
        project.ncurrent_deleted_workunits++;
        project.dcmutex->unlock();
        erased_immediately = true;
    }

    if (!erased_immediately && workunit != nullptr)
        project.current_deletions.push(workunit);

    const size_t drain_count = project.current_deletions.size();
    fprintf(stderr, "[BOINC_DEBUG] file_deleter: workunit=%s drain_count=%zu t=%.6g\n", workunit_number.c_str(),
            drain_count, sg4::Engine::get_clock());
    fflush(stderr);

    for (size_t i = 0; i < drain_count; ++i)
    {
        WorkunitT *qw = project.current_deletions.front();
        project.current_deletions.pop();
        if (can_delete_condition(qw))
        {
            project.current_workunits.erase(qw->number);
            project.dcmutex->lock();
            project.ncurrent_deleted_workunits++;
            project.dcmutex->unlock();
        }
        else
            project.current_deletions.push(qw);
    }

    project.ncurrent_deletions = static_cast<int64_t>(project.current_deletions.size());
    return 0;
}

/*
 *	Assimilator
 */
int assimilator(int argc, char *argv[])
{

    if (argc != 2)
    {
        fprintf(stderr, "[BOINC_DEBUG] assimilator: invalid argc=%d (expected 2)\n", argc);
        fflush(stderr);
        printf("Invalid number of parameter in assimilator()\n");
        return 0;
    }

    int project_number = atoi(argv[1]);
    ProjectDatabaseValue &project = SharedDatabase::_pdatabase[project_number];

    fprintf(stderr, "[BOINC_DEBUG] assimilator: actor started proj=%d t=%.6g\n", project_number,
            sg4::Engine::get_clock());
    fflush(stderr);

    // Wait until the database is initiated
    project.barrier->wait();

    while (!project.a_end)
    {

        std::unique_lock lock(*project.a_mutex);

        while (project.ncurrent_assimilations == 0 && !project.a_end)
            project.a_empty->wait(lock);

        if (project.a_end)
        {
            fprintf(stderr, "[BOINC_DEBUG] assimilator: a_end set, exiting proj=%d t=%.6g\n", project_number,
                    sg4::Engine::get_clock());
            fflush(stderr);
            break;
        }

        // Get workunit number to assimilate
        std::string workunit_number = project.current_assimilations.front();
        project.current_assimilations.pop();
        project.ncurrent_assimilations--;
        lock.unlock();

        WorkunitT *workunit = nullptr;
        std::unique_lock wlk(*(project.w_mutex));
        try
        {
            workunit = project.current_workunits.at(workunit_number);
        }
        catch (const std::out_of_range &)
        {
            wlk.unlock();
            fprintf(stderr, "[BOINC_DEBUG] assimilator: missing workunit key=%s proj=%d t=%.6g\n",
                    workunit_number.c_str(), project_number, sg4::Engine::get_clock());
            fflush(stderr);
            continue;
        }

        fprintf(stderr, "[BOINC_DEBUG] assimilator: assimilate workunit=%s status=%d proj=%d t=%.6g\n",
                workunit_number.c_str(), (int)workunit->status, project_number, sg4::Engine::get_clock());
        fflush(stderr);

        // Update workunit stats
        if (workunit->status == VALID)
            project.nvalid_workunits++;
        else
            project.nerror_workunits++;

        workunit->number_past_through_assimilator.fetch_add(1);
        wlk.unlock();

        // file_deleter acquires w_mutex itself and avoids holding it across TSQueue ops
        file_deleter(project, workunit->number);
    }

    return 0;
}

/*
 *	Projects initialization
 */
static void client_initialize_projects(client_t client, int argc, char **argv)
{
    std::map<std::string, ProjectInstanceOnClient *> dict;
    int number_proj;
    int i, index;

    number_proj = atoi(argv[0]);

    if (argc - 1 != number_proj * 5)
    {
        fprintf(stderr, "[BOINC_DEBUG] client_initialize_projects: argc mismatch got=%d need=%d (number_proj=%d)\n",
                argc - 1, number_proj * 5, number_proj);
        fflush(stderr);
        printf("Invalid number of parameters to client: %d. It should be %d\n", argc - 1, number_proj * 3);
        exit(1);
    }

    index = 1;
    for (i = 0; i < number_proj; i++)
    {
        ProjectInstanceOnClient *proj = new ProjectInstanceOnClient();
        proj->name = std::string(argv[index++]);
        proj->number = (char)atoi(argv[index++]);
        proj->priority = (char)atoi(argv[index++]);
        proj->success_percentage = (char)atoi(argv[index++]);
        proj->canonical_percentage = (char)atoi(argv[index++]);

        proj->on = 1;
        proj->stop_requested.store(false);

        proj->answer_mailbox = proj->name + client->name;

        proj->number_executed_task;   // Queue with task's numbers
        proj->workunit_executed_task; // Queue with task's sizes

        proj->run_list_mutex = sg4::Mutex::create();
        proj->tasks_swag_mutex = sg4::Mutex::create();

        proj->tasks_ready_mutex = sg4::Mutex::create();
        proj->tasks_ready_cv_is_empty = sg4::ConditionVariable::create();
        proj->stop_requested.store(false);

        proj->total_tasks_checked = 0;
        proj->total_tasks_executed = 0;
        proj->total_tasks_received = 0;
        proj->total_tasks_missed = 0;

        proj->client = client;

        dict[proj->name] = proj;
    }
    client->projects = dict;
}

/*****************************************************************************/
#include <boost/algorithm/string.hpp>
std::vector<double> get_distribution_parameter(char *argument)
{
    std::string str = argument;
    std::vector<std::string> tokens;

    boost::split(tokens, str, boost::is_any_of(","));
    std::vector<double> result;
    result.reserve(tokens.size());
    for (auto token : tokens)
    {
        result.push_back(atof(token.data()));
    }
    return result;
}

static client_t client_new(int argc, char *argv[])
{
    client_t client;
    std::string work_string;
    // xbt_dict_cursor_t cursor = NULL;
    int32_t group_number;
    double r = 0, aux = -1;
    int index = 1;

    client = new s_client_t();

    client->group_number = group_number = (int32_t)atoi(argv[index++]);

    // Initialize values
    if (argc > 3)
    {
        SharedDatabase::_group_info[group_number].group_power = (int32_t)sg4::this_actor::get_host()->get_speed();
        SharedDatabase::_group_info[group_number].n_clients = (int32_t)atoi(argv[index++]);
        SharedDatabase::_group_info[group_number].n_ordinary_clients = (int32_t)atoi(argv[index++]);
        SharedDatabase::_group_info[group_number].connection_interval = atof(argv[index++]);
        SharedDatabase::_group_info[group_number].scheduling_interval = atof(argv[index++]);
        SharedDatabase::_group_info[group_number].max_power = atof(argv[index++]);
        SharedDatabase::_group_info[group_number].min_power = atof(argv[index++]);
        SharedDatabase::_group_info[group_number].sp_distri = (char)atoi(argv[index++]);
        SharedDatabase::_group_info[group_number].sa_param = get_distribution_parameter(argv[index++]);
        SharedDatabase::_group_info[group_number].sb_param = get_distribution_parameter(argv[index++]);
        SharedDatabase::_group_info[group_number].db_distri = (char)atoi(argv[index++]);
        SharedDatabase::_group_info[group_number].da_param = get_distribution_parameter(argv[index++]);
        SharedDatabase::_group_info[group_number].db_param = get_distribution_parameter(argv[index++]);
        SharedDatabase::_group_info[group_number].av_distri = (char)atoi(argv[index++]);
        SharedDatabase::_group_info[group_number].aa_param = get_distribution_parameter(argv[index++]);
        SharedDatabase::_group_info[group_number].ab_param = get_distribution_parameter(argv[index++]);
        SharedDatabase::_group_info[group_number].nv_distri = (char)atoi(argv[index++]);
        SharedDatabase::_group_info[group_number].na_param = get_distribution_parameter(argv[index++]);
        SharedDatabase::_group_info[group_number].nb_param = get_distribution_parameter(argv[index++]);

        aux = parameters::parse_trace_parameter(argv[index++]);

        SharedDatabase::_group_info[group_number].proj_args = &argv[index];
        SharedDatabase::_group_info[group_number].on = argc - index;

        SharedDatabase::_group_info[group_number].cond->notify_all();
    }
    else
    {
        std::unique_lock lock(*SharedDatabase::_group_info[group_number].mutex);
        while (SharedDatabase::_group_info[group_number].on == 0)
            SharedDatabase::_group_info[group_number].cond->wait(lock);
        aux = parameters::parse_trace_parameter(argv[index++]);
    }

    if (aux == -1)
    {
        aux = ran_distri(SharedDatabase::_group_info[group_number].sp_distri, SharedDatabase::_group_info[group_number].sa_param, SharedDatabase::_group_info[group_number].sb_param, *g_rndg_for_host_speed);
        if (aux > SharedDatabase::_group_info[group_number].max_power)
            aux = SharedDatabase::_group_info[group_number].max_power;
        else if (aux < SharedDatabase::_group_info[group_number].min_power)
            aux = SharedDatabase::_group_info[group_number].min_power;
    }

    client->power = (int64_t)(aux * 1000000000.0);

    client->factor = (double)client->power / SharedDatabase::_group_info[group_number].group_power;

    client->name = sg4::this_actor::get_host()->get_name();

    client_initialize_projects(client, SharedDatabase::_group_info[group_number].on, SharedDatabase::_group_info[group_number].proj_args);
    // client->deadline_missed; // FELIX, antes había 8. = xbt_heap_new(8, NULL)

    // printf("Client power: %f GFLOPS\n", client->power/1000000000.0);

    client->on = 0;
    client->running_project = NULL;

    // Suspender a work_fetch_thread cuando la máquina se cae
    client->ask_for_work_mutex = sg4::Mutex::create();
    client->ask_for_work_cond = sg4::ConditionVariable::create();

    client->suspended = 0;

    client->sched_mutex = sg4::Mutex::create();
    client->sched_cond = sg4::ConditionVariable::create();
    client->work_fetch_mutex = sg4::Mutex::create();
    client->work_fetch_cond = sg4::ConditionVariable::create();

    client->finished = 0;

    client->mutex_init = sg4::Mutex::create();
    client->cond_init = sg4::ConditionVariable::create();
    client->initialized = 0;
    client->n_projects = 0;

    work_string = bprintf("work_fetch:%s", client->name.c_str());
    client->work_fetch_thread = sg4::Actor::create(work_string, sg4::this_actor::get_host(), client_work_fetch, client);

    // printf("Starting client %s, ConnectionInterval %lf SchedulingInterval %lf\n", client->name, SharedDatabase::_group_info[client->group_number].connection_interval, _group_power[client->group_number].scheduling_interval);

    /* start one thread to each project to run tasks */
    for (auto &[key, proj] : client->projects)
    {

        std::string proj_name = bprintf("%s:%s\n", key.c_str(), client->name.c_str());
        proj->thread = sg4::Actor::create(proj_name, sg4::this_actor::get_host(), &client_execute_tasks, proj);

        r += proj->priority;
        client->n_projects++;
    }

    client->mutex_init->lock();
    client->sum_priority = r;
    client->initialized = 1;
    client->cond_init->notify_all();
    client->mutex_init->unlock();

    return client;
}

std::atomic_int g_terminated_clients_cnt{0};

// Main client function
int client(int argc, char *argv[])
{
    client_t client;
    dsmessage_t msg2;
    int64_t power;

    client = client_new(argc, argv);
    power = client->power;

    fprintf(stderr, "[BOINC_DEBUG] client: start name=%s t=%.6g\n", client->name.c_str(), sg4::Engine::get_clock());
    fflush(stderr);

    auto [available, notavailable] = client_main_loop(client);

    client->work_fetch_cond->notify_all();

    {
        std::unique_lock lock(*client->ask_for_work_mutex);
        while (client->suspended != -1)
            client->ask_for_work_cond->wait(lock);
    }
    // Ensure work_fetch actor is fully terminated before client is deleted.
    // Waiting only on suspended==-1 is not enough: the actor may still be unwinding.
    if (client->work_fetch_thread)
    {
        fprintf(stderr, "[BOINC_DEBUG] client: join work_fetch_thread name=%s t=%.6g\n",
                client->name.c_str(), sg4::Engine::get_clock());
        fflush(stderr);
        client->work_fetch_thread->join();
        client->work_fetch_thread = nullptr;
    }

    // printf("Client %s finish at %e\n", client->name, sg4::Engine::get_clock());

// Print client execution results
#if 0
	xbt_dict_foreach(client->projects, cursor, key, proj) {
                printf("Client %s:   Projet: %s    total tasks executed: %d  total tasks received: %d total missed: %d\n",
                        client->name, proj->name, proj->total_tasks_executed,
                        proj->total_tasks_received, proj->total_tasks_missed);
        }
#endif
    for (auto &[name, project_on_client] : client->projects)
    {
        SharedDatabase::_pdatabase[(int)project_on_client->number].ncancelled_results += project_on_client->total_tasks_missed;
    }

    // Print client finish
    // printf("Client %s %f GLOPS finish en %g sec. %g horas.\t Working: %0.1f%% \t Not working %0.1f%%\n", client->name, client->power/1000000000.0, t0, t0/3600.0, available*100/(available+notavailable), (notavailable)*100/(available+notavailable));

    SharedDatabase::_group_info[client->group_number].mutex->lock();
    SharedDatabase::_group_info[client->group_number].total_available += available * 100 / (available + notavailable);
    SharedDatabase::_group_info[client->group_number].total_notavailable += (notavailable) * 100 / (available + notavailable);
    SharedDatabase::_group_info[client->group_number].total_power += power;
    SharedDatabase::_group_info[client->group_number].mutex->unlock();

    // Finish client
    sg4::this_actor::sleep_for(60);

    // Stop project execution actors without holding a SimGrid mutex across join().
    fprintf(stderr, "[BOINC_DEBUG] client: shutdown notify name=%s t=%.6g\n", client->name.c_str(),
            sg4::Engine::get_clock());
    fflush(stderr);
    for (auto &[key, proj] : client->projects)
    {
        proj->stop_requested.store(true);
        proj->tasks_ready_cv_is_empty->notify_all();
    }
    for (auto &[key, proj] : client->projects)
    {
        if (proj->thread)
        {
            fprintf(stderr, "[BOINC_DEBUG] client: join project=%s name=%s\n", key.c_str(), client->name.c_str());
            fflush(stderr);
            proj->thread->join();
            proj->thread = nullptr;
        }
    }
    fprintf(stderr, "[BOINC_DEBUG] client: oclient_mutex critical name=%s t=%.6g\n", client->name.c_str(),
            sg4::Engine::get_clock());
    fflush(stderr);
    {
        std::unique_lock lock(*SharedDatabase::oclient_mutex);
        for (auto &[key, proj] : client->projects)
        {
            SharedDatabase::_group_info[client->group_number].nfinished_oclients++;
            if (SharedDatabase::_group_info[client->group_number].nfinished_oclients ==
                SharedDatabase::_group_info[client->group_number].n_ordinary_clients)
            {
                for (int i = 0; i < SharedDatabase::_pdatabase[(int)proj->number].ndata_clients; i++)
                {
                    msg2 = new s_dsmessage_t();
                    msg2->type = TERMINATION;
                    auto cl_n_mb = SharedDatabase::_pdatabase[(int)proj->number].data_clients[i];
                    if (cl_n_mb.empty())
                    {
                        fprintf(stderr,
                                "[BOINC_DEBUG] client: skip empty data_client mailbox proj=%d idx=%d t=%.6g\n",
                                (int)proj->number, i, sg4::Engine::get_clock());
                        fflush(stderr);
                        delete msg2;
                        continue;
                    }
                    if (!the_same_client_group(sg4::this_actor::get_host()->get_name(), cl_n_mb))
                    {
                        fprintf(stderr,
                                "[BOINC_DEBUG] client: skip TERMINATION wrong group host=%s dc_mb=%s proj=%d "
                                "t=%.6g\n",
                                sg4::this_actor::get_host()->get_name().c_str(), cl_n_mb.c_str(), (int)proj->number,
                                sg4::Engine::get_clock());
                        fflush(stderr);
                        delete msg2;
                        continue;
                    }

                    sg4::Mailbox::by_name(cl_n_mb)->put(msg2, 1);
                }
            }
        }
    }

    fprintf(stderr, "[BOINC_DEBUG] client: left oclient_mutex critical name=%s t=%.6g\n", client->name.c_str(),
            sg4::Engine::get_clock());
    fflush(stderr);

    // last client sends termination messages to the server side.
    // Atomic increment avoids data races when many clients finish close together.
    int terminated_now = g_terminated_clients_cnt.fetch_add(1) + 1;
    if (terminated_now == g_total_number_ordinary_clients)
    {
        fprintf(stderr, "[BOINC_DEBUG] client: server_side_termination t=%.6g\n", sg4::Engine::get_clock());
        fflush(stderr);
        server_side_termination();
    }

    fprintf(stderr, "[BOINC_DEBUG] client: delete client name=%s t=%.6g\n", client->name.c_str(),
            sg4::Engine::get_clock());
    fflush(stderr);
    delete (client);

    return 0;
} /* end_of_client */

/*****************************************************************************/

/** Test function */
void test_all(int argc, char *argv[], sg4::Engine &e)
{
    const char *platform_file = argv[1];
    const char *application_file = argv[2];

    // printf("Executing test_all\n");

    int i, days, hours, min;
    double t; // Program time

    // sg4::Engine::set_config("contexts/nthreads:1");

    t = (double)time(NULL);

    {
        /*  Simulation setting */
        e.load_platform(platform_file);

        e.on_simulation_end_cb(print_results);

        e.register_function("show_progress", show_progress);
        e.register_function("init_database", init_database);
        e.register_function("work_generator", work_generator);
        e.register_function("validator", validator);
        e.register_function("assimilator", assimilator);
        e.register_function("scheduling_server_requests", scheduling_server_requests);
        e.register_function("scheduling_server_dispatcher", scheduling_server_dispatcher);
        e.register_function("data_server_requests", data_server_requests);
        e.register_function("data_server_dispatcher", data_server_dispatcher);
        e.register_function("data_client_server_requests", data_client_server_requests);
        e.register_function("data_client_server_dispatcher", data_client_server_dispatcher);
        e.register_function("data_client_requests", data_client_requests);
        e.register_function("data_client_dispatcher", data_client_dispatcher);
        // client(
        e.register_function("client", client);
        e.load_deployment(application_file);
    }

    e.run();
    // printf( " Simulation time %g sec. %g horas\n", sg4::Engine::get_clock(), sg4::Engine::get_clock()/3600);

    // for (i = 0; i < NUMBER_CLIENT_GROUPS; i++)
    // {
    //     printf(" Group %d. Average power: %f GFLOPS. Available: %0.1f%% Not available %0.1f%%\n", i, (double)SharedDatabase::_group_info[i].total_power / SharedDatabase::_group_info[i].n_clients / 1000000000.0, SharedDatabase::_group_info[i].total_available * 100.0 / (SharedDatabase::_group_info[i].total_available + SharedDatabase::_group_info[i].total_notavailable), (SharedDatabase::_group_info[i].total_notavailable) * 100.0 / (SharedDatabase::_group_info[i].total_available + SharedDatabase::_group_info[i].total_notavailable));
    //     _total_power += SharedDatabase::_group_info[i].total_power;
    //     _total_available += SharedDatabase::_group_info[i].total_available;
    //     _total_notavailable += SharedDatabase::_group_info[i].total_notavailable;
    // }

    // printf("\n Clients. Average power: %f GFLOPS. Available: %0.1f%% Not available %0.1f%%\n\n", (double)_total_power / NUMBER_CLIENTS / 1000000000.0, _total_available * 100.0 / (_total_available + _total_notavailable), (_total_notavailable) * 100.0 / (_total_available + _total_notavailable));

    t = (double)time(NULL) - t;    // Program time
    days = (int)(t / (24 * 3600)); // Calculate days
    t -= (days * 24 * 3600);
    hours = (int)(t / 3600); // Calculate hours
    t -= (hours * 3600);
    min = (int)(t / 60); // Calculate minutes
    t -= (min * 60);
    // printf(" Execution time:\n %d days %d hours %d min %d s\n\n", days, hours, min, (int)round(t));

} /* end_of_test_all */

void init_global_parameters(const parameters::Config &config)
{
    // set what earlier was defined values. I'm not proud of this repetive code
    g_number_projects = config.server_side.n_projects;
    config.set_with_sum_scheduling_servers(g_total_number_scheduling_servers);
    config.set_with_sum_data_servers(g_total_number_data_servers);
    config.set_with_data_client_servers(g_total_number_data_client_servers);
    g_number_client_groups = config.client_side.n_groups;
    // there is no support for multiple groups
    // todo: I start to doubt. Refs: I'm not sure how SharedDatabase::_pdatabase[i].nclients is used.
    // But for a project I don't necessarily need to know it. Let's say, I'm not sure if it
    // is supported, so we assume it doesn't
    g_total_number_clients = 0;
    g_total_number_data_clients = 0;
    for (auto &group : config.client_side.groups)
    {
        g_total_number_clients += group.n_clients;
        g_total_number_data_clients += group.ndata_clients;
    }
    g_total_number_ordinary_clients = (g_total_number_clients - g_total_number_data_clients);

    MAX_SIMULATED_TIME = config.simulation_time;
    WARM_UP_TIME = config.warm_up_time;

    maxtt = (MAX_SIMULATED_TIME + WARM_UP_TIME) * 3600;
    maxst = (MAX_SIMULATED_TIME) * 3600;
    maxwt = (WARM_UP_TIME) * 3600;
}

void init_measurements(const parameters::Config &config)
{
    for (int i = 0; i < g_number_projects; ++i)
    {
        auto &project_config = config.server_side.sprojects[i];
        std::string project_name = "Project" + std::to_string(project_config.snumber + 1);

        auto *dur = new thermometer::AggregateMeanSecondsToMinutes<double>();
        g_measure_task_duration_per_project[project_name] = dur;
    }
    auto *nav = new thermometer::AggregateMeanSecondsToMinutes<double>();
    g_measure_non_availability_duration = nav;
}

/* Main function */
int main(int argc, char *argv[])
{

    int j;
#if defined(__linux__)
    std::signal(SIGSEGV, on_segv);
#endif
    sg4::Engine e(&argc, argv);
    SharedDatabase::oclient_mutex = sg4::Mutex::create();
    SharedDatabase::dclient_nfinished_mutex = sg4::Mutex::create();
    // xbt_log_control_set("data_client.thresh:debug");
    // xbt_log_control_set("ker_engine.thresh:debug");

    // MSG_init(&argc, argv);

    if (argc != 4)
    {
        fprintf(stderr, "[BOINC_DEBUG] main: bad argc=%d (need platform deployment parameters.yaml)\n", argc);
        fflush(stderr);
        printf("Usage: %s PLATFORM_FILE DEPLOYMENT_FILE PARAMETERS_AS_YAML_FILE \n", argv[0]);
        printf("Example: %s platform.xml deloyment.xml parameters.yaml\n", argv[0]);
        exit(1);
    }

    parameters::Config config;
    try
    {
        config = parameters::read_from_file(argv[3]);
    }
    catch (const std::exception &ex)
    {
        fprintf(stderr, "[BOINC_DEBUG] main: read parameters failed file=%s: %s\n", argv[3], ex.what());
        fflush(stderr);
        throw;
    }
#if 0
    // check all parameters are read correctly
    std::cout << "execute_state_log_path: " << config.experiment_run.timeline.execute_state_log_path << std::endl;
    std::cout << "observable_clients: \n";
    for (auto observable_client : config.experiment_run.timeline.observable_clients)
    {
        std::cout << '\t' << observable_client << '\n';
    }
    std::cout << "seed: " << config.experiment_run.seed_for_deterministic_run.value_or(0) << std::endl;
#endif

    init_global_parameters(config);

    // set seeds of random generators
    if (config.experiment_run.seed_for_deterministic_run.has_value())
    {
        auto seed = *config.experiment_run.seed_for_deterministic_run;
        g_rndg = std::make_unique<boost::random::rand48>(seed);
        g_rndg_for_host_speed = std::make_unique<boost::random::rand48>(seed);
        g_rndg_for_disk_cap = std::make_unique<boost::random::rand48>(seed);
        g_rndg_for_data_client_avail = std::make_unique<boost::random::rand48>(seed);
        g_rndg_for_client_avail = std::make_unique<boost::random::rand48>(seed);
    }
    else
    {
        g_rndg = std::make_unique<boost::random::rand48>(time(0));
        g_rndg_for_host_speed = std::make_unique<boost::random::rand48>(time(0));
        g_rndg_for_disk_cap = std::make_unique<boost::random::rand48>(time(0));
        g_rndg_for_data_client_avail = std::make_unique<boost::random::rand48>(time(0));
        g_rndg_for_client_avail = std::make_unique<boost::random::rand48>(time(0));
    }

    // set the clients whose timeline we would like to save (periods when they were non-available, idle or busy)
    execution_state::Switcher::set_observable_clients(config.experiment_run.timeline.observable_clients);

    _total_power = 0;
    _total_available = 0;
    _total_notavailable = 0;
    SharedDatabase::_pdatabase.resize(g_number_projects);
    SharedDatabase::_sserver_info = new s_sserver_t[g_total_number_scheduling_servers];
    SharedDatabase::_dserver_info = new s_dserver_t[g_total_number_data_servers];
    SharedDatabase::_dcserver_info = new s_dcserver_t[g_total_number_data_client_servers];
    SharedDatabase::_dclient_info = std::vector<data_client>(g_total_number_data_clients);
    SharedDatabase::_group_info.resize(g_number_client_groups);

    init_measurements(config);

    for (int i = 0; i < g_number_projects; i++)
    {
        auto &project_config = config.server_side.sprojects[i];
        /* Project attributes */

        SharedDatabase::_pdatabase[i].nclients = g_total_number_clients;
        SharedDatabase::_pdatabase[i].ndata_clients = g_total_number_data_clients;
        SharedDatabase::_pdatabase[i].nordinary_clients = SharedDatabase::_pdatabase[i].nclients - SharedDatabase::_pdatabase[i].ndata_clients;
        SharedDatabase::_pdatabase[i].nscheduling_servers = project_config.nscheduling_servers;
        SharedDatabase::_pdatabase[i].scheduling_servers.resize((int)SharedDatabase::_pdatabase[i].nscheduling_servers);
        for (j = 0; j < SharedDatabase::_pdatabase[i].nscheduling_servers; j++)
            SharedDatabase::_pdatabase[i].scheduling_servers[j] = std::string(bprintf("s%" PRId32 "%" PRId32, i + 1, j));

        SharedDatabase::_pdatabase[i].ndata_client_servers = project_config.ndata_client_servers;
        SharedDatabase::_pdatabase[i].data_client_servers.resize((int)SharedDatabase::_pdatabase[i].ndata_client_servers);
        for (j = 0; j < SharedDatabase::_pdatabase[i].ndata_client_servers; j++)
        {
            SharedDatabase::_pdatabase[i].data_client_servers[j] = std::string(bprintf("t%" PRId32 "%" PRId32, i + 1, j));
        }
        SharedDatabase::_pdatabase[i].data_clients.resize(SharedDatabase::_pdatabase[i].ndata_clients);
        // for (j = 0; j < SharedDatabase::_pdatabase[i].ndata_clients; j++)
        //     SharedDatabase::_pdatabase[i].data_clients[j] = NULL;

        SharedDatabase::_pdatabase[i].nfinished_oclients = 0;
        SharedDatabase::_pdatabase[i].nfinished_dclients = 0;

        /* Work generator */

        SharedDatabase::_pdatabase[i].ncurrent_results = 0;
        SharedDatabase::_pdatabase[i].ncurrent_workunits = 0;
        SharedDatabase::_pdatabase[i].current_results;
        SharedDatabase::_pdatabase[i].r_mutex = sg4::Mutex::create();
        SharedDatabase::_pdatabase[i].ncurrent_error_results = 0;
        SharedDatabase::_pdatabase[i].current_error_results;
        SharedDatabase::_pdatabase[i].w_mutex = sg4::Mutex::create();
        SharedDatabase::_pdatabase[i].er_mutex = sg4::Mutex::create();
        SharedDatabase::_pdatabase[i].wg_empty = sg4::ConditionVariable::create();
        SharedDatabase::_pdatabase[i].wg_full = sg4::ConditionVariable::create();
        SharedDatabase::_pdatabase[i].wg_end = 0;
        SharedDatabase::_pdatabase[i].wg_dcs = 0;

        /* Validator */

        SharedDatabase::_pdatabase[i].ncurrent_validations = 0;
        SharedDatabase::_pdatabase[i].current_validations;
        SharedDatabase::_pdatabase[i].v_mutex = sg4::Mutex::create();
        SharedDatabase::_pdatabase[i].v_empty = sg4::ConditionVariable::create();
        SharedDatabase::_pdatabase[i].v_end = 0;

        /* Assimilator */

        SharedDatabase::_pdatabase[i].ncurrent_assimilations = 0;
        SharedDatabase::_pdatabase[i].current_assimilations;
        SharedDatabase::_pdatabase[i].a_mutex = sg4::Mutex::create();
        SharedDatabase::_pdatabase[i].a_empty = sg4::ConditionVariable::create();
        SharedDatabase::_pdatabase[i].a_end = 0;

        /* Data clients */

        SharedDatabase::_pdatabase[i].rfiles_mutex = sg4::Mutex::create();
        SharedDatabase::_pdatabase[i].dcrfiles_mutex = sg4::Mutex::create();
        SharedDatabase::_pdatabase[i].dsuploads_mutex = sg4::Mutex::create();
        SharedDatabase::_pdatabase[i].dcuploads_mutex = sg4::Mutex::create();

        /* Input files */

        SharedDatabase::_pdatabase[i].ninput_files = 0;
        // SharedDatabase::_pdatabase[i].input_files;
        // SharedDatabase::_pdatabase[i].i_mutex = sg4::Mutex::create();
        // SharedDatabase::_pdatabase[i].i_empty = sg4::ConditionVariable::create();
        // SharedDatabase::_pdatabase[i].i_full = sg4::ConditionVariable::create();

        /* File deleter */

        SharedDatabase::_pdatabase[i].ncurrent_deletions = 0;
        SharedDatabase::_pdatabase[i].current_deletions;

        /* Output files */

        // SharedDatabase::_pdatabase[i].noutput_files = 0;
        // SharedDatabase::_pdatabase[i].output_files;
        // SharedDatabase::_pdatabase[i].o_mutex = sg4::Mutex::create();
        // SharedDatabase::_pdatabase[i].o_empty = sg4::ConditionVariable::create();
        // SharedDatabase::_pdatabase[i].o_full = sg4::ConditionVariable::create();

        /* Synchronization */

        SharedDatabase::_pdatabase[i].ssrmutex = sg4::Mutex::create();
        SharedDatabase::_pdatabase[i].ssdmutex = sg4::Mutex::create();
        SharedDatabase::_pdatabase[i].dcmutex = sg4::Mutex::create();
        SharedDatabase::_pdatabase[i].barrier = sg4::Barrier::create(SharedDatabase::_pdatabase[i].nscheduling_servers + SharedDatabase::_pdatabase[i].ndata_client_servers + 4);
    }

    for (j = 0; j < g_total_number_scheduling_servers; j++)
    {
        SharedDatabase::_sserver_info[j].mutex = sg4::Mutex::create();
        SharedDatabase::_sserver_info[j].cond = sg4::ConditionVariable::create();
        SharedDatabase::_sserver_info[j].client_requests;
        SharedDatabase::_sserver_info[j].Nqueue = 0;
        SharedDatabase::_sserver_info[j].EmptyQueue = 0;
        SharedDatabase::_sserver_info[j].time_busy = 0;
    }

    for (j = 0; j < g_total_number_data_servers; j++)
    {
        SharedDatabase::_dserver_info[j].mutex = sg4::Mutex::create();
        SharedDatabase::_dserver_info[j].cond = sg4::ConditionVariable::create();
        SharedDatabase::_dserver_info[j].client_requests;
        SharedDatabase::_dserver_info[j].Nqueue = 0;
        SharedDatabase::_dserver_info[j].EmptyQueue = 0;
        SharedDatabase::_dserver_info[j].time_busy = 0;
    }

    for (j = 0; j < g_total_number_data_client_servers; j++)
    {
        SharedDatabase::_dcserver_info[j].mutex = sg4::Mutex::create();
        SharedDatabase::_dcserver_info[j].cond = sg4::ConditionVariable::create();
        SharedDatabase::_dcserver_info[j].client_requests;
        SharedDatabase::_dcserver_info[j].Nqueue = 0;
        SharedDatabase::_dcserver_info[j].EmptyQueue = 0;
        SharedDatabase::_dcserver_info[j].time_busy = 0;
    }

    for (j = 0; j < g_total_number_data_clients; j++)
    {
        SharedDatabase::_dclient_info[j].mutex = sg4::Mutex::create();
        SharedDatabase::_dclient_info[j].ask_for_files_mutex = sg4::Mutex::create();
        SharedDatabase::_dclient_info[j].cond = sg4::ConditionVariable::create();
        SharedDatabase::_dclient_info[j].client_requests;
        SharedDatabase::_dclient_info[j].Nqueue = 0;
        SharedDatabase::_dclient_info[j].EmptyQueue = 0;
        SharedDatabase::_dclient_info[j].time_busy = 0;
        SharedDatabase::_dclient_info[j].finish = 0;
    }

    for (j = 0; j < g_number_client_groups; j++)
    {
        SharedDatabase::_group_info[j].total_power = 0;
        SharedDatabase::_group_info[j].total_available = 0;
        SharedDatabase::_group_info[j].total_notavailable = 0;
        SharedDatabase::_group_info[j].on = 0;
        SharedDatabase::_group_info[j].mutex = sg4::Mutex::create();
        SharedDatabase::_group_info[j].cond = sg4::ConditionVariable::create();
        
        for (size_t i = 0; i < config.client_side.groups[j].gprojects.size(); ++i) {
            GroupProject group_proj;
            group_proj.lsbw = config.client_side.groups[j].gprojects[i].lsbw;
            group_proj.lslatency = config.client_side.groups[j].gprojects[i].lslatency;
            group_proj.ldbw = config.client_side.groups[j].gprojects[i].ldbw;
            group_proj.ldlatency = config.client_side.groups[j].gprojects[i].ldlatency;
            SharedDatabase::_group_info[j].gprojects.push_back(group_proj);
        }
    }

    test_all(argc, argv, e);

    // save metrics and timelines that were recorded during the simulation
    auto &experiment_run = config.experiment_run;
    if (!experiment_run.timeline.execute_state_log_path.empty())
    {
        execution_state::Switcher::save_to_file(experiment_run.timeline.execute_state_log_path);
    }
    if (!experiment_run.measures.save_filepath.empty())
    {
        std::string metric_save_file = experiment_run.measures.save_filepath;
        if (experiment_run.measures.clean_before_write)
        {
            std::ofstream file(metric_save_file);
        }
        for (int i = 0; i < g_number_projects; ++i)
        {
            auto &project_config = config.server_side.sprojects[i];
            std::string project_name = "Project" + std::to_string(project_config.snumber + 1);

            g_measure_task_duration_per_project[project_name]->save_series_to_file(
                project_config.name + " project, average task execution (minutes)",
                metric_save_file);
        }
        g_measure_non_availability_duration->save_series_to_file("non-availability period (minutes)", metric_save_file);
    }
}
