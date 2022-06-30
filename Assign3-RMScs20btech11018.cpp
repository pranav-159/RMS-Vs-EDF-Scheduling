#include <iostream>
#include <fstream>
#include <exception>
#include <queue>
#include <map>
#include <cstdlib>
#include <set>
#include <vector>
using namespace std;

struct proc_vars_t {
    int pid;
    float priority;
    int process_time;
    int period;
    int repetations;
    int success_exits;
    int failure_exits;
};

typedef struct proc_vars_t proc_vars;

enum class event_type_t {ARRIVAL,DEADLINE,};
enum class state_t {READY,RUNNING,WAITING,TERMINATED};


struct event_token {
    event_type_t type;
    int time;
    proc_vars* process;
};
typedef struct event_token event_token_t;

struct processing_token {
    proc_vars* process;
    int runned_time;
    int arrival_time;
    int waiting_time;
};

typedef struct processing_token processing_token_t;

std::fstream logfile;

int successfully_terminated_processes1 = 0;
std::map<int,long long> waiting_time_sum;
std::map<int,int> pid_vs_index;

/**
 * @brief  defining state variables of DES
 *
 */
processing_token_t running_proc;
int burst_start_time;
int estimated_termination_time ;
int deadline;
int simulation_clock = 0;
event_token_t curr_event;

// constructing ready queue for DES engine
using my_container_ready_t = std::vector<processing_token_t>;

auto my_comp_ready = [](const processing_token_t &a, const processing_token_t &b)
{ return a.process->priority < b.process->priority; };

std::priority_queue<processing_token_t, my_container_ready_t, decltype(my_comp_ready)> ready_queue{my_comp_ready};

std::map<int,state_t> state_table;

std::map<int,float> average_waiting_time_table;



/**
 * @brief event follow up functions or triggers as well as their helper functions 
 * 
 */
void arrival_event_func(const event_token_t&);
void start_execution_func(const processing_token_t&);
void terminate_execution_func();
void deadline_event_func(const event_token_t&);
void preemption_func(processing_token_t&);
inline void update_runtime();

void average_waiting_time_builder(const processing_token_t&);


