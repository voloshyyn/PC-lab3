#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <Windows.h>

using namespace std;
using namespace std::chrono;

mutex cout_mtx;

class SimpleThreadPool
{
    struct QueueData
    {
        queue<function<void()>> tasks;
        mutex mtx;
        condition_variable cv;
        vector<thread> workers;

        double total_wait_time = 0;
        int wait_count = 0;
        double total_exec_time = 0;
        int completed_tasks = 0;
        int queue_len_sum = 0;
        int queue_samples = 0;
    };

    QueueData q[2];
    bool stop_all = false;
    bool finish_tasks = false;
    bool pause_pool = false;

public:
    SimpleThreadPool()
    {
        for (int i = 0; i < 2; i++)
        {
            for (int j = 0; j < 2; j++)
            {
                q[i].workers.push_back(thread(&SimpleThreadPool::worker, this, i, j));
            }
        }
    }
}