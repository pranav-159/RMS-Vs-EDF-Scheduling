#include <iostream>
#include <fstream>
#include <exception>
#include <queue>
#include <map>
#include <cstdlib>
#include <set>
#include <vector>
using namespace std;

struct proc_vars_t
{
    int pid;
    int process_time;
    int period;
    int repetations;
    int success_exits;
    int failure_exits;
};

typedef struct proc_vars_t proc_vars;

struct event_token
{
    int time;
    proc_vars *process;
};
typedef struct event_token event_token_t;

struct processing_token
{
    proc_vars *process;
    int runned_time;
    int arrival_time;
    int deadline;
    int waiting_time;
};

typedef struct processing_token processing_token_t;

std::fstream logfile;

std::map<int,long long> waiting_time_sum;
std::map<int,int> pid_vs_index;

/**
 * @brief  defining state variables of DES
 *
 */
processing_token_t running_proc;
int burst_start_time;
int estimated_termination_time;
int deadline;
int simulation_clock = 0;
event_token_t curr_event;

// constructing ready queue for DES engine
using my_container_ready_t = std::vector<processing_token_t>;

auto my_comp_ready = [](const processing_token_t &a, const processing_token_t &b)
{ if(a.deadline == b.deadline)
    return (a.process->process_time-a.runned_time) > (a.process->process_time-a.runned_time);
    return a.deadline > b.deadline; };

std::priority_queue<processing_token_t, my_container_ready_t, decltype(my_comp_ready)> ready_queue{my_comp_ready};

std::map<int, float> average_waiting_time_table;

/**
 * @brief event follow up functions or triggers as well as their helper functions
 *
 */
void arrival_event_func(const event_token_t &);
void start_execution_func(const processing_token_t &);
void terminate_execution_func();
void preemption_func(processing_token_t &);
inline void update_runtime();

void average_waiting_time_builder(const processing_token_t &);




