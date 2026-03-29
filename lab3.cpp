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

    void addTask(function<void()> task)
    {
        int q_idx = rand() % 2;

        {
            lock_guard<mutex> lock(q[q_idx].mtx);
            q[q_idx].tasks.push(task);

            q[q_idx].queue_len_sum += q[q_idx].tasks.size();
            q[q_idx].queue_samples++;
        }
        q[q_idx].cv.notify_one();
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

    void printMetrics()
    {
        cout << "\n=== РЕЗУЛЬТАТИ ТЕСТУВАННЯ ===\n";
        cout << "Всього створено потоків: 4\n\n";
        for (int i = 0; i < 2; i++)
        {
            double avg_wait = q[i].wait_count ? q[i].total_wait_time / q[i].wait_count : 0;
            double avg_exec = q[i].completed_tasks ? q[i].total_exec_time / q[i].completed_tasks : 0;
            double avg_len = q[i].queue_samples ? (double)q[i].queue_len_sum / q[i].queue_samples : 0;

            cout << "--- Черга " << i << " ---\n"
                 << "  Сер. час очікування: " << avg_wait << " с\n"
                 << "  Сер. довжина черги: " << avg_len << " задач\n"
                 << "  Сер. час виконання: " << avg_exec << " с\n"
                 << "  Виконано задач: " << q[i].completed_tasks << "\n\n";
        }
    }

    ~SimpleThreadPool() { shutdown(false); }
};

void producer(int id, SimpleThreadPool &pool, bool &active)
{
    int task_id = 1;
    while (active)
    {
        {
            lock_guard<mutex> l(cout_mtx);
            cout << "[Продюсер " << id << "] ЗГЕНЕРУВАВ задачу " << task_id << " і додає в пул.\n";
        }

        pool.addTask([id, task_id]()
                     {
            int exec_time = 4 + rand() % 12;
            {
                lock_guard<mutex> l(cout_mtx);
                cout << "   -> [Пул] Задача " << task_id << " від Продюсера " << id << " ПОЧАЛА виконуватись (" << exec_time << "с).\n";
            }

            this_thread::sleep_for(seconds(exec_time));

            {
                lock_guard<mutex> l(cout_mtx);
                cout << "   <- [Пул] Задача " << task_id << " від Продюсера " << id << " ЗАВЕРШЕНА.\n";
            } });

        task_id++;
        this_thread::sleep_for(milliseconds(500));
    }
}

int main()
{
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);
    srand(time(NULL));

    cout << "--- Пул потоків запущено ---\n\n";
    SimpleThreadPool pool;

    vector<thread> producers;
    bool testing_active = true;

    for (int i = 1; i <= 3; ++i)
    {
        producers.push_back(thread(producer, i, ref(pool), ref(testing_active)));
    }

    this_thread::sleep_for(seconds(15));

    testing_active = false;
    for (auto &p : producers)
    {
        if (p.joinable())
            p.join();
    }

    cout << "\n--- Генерацію зупинено. Пул доробляє задачі (Graceful Shutdown) ---\n";
    pool.shutdown(true);
    pool.printMetrics();

    return 0;
}
