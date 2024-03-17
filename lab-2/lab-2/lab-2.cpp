#include <iostream>
#include <functional>
#include <cstdlib> // Для функций rand() и srand()
#include <ctime>   // Для функции time()
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <algorithm>


std::mutex m1;
bool produce_stop = false;
bool consume_stop = false;

class Task {
    int id = 0;
    int time = 0;
    std::function<void(int, int)> f;

public:
    Task() {
        f = [](int id, int time) {};
    };
    Task(int id_,int time_): id(id_), time(time_) {
        f = [](int id, int time) {std::this_thread::sleep_for(std::chrono::seconds(1));
                             std::lock_guard<std::mutex> lock(m1);
                             std::cout << "id= " << id << "   " << "time= " << time << std::endl; };
    }

    void start() {
        f(id, time);
    }

    void print_info() {
        std::cout << "time is " << time << " id is " << id << std::endl;
    }

    int get_time() { return time; }
};


class MyQueue {
    std::mutex mutex;
    std::condition_variable condvar;
    std::queue<Task> q;
 //   std::queue<Task> q2;
    int total_time = 0;
    bool stop = false;
 //   int time_q2 = 0;

public:
 //   MyQueue() : queue() {}

    void push(Task task) {
        std::lock_guard<std::mutex> lock(mutex);
 //       if (stop) return;
        q.push(task);
        total_time += task.get_time();
        condvar.notify_one();
    }

/*
    bool empty() {
        std::lock_guard<std::mutex> lock(mutex);
        return q.empty();
    }
*/
    int get_total_time() {
        std::lock_guard<std::mutex> lock(mutex);
        return total_time; 
    }

    void set_total_time(int time) { 
        std::lock_guard<std::mutex> lock(mutex);
        total_time += time; 
    }

    void wait_front_pop_run() {
        std::unique_lock<std::mutex> lock(mutex);
        while (q.empty() && !stop) {
            condvar.wait(lock);
        }
        if (q.empty()) { 
            {
                std::unique_lock<std::mutex> lock(m1);
                consume_stop = true;
            }
            Task task;
            lock.unlock();
            task.start();
            return;
        }
        Task task = q.front();
        q.pop();
        total_time -= task.get_time();
        lock.unlock();
        task.start();
    }

    void stopQ() {
        std::unique_lock<std::mutex> lock(mutex);
        stop = true;
//       while (q.empty() && stop) {
            condvar.notify_all();
//            return;
 //       } 
    }
/*
    void print_tasks() {
        std::lock_guard<std::mutex> lock(mutex);
        std::cout << "num Tasks= " << q.size() << "\n";
        while(!q.empty()) {
            q.front().print_info();
            q.pop();
        }

    }
*/
};


class Maneger {
    int created_tasks = 0;
    int num_producers;
    std::vector<std::thread> producers_threads;
 //   std::mutex mutex;

public:
    Maneger(int num) : num_producers(num) {  }

    Task create_task() {
        int num_tasks;
        //std::srand(static_cast<unsigned int>(std::time(nullptr)));
        int randomNumber = std::rand() % 14 + 2;
        {
            std::lock_guard<std::mutex> lock(m1);
            num_tasks = created_tasks++;
        }
        Task t = Task(num_tasks, randomNumber);
        return t;
    }
 
    void start(MyQueue &q1, MyQueue &q2) {
        producers_threads.reserve(num_producers);
        for (int i = 0; i < num_producers; i++) {
            producers_threads.emplace_back(&Maneger::produce, this, std::ref(q1), std::ref(q2));
        }
    }

    void produce(MyQueue &q1, MyQueue &q2) {
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            {
            std::lock_guard<std::mutex> lock(m1);
            if (produce_stop) {
                std::cout << "created_tasks= " << created_tasks << "\n";
                break;
            }
            }

 //           std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            Task task = create_task();
 //           std::lock_guard<std::mutex> lock(mutex);
 //           Task task = create_task();
 //           std::lock_guard<std::mutex> lock(m1);
            if (q1.get_total_time() > q2.get_total_time()) {
                q2.push(task);
                q2.set_total_time( task.get_time() );
            }
            else {
                q1.push(task);
                q1.set_total_time(task.get_time());
            }
 //          my_q.push(create_task());
 //           std::cout << "Hello world!" << std::endl;
            
        }
        
    }
   
    ~Maneger() {
        for (int i = 0; i < num_producers; i++) {
            producers_threads[i].join();
        }
    }
};


class ThreadPool {
 //   std::queue<Task> q;
    std::vector<std::thread> consumers_threads;
 //   std::mutex mt;
    int num_consumers_1;

public:
    ThreadPool(int num1) : num_consumers_1(num1) {}

    void start(MyQueue& Q){
        consumers_threads.reserve(num_consumers_1);
        for (int i = 0; i < num_consumers_1; i++) {
            consumers_threads.emplace_back(&ThreadPool::consume, this, std::ref(Q));
        }
    }

    void consume(MyQueue& Q) {
        while (true) {
            {
                std::lock_guard<std::mutex> lock(m1);
                if (consume_stop) break;
            }
            Q.wait_front_pop_run();
        }
    }

    int stop() { return 0; }

    int immediate_stop() { return 0; }

    ~ThreadPool() {
        for (int i = 0; i < num_consumers_1; i++) {
            consumers_threads[i].join();
        }
    }

};


int main()
{
    MyQueue myQueue_1;
    MyQueue myQueue_2;
    Maneger maneger(3);
    ThreadPool threadPool_1(2);
    ThreadPool threadPool_2(2);

 
    std::cout << "Quick stop   q\n";
    std::cout << "Slow stop    s\n";
    char x;
    do {
        std::cout << "Begin        b\n";
        std::cin >> x;
 //       if ('b' == x) return 0;
    }     while ('b' != x);


    maneger.start(myQueue_1, myQueue_2);
    threadPool_1.start(myQueue_1);
    threadPool_2.start(myQueue_2);
 
    do {
        std::cin >> x;
    } while ('q' != x && 's' != x);

    if ('q' == x) {
        {
            std::lock_guard<std::mutex> lock(m1);
            produce_stop = true;
            consume_stop = true;
        }
        myQueue_1.stopQ();
        myQueue_2.stopQ();
    }
    else {
        {
            std::lock_guard<std::mutex> lock(m1);
            produce_stop = true;
        }
        myQueue_1.stopQ();
        myQueue_2.stopQ();

    }

 //   std::this_thread::sleep_for(std::chrono::seconds(10));
 //   myqueue.print_tasks();

    return 0;
}