int main(int argc, char const *argv[])
{
    std::ifstream ifile;
    ifile.open("inp-params.txt");

    logfile.open("EDF-Log.txt", std::fstream::out);

    /**
     * @brief taking input from inputfile 
     * 
     */
    if (!ifile.is_open())
    {
        cerr << "error in opening the file\n";
    }
    int no_of_processes;
    ifile >> no_of_processes;

    proc_vars processes[no_of_processes];

    for (int i = 0; i < no_of_processes; i++)
    {
        ifile >> processes[i].pid;
        ifile >> processes[i].process_time;
        ifile >> processes[i].period;
        ifile >> processes[i].repetations;
        processes[i].success_exits = 0;
        processes[i].failure_exits = 0;

        waiting_time_sum.insert(std::make_pair(processes[i].pid,0));
        pid_vs_index.insert(std::make_pair(processes[i].pid,i));
    }
    ifile.close();

    
    /**
     * @brief constructing event queue for DES
     *
     */
    using my_container_t = std::vector<event_token_t>;

    auto my_comp = [](const event_token_t &a, const event_token_t &b)
                           {if(a.time == b.time){ 
                               if(a.process->period == b.process->period)
                                    return a.process->process_time > b.process->process_time;
                               else return a.process->period > b.process->period;
                            }   
                            else return a.time > b.time; };

    std::priority_queue<event_token_t, my_container_t, decltype(my_comp)> event_queue{my_comp};

    for (int i = 0; i < no_of_processes; i++)
    {

        logfile << "Process P" << processes[i].pid << ":"
                << " processing time=" << processes[i].process_time
                << "; deadline:" << processes[i].period << "; period:" << processes[i].period
                << " joined the system at time " << simulation_clock << "\n";

        int no_of_repeats = processes[i].repetations;

        int time = 0; // process first arrival time
        while (no_of_repeats--)
        {
            event_token_t arrival_event;

            arrival_event.time = time;
            arrival_event.process = &processes[i];

            event_queue.push(arrival_event);

            time += processes[i].period;
        }
    }
    /**
     * @brief initiate running process
     *
     */
    running_proc.process = nullptr;
    running_proc.runned_time = 0;
    /**
     * @brief Construct a simulation engine
     *
     */
    while (!event_queue.empty() || running_proc.process != nullptr)
    {
        if(!event_queue.empty())
        {
            curr_event = event_queue.top();
            event_queue.pop();
            simulation_clock = curr_event.time;
        }
        else
        {
            curr_event.process = nullptr;
            simulation_clock = deadline;
        }
        int prev_deadline = deadline;
        
        /**
         * @brief checks whether the current process is terminated and peform
         * necessary actions.
         *
         */
        while ((simulation_clock >= deadline || simulation_clock >= estimated_termination_time) &&
               running_proc.process != nullptr)
        {
            if (deadline < estimated_termination_time)
            {
                simulation_clock = deadline;
                update_runtime();
                running_proc.process->failure_exits++;
                running_proc.waiting_time = running_proc.process->period - running_proc.runned_time;
                waiting_time_sum[running_proc.process->pid] += running_proc.waiting_time;
                average_waiting_time_builder(running_proc);
                // logging
                logfile << "Process P" << running_proc.process->pid << " reached its deadline. Remaining processing time:"
                        << (running_proc.process->process_time - running_proc.runned_time) << "\n";
            }
            else
            {
                simulation_clock = estimated_termination_time;
                terminate_execution_func();
            }

            bool runned = false;
            while (!ready_queue.empty())
            {
                processing_token_t proc = ready_queue.top();

                if( proc.deadline <= simulation_clock)
                {
                    proc.process->failure_exits++;
                    proc.waiting_time = proc.process->period - proc.runned_time;
                    waiting_time_sum[proc.process->pid] += proc.waiting_time;
                    average_waiting_time_builder(proc);
                    ready_queue.pop();
                    continue;
                }
                start_execution_func(proc);
                ready_queue.pop();
                runned = true;
                break;
            }
            if (!runned)
            {
                running_proc.process = nullptr;
                running_proc.runned_time = 0;
            }
            if(curr_event.process != nullptr)
                simulation_clock = curr_event.time;
            else simulation_clock = prev_deadline;

            if (simulation_clock == estimated_termination_time)
                break;
        }
        if(curr_event.process != nullptr)
            arrival_event_func(curr_event);
    }
    // logfile.close();

    /**
     * @brief stat variables
     *
     */
    int total_no_of_processes = 0;
    int successfully_terminated_processes = 0;
    int processes_missed_deadline = 0;

    for (int i = 0; i < no_of_processes; i++)
    {
        total_no_of_processes += processes[i].repetations;
        successfully_terminated_processes += processes[i].success_exits;
    }
    processes_missed_deadline = total_no_of_processes - successfully_terminated_processes;

    std::fstream statfile("EDF-stats.txt", std::fstream::out);

    statfile << "number of processes that came into the system = " << total_no_of_processes << "\n";
    statfile << "number of processes that successfully completed = " << successfully_terminated_processes << "\n";
    statfile << "number of processes that missed their deadlines = " << processes_missed_deadline << "\n";
    statfile << "process : average waiting time\n";

    // correct waiting times
    // for (int i = 0; i < no_of_processes; i++)
    // {
    //     average_waiting_time_table[processes[i].pid] *= (processes[i].success_exits / (float)processes[i].repetations);
    //     average_waiting_time_table[processes[i].pid] += (processes[i].period * (1 - (processes[i].success_exits / (float)processes[i].repetations)));
    // }

    auto wait_times_iter = waiting_time_sum.begin();
    while (no_of_processes--)
    {
        statfile << wait_times_iter->first << " : "
         << (float)(wait_times_iter->second)/(processes[pid_vs_index[wait_times_iter->first]].repetations) << "\n";
        wait_times_iter++;
    }
    statfile.close();

    return 0;
}
/**
 * @brief processing token is build from arrival_event and pushed into ready queue
 *
 * @param arrival_event event_token_t of the event arrival
 */
