#include <iostream>
#include <functional>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <chrono>
#include <random>


int total_task = 0 ;
std::mutex m_out;
std::vector<std::chrono::nanoseconds> waiting_times;
int num_wait = 0;

std::vector<int> size_q1;
std::vector<int> size_q2;

std::mutex mtx;
std::vector<std::chrono::nanoseconds> working_times;

class Task {
    int my_id;
    int my_time;
    std::function<void(int time,int id)> f;

public:
    Task(int id, int time) : my_id(id), my_time(time) {
        f = [](int my_time,int my_id) {
            std::this_thread::sleep_for(std::chrono::seconds(my_time)); 
            //std::this_thread::sleep_for(std::chrono::seconds(4));
            std::lock_guard<std::mutex> lock(m_out);
            std::cout << "Task with time " << my_time << " id is " << my_id <<" Thread id "<< std::this_thread::get_id()<< std::endl;
            total_task++;
        };
    }

    Task():my_id(0),my_time(0){}

    int get_time() { return my_time; }
    int get_id() { return my_id; }

    std::function<void(int time,int id)> get_f() { return f; }
};

class MyQueue {
    int all_time = 0;
    std::queue<Task> q;

public:
    std::condition_variable con_var;

    void push(Task t) {
        q.push(t);
        set_all_time(t.get_time() + get_all_time());
    }

    Task front_and_pop() {
        Task t = q.front();
        q.pop();
        set_all_time(get_all_time() - t.get_time() );
        return t;
    }

    int size() {
        return q.size();
    }

    bool empty() {
        return q.empty();
    }

    Task front() {
        return q.front();
    }

    int get_all_time() {
        return all_time;
    }

    void set_all_time(int time) {
        all_time = time;
    }
};


class ThreadPool {
    std::vector<std::thread> producer_threads;
    std::vector<std::thread> consumer_threads;
    int created_tasks = 0;
    int num_producers;
    int num_consumers;
    MyQueue queue1;
    MyQueue queue2;

    std::random_device rd;
    

    std::mutex m;
    std::condition_variable cv;
    bool finished = false;
    bool paused = false;
    bool working_to_the_end = false;

public:
    ThreadPool() = default;

    void initialize(int producers, int consumers) {
        num_consumers = consumers;
        num_producers = producers;
        for (int i = 0; i < producers; i++) {
            producer_threads.emplace_back(&ThreadPool::produce,this,std::ref(queue1),std::ref(queue2));
        }

        for (int i = 0; i < consumers/2; i++) {
            consumer_threads.emplace_back(&ThreadPool::consume, this, std::ref(queue1));
        }

        for (int i = consumers / 2; i < consumers; i++) {
            consumer_threads.emplace_back(&ThreadPool::consume, this, std::ref(queue2));
        }
    }

private:
    Task create_task() {
        std::mt19937 mersenne(rd());
        std::uniform_int_distribution<int> dist(2, 15); 
        int random_number = dist(mersenne);

        Task t = Task(created_tasks, random_number);
        created_tasks++;
        return t;
    }

