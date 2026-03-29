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

    void worker(int q_idx, int id)
    {
        while (true)
        {
            function<void()> task;
            {
                unique_lock<mutex> lock(q[q_idx].mtx);
                auto start_wait = steady_clock::now();

                q[q_idx].cv.wait(lock, [&]()
                                 { return stop_all || (finish_tasks && q[q_idx].tasks.empty()) ||
                                          (!pause_pool && !q[q_idx].tasks.empty()); });

                auto end_wait = steady_clock::now();
                q[q_idx].total_wait_time += duration<double>(end_wait - start_wait).count();
                q[q_idx].wait_count++;

                if (stop_all || (finish_tasks && q[q_idx].tasks.empty()))
                {
                    lock_guard<mutex> l(cout_mtx);
                    cout << "[Потік " << q_idx << "-" << id << "] Завершено.\n";
                    return;
                }

                task = q[q_idx].tasks.front();
                q[q_idx].tasks.pop();
            }

            auto start_exec = steady_clock::now();
            task();
            auto end_exec = steady_clock::now();

            {
                lock_guard<mutex> lock(q[q_idx].mtx);
                q[q_idx].total_exec_time += duration<double>(end_exec - start_exec).count();
                q[q_idx].completed_tasks++;
            }
        }
    }

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

    void setPause(bool p)
    {
        pause_pool = p;
        if (!p)
        {
            q[0].cv.notify_all();
            q[1].cv.notify_all();
        }
    }

    void shutdown(bool graceful)
    {
        stop_all = !graceful;
        finish_tasks = graceful;

        for (int i = 0; i < 2; i++)
        {
            q[i].cv.notify_all();
            for (auto &w : q[i].workers)
            {
                if (w.joinable())
                    w.join();
            }
        }
    }
};