void arrival_event_func(const event_token_t &arrival_event)
{

    processing_token_t proc_tok;
    proc_tok.process = arrival_event.process;
    proc_tok.runned_time = 0;
    proc_tok.arrival_time = simulation_clock;
    proc_tok.deadline = simulation_clock + proc_tok.process->period;
    proc_tok.waiting_time = 0;

    if (running_proc.process == nullptr)
    {
        if (simulation_clock != 0)
        {
            logfile << "cpu is idle till time " << simulation_clock - 1 << "\n";
        }
        start_execution_func(proc_tok);
    }
    else if (running_proc.deadline <= proc_tok.deadline)
    {
        ready_queue.push(proc_tok);
    }
    else
    {
        preemption_func(proc_tok);
    }

    return;
}

/**
 * @brief performs necessary actions when the process is started execution
 *
 * @param proc token of process to be executed.
 */
void start_execution_func(const processing_token_t &proc)
{

    proc_vars *prev_proc = running_proc.process;

    running_proc = proc;

    burst_start_time = simulation_clock;
    estimated_termination_time = burst_start_time + running_proc.process->process_time - running_proc.runned_time;
    deadline = running_proc.arrival_time + running_proc.process ->period;

    if (prev_proc != nullptr)
        simulation_clock++;

    // logging
    if (running_proc.runned_time == 0)
    {
        logfile << "Process P" << running_proc.process->pid << " starts execution at time " << simulation_clock << "\n";
    }
    else
    {
        logfile << "Process P" << running_proc.process->pid << " resumes execution at time " << simulation_clock << "\n";
    }
}
/**
 * @brief performs necessary when the process is terminated
 *
 */
void terminate_execution_func()
{

    update_runtime();
    if (running_proc.runned_time != running_proc.process->process_time)
    {
        cerr << "termination error::total time not finished\n";
        exit(-1);
    }

    running_proc.waiting_time = simulation_clock - running_proc.arrival_time - running_proc.runned_time;

    running_proc.process->success_exits += 1;

    waiting_time_sum[running_proc.process->pid] += running_proc.waiting_time;

    average_waiting_time_builder(running_proc);

    // logging
    logfile << "Process P" << running_proc.process->pid << " finishes execution at time " << simulation_clock << "\n";
}

/**
 * @brief performs necessary tasks during preemption of running process with
 * the new process
 *
 * @param new_process token of process which preempts the running_process
 */
void preemption_func(processing_token_t &new_process)
{
    if (new_process.deadline >= running_proc.deadline)
    {
        cerr << "preemption error:new process priority is not higher than running process\n";
        exit(-1);
    }
    update_runtime();
    ready_queue.push(running_proc);

    // logging
    logfile << "Process P" << running_proc.process->pid
            << " is preempted by Process P" << new_process.process->pid << " at time " << simulation_clock
            << ". Remaining processing time:" << (running_proc.process->process_time - running_proc.runned_time) << "\n";

    start_execution_func(new_process);
}

/**
 * @brief updates runned_time of the running_process
 *
 */
inline void update_runtime()
{
    running_proc.runned_time += simulation_clock - burst_start_time;
    burst_start_time = simulation_clock;
}
/**
 * @brief calculates average waiting times and stores them in avg_waiting_time_table
 *
 * @param proc token of process whose average waiting time is calculated
 *
 */
void average_waiting_time_builder(const processing_token_t &proc)
{
    if (average_waiting_time_table.count(proc.process->pid) == 0)
    {
        average_waiting_time_table.insert(std::make_pair(proc.process->pid, proc.waiting_time));
    }
    else
    {
        int instants = running_proc.process->success_exits;
        average_waiting_time_table[proc.process->pid] =
            ((instants - 1) * average_waiting_time_table[proc.process->pid] / instants) + (proc.waiting_time / (float)instants);
    }
}