int main(int argc, char const *argv[])
{
    std::ifstream ifile;
    ifile.open("inp-params.txt");

    logfile.open("RMS-Log.txt", std::fstream::out);

    //input parameters
    if(!ifile.is_open())
    {
        cerr << "error in opening the file\n";
    }
    int no_of_processes;
    ifile >> no_of_processes;

    proc_vars processes[no_of_processes];

    for(int i = 0;i<no_of_processes;i++)
    {
        ifile >> processes[i].pid;
        ifile >> processes[i].process_time;
        ifile >> processes[i].period;
        ifile >> processes[i].repetations;

        processes[i].priority = 1/(float)processes[i].period;
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

    auto my_comp = [](const event_token_t& a,const event_token_t& b)
                        {   if(a.time == b.time){ 
                                if(a.process->priority == b.process->priority)
                                    return (int)a.type < (int)b.type;
                                else return a.process->priority < b.process->priority;
                            }   
                            else return a.time > b.time;};

    std::priority_queue<event_token_t,my_container_t,decltype(my_comp)> event_queue {my_comp};

    for(int i=0; i<no_of_processes; i++)
    {

        logfile << "Process P" << processes[i].pid << ":"
                << " processing time=" << processes[i].process_time
                << "; deadline:" << processes[i].period << "; period:" << processes[i].period
                << " joined the system at time " << simulation_clock << "\n";
                
        int no_of_repeats = processes[i].repetations;

        int time = 0;//process first arrival time 
        while(no_of_repeats--)
        {
            event_token_t arrival_event;
            arrival_event.type = event_type_t::ARRIVAL;
            arrival_event.time = time;
            arrival_event.process = &processes[i];

            event_token_t deadline_event;
            deadline_event.type = event_type_t::DEADLINE;
            deadline_event.time = time+processes[i].period;
            deadline_event.process = &processes[i];

            event_queue.push(arrival_event);
            event_queue.push(deadline_event);

            time+=processes[i].period;
        }
    }

    /**
     * @brief initiate running process 
     * 
     */
    running_proc.process = nullptr;
    running_proc.runned_time = 0;
    running_proc.arrival_time = 0;
    running_proc.waiting_time = 0;
    /**
     * @brief Construct a simulation engine
     * 
     */
    while(!event_queue.empty())
    {
        curr_event = event_queue.top();
        event_queue.pop();
        simulation_clock = curr_event.time;


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

                if (proc.arrival_time + proc.process->period <= simulation_clock)
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
            simulation_clock = curr_event.time;
        }

        if(curr_event.type == event_type_t::ARRIVAL)
        {
            arrival_event_func(curr_event);
        }
        else if(curr_event.type == event_type_t::DEADLINE)
        {
            deadline_event_func(curr_event);
        }
    }
    logfile.close();
    
    /**
     * @brief stat variables 
     * 
     */
    int total_no_of_processes = 0;
    int successfully_terminated_processes = 0;    
    int processes_missed_deadline = 0;

    for(int i = 0;i < no_of_processes;i++)
    {
        total_no_of_processes += processes[i].repetations;
        successfully_terminated_processes += processes[i].success_exits;

    }
    processes_missed_deadline = total_no_of_processes - successfully_terminated_processes;

    std::fstream statfile("RMS-stats.txt",std::fstream::out);

    statfile << "number of processes that came into the system = " << total_no_of_processes << "\n";
    statfile << "number of processes that successfully completed = "<<successfully_terminated_processes << "\n";
    statfile << "number of processes that missed their deadlines = "<<processes_missed_deadline <<"\n";
    statfile << "process : average waiting time\n";

    // //correct waiting times 
    // for(int i = 0;i<no_of_processes;i++)
    // {
    //     average_waiting_time_table[processes[i].pid] *= (processes[i].success_exits / (float)processes[i].repetations);
    //     average_waiting_time_table[processes[i].pid]+=(processes[i].period*(1-(processes[i].success_exits/(float)processes[i].repetations)));
    // }

    auto wait_times_iter = waiting_time_sum.begin();
    for(int i = 0;i<no_of_processes;i++)
    {
        int instants = processes[pid_vs_index[wait_times_iter->first]].repetations;
        float avg_time = (float)(wait_times_iter->second)/instants;
        statfile << wait_times_iter->first << " : "  << avg_time << "\n";
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
    if (arrival_event.type != event_type_t::ARRIVAL)
    {
        cerr << "event type mismatch error\n";
        exit(-1);
    }

    processing_token_t proc_tok;
    proc_tok.process = arrival_event.process;
    proc_tok.runned_time = 0;
    proc_tok.arrival_time = simulation_clock;
    proc_tok.waiting_time = 0;
    
    if (running_proc.process == nullptr)
    {
        if (simulation_clock != 0)
        {
            logfile << "cpu is idle till time " << simulation_clock-1 << "\n";
        }
        start_execution_func(proc_tok);
        state_table.insert(std::make_pair(proc_tok.process->pid, state_t::RUNNING));
    }
    else if (running_proc.process->priority >= proc_tok.process->priority)
    {
        ready_queue.push(proc_tok);
        state_table.insert(std::make_pair(proc_tok.process->pid, state_t::READY));
    }
    else
    {
        preemption_func(proc_tok);
        state_table.insert(std::make_pair(proc_tok.process->pid, state_t::RUNNING));
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


    proc_vars* prev_proc = running_proc.process;

    running_proc = proc;

    state_table[proc.process->pid] = state_t::RUNNING;

    burst_start_time = simulation_clock;
    estimated_termination_time = burst_start_time + running_proc.process->process_time - running_proc.runned_time;
    deadline = running_proc.arrival_time + running_proc.process->period;


    if(prev_proc != nullptr)
        simulation_clock++;

    //logging
    if(running_proc.runned_time == 0){
        logfile << "Process P"<< running_proc.process->pid << 
        " starts execution at time "<<simulation_clock <<"\n";
    }
    else
    {
        logfile << "Process P" << running_proc.process->pid <<
         " resumes execution at time " << simulation_clock << "\n";
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
    state_table[running_proc.process->pid] = state_t::TERMINATED;

    running_proc.waiting_time = simulation_clock - running_proc.arrival_time - running_proc.runned_time;

    running_proc.process->success_exits += 1;

    waiting_time_sum[running_proc.process->pid] += running_proc.waiting_time;

    average_waiting_time_builder(running_proc);

    //logging
    logfile << "Process P" << running_proc.process->pid <<
     " finishes execution at time " << simulation_clock << "\n";
}

/**
 * @brief checks things about how process performed 
 * 
 * @param deadline_event token of process to be checked
 */
void deadline_event_func(const event_token_t &deadline_event)
{
    if (deadline_event.type != event_type_t::DEADLINE)
    {
        cerr << "event type mismatch error\n";
        exit(-1);
    }

    if (state_table[deadline_event.process->pid] == state_t::TERMINATED)
    {
        successfully_terminated_processes1++;
    }
    

    state_table.erase(deadline_event.process->pid);
}
/**
 * @brief performs necessary tasks during preemption of running process with
 * the new process
 * 
 * @param new_process token of process which preempts the running_process 
 */
void preemption_func(processing_token_t &new_process)
{
    if (new_process.process->priority <= running_proc.process->priority)
    {
        cerr << "preemption error:new process priority is not higher than running process\n";
        exit(-1);
    }
    update_runtime();
    ready_queue.push(running_proc);

    //logging
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
void average_waiting_time_builder(const processing_token_t & proc)
{
    if(average_waiting_time_table.count(proc.process->pid) == 0)
    {
        average_waiting_time_table.insert(std::make_pair(proc.process->pid,proc.waiting_time));
    }
    else
    {
        int instants = running_proc.process->success_exits + running_proc.process->failure_exits;
        average_waiting_time_table[proc.process->pid] =
            ((instants - 1) * average_waiting_time_table[proc.process->pid]/instants)
            +(proc.waiting_time/(float)instants);
    }
    
}