    void produce(MyQueue& q1, MyQueue& q2) {
        while (true ) {          
            std::unique_lock<std::mutex> lock(m);
            while (paused) { cv.wait(lock); }
            if (finished || working_to_the_end) return;
            if (q1.get_all_time() > q2.get_all_time()) {
                q2.push(create_task()); q2.con_var.notify_one();}
            else { q1.push(create_task()); q1.con_var.notify_one(); }
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }

    void consume(MyQueue& q) {
        while (true) {
            std::unique_lock<std::mutex> lock(m);
            while ((q.empty() || paused) && !finished && !working_to_the_end) {
                num_wait++;
                auto start_waiting = std::chrono::high_resolution_clock::now();
                q.con_var.wait(lock); 
                auto finish_waiting = std::chrono::high_resolution_clock::now();
                auto waiting_time = std::chrono::duration_cast<std::chrono::nanoseconds>(finish_waiting - start_waiting);
                waiting_times.push_back(waiting_time);
            }
            if (finished || q.empty() ) return;
            Task t;
            t = q.front_and_pop();
            lock.unlock();
            auto start_working = std::chrono::high_resolution_clock::now();
            t.get_f()(t.get_time(), t.get_id());
            auto finish_working = std::chrono::high_resolution_clock::now();
            auto working_time = std::chrono::duration_cast<std::chrono::nanoseconds>(finish_working - start_working);
            std::lock_guard<std::mutex> lok(mtx);
            working_times.push_back(working_time);
        }
    }

public:
    void pause() {
        std::unique_lock<std::mutex> lock(m);
        paused = true;
        std::cout << "pause started\n";
    }

    void resume() {
        std::unique_lock<std::mutex> lock(m);
        paused = false;
        queue1.con_var.notify_all();
        queue2.con_var.notify_all();
        cv.notify_all();
        std::cout << "pause finished\n";
    }

    void finish() {
        std::lock_guard<std::mutex> lock(m);
        finished = true;
        queue1.con_var.notify_all();
        queue2.con_var.notify_all();
    }

    void working_to_the_end_finish() {
        std::lock_guard<std::mutex> lock(m);
        working_to_the_end = true;
        queue1.con_var.notify_all();
        queue2.con_var.notify_all();
    }

    int get_created_tasks() { return created_tasks; }
    int get_size_q1() { return queue1.size(); }
    int get_size_q2() { return queue2.size(); }

    ~ThreadPool() {

        for (int i = 0; i < num_producers; i++) {
            producer_threads[i].join();
        }

        for (int i = 0; i < num_consumers; i++) {
            consumer_threads[i].join();
        }
        std::cout << "Created tasks " << this->get_created_tasks() << "\n" << "Total task " << total_task << "\n";
    }

};

void test(ThreadPool& tp) {
    for (int i = 0; i < 60; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        size_q1.push_back(tp.get_size_q1());
        size_q2.push_back(tp.get_size_q2());
    }

    tp.finish();

    std::this_thread::sleep_for(std::chrono::seconds(25));

    int average_q1 = 0;
    
    for (int i = 0; i < size_q1.size(); i++) {
        average_q1 += size_q1[i];
    }
   average_q1 = average_q1 / size_q1.size();

    int average_q2 = 0;
    for (int i = 0;i < size_q2.size(); i++) {
        average_q2 += size_q2[i];
    }
   average_q2 = average_q2 / size_q2.size();

    std::cout << "Average length of queue1: " << average_q1 << ", queue2: " << average_q2 << std::endl;
}



int main()
{
    ThreadPool tp;
    tp.initialize(4, 4);
    //test(std::ref(tp));
    std::this_thread::sleep_for(std::chrono::seconds(15));
    tp.working_to_the_end_finish();
    std::this_thread::sleep_for(std::chrono::seconds(15));

    /*std::this_thread::sleep_for(std::chrono::seconds(300));
    tp.finish();

    std::chrono::nanoseconds total_working_time = std::chrono::nanoseconds::zero();
    for (const auto& time : working_times) {
        total_working_time += time;
    }
    std::chrono::nanoseconds average_working_time;

    if (working_times.size() == 0) average_working_time = std::chrono::nanoseconds::zero();
    else
        average_working_time = total_working_time / working_times.size();

    std::cout << "Average working time: " << average_working_time.count() * 1e-9 << " seconds" << std::endl;
    

    std::chrono::nanoseconds total_waiting_time = std::chrono::nanoseconds::zero();
    for (const auto& time : waiting_times) {
        total_waiting_time += time;
    }
    std::chrono::nanoseconds average_waiting_time;

    if(waiting_times.size() == 0 ) average_waiting_time = std::chrono::nanoseconds::zero();
    else
    average_waiting_time = total_waiting_time / waiting_times.size();

    std::cout << "Average waiting time: " << average_waiting_time.count() *1e-9<< " seconds" << std::endl;
    std::cout << "Size of vect " << waiting_times.size() << " num wait is " << num_wait << std::endl;
    */
}


