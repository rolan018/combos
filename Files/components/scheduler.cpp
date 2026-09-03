#include "scheduler.hpp"

#include <iostream>
#include <simgrid/s4u.hpp>
#include <math.h>
#include <inttypes.h>
#include <vector>

#include "types.hpp"
#include "shared.hpp"
#include "scheduler_matching.hpp"

/**
 * @brief
 * takes client's request which is
 * - reply: send to validator results of computantions that present in msg->content
 * - request: send computations to client: take [result] item from [project.current_results]
 *            and create [result->number_tasks] tasks for it, with [workunit] equals to
 *            [result->workunit->number]
 */

namespace sg4 = simgrid::s4u;

namespace {

void dispatch_client_replies(ProjectDatabaseValue &project, const std::vector<SchedulingServerMessage *> &replies)
{
    for (auto *msg : replies)
    {
        project.v_mutex->lock();
        project.current_validations.push(reinterpret_cast<reply_t>(msg->content));
        project.ncurrent_validations++;
        project.v_empty->notify_all();
        project.v_mutex->unlock();
        delete msg;
    }
}

void dispatch_work_requests(int project_number, ProjectDatabaseValue &project,
                            const std::vector<SchedulingServerMessage *> &requests, sg4::ActivitySet &sscomm)
{
    if (requests.empty())
        return;

    std::vector<request_t> reqs;
    reqs.reserve(requests.size());
    for (auto *msg : requests)
        reqs.push_back(reinterpret_cast<request_t>(msg->content));

    std::vector<std::vector<AssignedResult *>> assignments;
    project.r_mutex->lock();
    if (project.ncurrent_results == 0)
        assignments.assign(requests.size(), {});
    else
        assignments = scheduling_assign_work_batch(project_number, reqs);
    project.r_mutex->unlock();

    for (size_t i = 0; i < requests.size(); ++i)
    {
        auto *msg = requests[i];
        auto *req = reinterpret_cast<request_t>(msg->content);

        ResultBag *msg_pack = new ResultBag{.results = assignments[i]};
        int msg_sz = 0;
        for (auto &task : assignments[i])
            msg_sz += KB * task->ninput_files;

        auto ans_mailbox = sg4::Mailbox::by_name(req->answer_mailbox);
        auto comm = ans_mailbox->put_async(msg_pack, msg_sz);

        delete_completed_communications(sscomm);
        sscomm.push(comm);

        delete req;
        delete msg;
    }
}

} // namespace

/*
 *	Scheduling server requests function
 */
int scheduling_server_requests(int argc, char *argv[])
{
    // Check number of arguments
    if (argc != 3)
    {
        std::cerr << "Invalid number of parameter in scheduling_server_requests()" << std::endl;
        return 0;
    }


    // Init boinc server
    int32_t project_number = (int32_t)atoi(argv[1]);           // Project number
    int32_t scheduling_server_number = (int32_t)atoi(argv[2]); // Scheduling server number

    ProjectDatabaseValue &project = SharedDatabase::_pdatabase[project_number];        // Database
    sserver_t sserver_info = &SharedDatabase::_sserver_info[scheduling_server_number]; // Scheduling server info

    sserver_info->server_name = sg4::this_actor::get_host()->get_name(); // Server name

    // Wait until database is ready
    project.barrier->wait();

    sg4::Mailbox *self_mailbox = sg4::Mailbox::by_name(sserver_info->server_name);
    while (1)
    {
        // Receive message
        auto msg = self_mailbox->get<SchedulingServerMessage>();

        // Termination message
        if (msg->type == TERMINATION)
        {
            delete msg;
            break;
        }
        // Client answer with execution results
        else if (msg->type == REPLY)
        {
            project.ssrmutex->lock();
            project.nmessages_received++;
            project.nresults_received++;
            project.ssrmutex->unlock();
        }
        // Client work request
        else
        {
            project.ssrmutex->lock();
            project.nmessages_received++;
            project.nwork_requests++;
            project.ssrmutex->unlock();
        }

        // Insert request into queue
        sserver_info->mutex->lock();
        sserver_info->Nqueue++;
        sserver_info->client_requests.push(msg);

        // If queue is not empty, wake up dispatcher process
        if (sserver_info->Nqueue > 0)
            sserver_info->cond->notify_all();
        sserver_info->mutex->unlock();

        msg = nullptr;
    }

    // Terminate dispatcher execution
    sserver_info->mutex->lock();
    sserver_info->EmptyQueue = 1;
    sserver_info->cond->notify_all();
    sserver_info->mutex->unlock();

    return 0;
}

/*
 *	Scheduling server dispatcher function
 */
int scheduling_server_dispatcher(int argc, char *argv[])
{
    dsmessage_t work = nullptr;       // Termination message
    sserver_t sserver_info = nullptr; // Scheduling server info
    int32_t i, project_number;        // Index, project number
    int32_t scheduling_server_number; // Scheduling_server_number
    double t0, t1;                    // Time measure

    sg4::ActivitySet _sscomm; // Asynchro communications storage (scheduling server with client)

    // Check number of arguments
    if (argc != 3)
    {
        printf("Invalid number of parameter in scheduling_server_dispatcher()\n");
        return 0;
    }

    // Init boinc dispatcher
    t0 = t1 = 0.0;
    project_number = (int32_t)atoi(argv[1]);           // Project number
    scheduling_server_number = (int32_t)atoi(argv[2]); // Scheduling server number

    ProjectDatabaseValue &project = SharedDatabase::_pdatabase[project_number]; // Server info
    sserver_info = &SharedDatabase::_sserver_info[scheduling_server_number];    // Scheduling server info

    while (1)
    {
        std::vector<SchedulingServerMessage *> pending_replies;
        std::vector<SchedulingServerMessage *> pending_requests;

        {
            std::unique_lock lock(*sserver_info->mutex);

            while ((sserver_info->Nqueue == 0) && (sserver_info->EmptyQueue == 0))
                sserver_info->cond->wait(lock);

            if ((sserver_info->EmptyQueue == 1) && sserver_info->Nqueue == 0)
                break;

            t0 = sg4::Engine::get_clock();

            while (sserver_info->Nqueue > 0)
            {
                auto *msg = sserver_info->client_requests.front();
                sserver_info->client_requests.pop();
                sserver_info->Nqueue--;

                if (msg->type == REPLY)
                    pending_replies.push_back(msg);
                else
                    pending_requests.push_back(msg);
            }
        }

        compute_server(36000000);

        dispatch_client_replies(project, pending_replies);
        dispatch_work_requests(project_number, project, pending_requests, _sscomm);

        t1 = sg4::Engine::get_clock();
        if (t0 < maxtt)
            sserver_info->time_busy += (t1 - t0);
    }

    // Wait until all scheduling servers finish
    project.ssdmutex->lock();
    project.nfinished_scheduling_servers++;
    project.ssdmutex->unlock();

    // Check if it is the last scheduling server
    if (project.nfinished_scheduling_servers == project.nscheduling_servers)
    {
        // Send termination message to data servers
        for (i = 0; i < project.ndata_servers; i++)
        {
            // Create termination message
            work = new s_dsmessage_t();

            // Group power = -1 indicates it is a termination message
            work->type = TERMINATION;

            // Send message
            sg4::Mailbox::by_name(project.data_servers[i])->put(work, KB);
        }
        // Free
        project.data_servers.clear();

        // Finish project back-end
        project.wg_end = 1;
        project.v_end = 1;
        project.a_end = 1;
        project.wg_full->notify_all();
        project.v_empty->notify_all();
        project.a_empty->notify_all();
    }

    _sscomm.wait_all();

    return 0;
}
