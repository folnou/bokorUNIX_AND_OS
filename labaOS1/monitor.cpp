#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
class Event {
public:
    Event(int id, std::size_t payloadSize)
        : id_(id), payload_(payloadSize, 3.14) {
    }

    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    int id() const { return id_; }
    std::size_t size() const { return payload_.size(); }

private:
    int id_;
    std::vector<double> payload_;
};
class Monitor {
public:
    bool provide(std::unique_ptr<Event> e) {
        const int id = e->id();
        bool accepted;

        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (ready_) {
                accepted = false;
            }
            else {
                data_ = std::move(e);
                ready_ = true;
                accepted = true;
            }
        }
        if (accepted) {
            std::cout << "[поставщик]  событие #" << id
                << " отправлено" << std::endl;
            cv_.notify_one();
        }
        else {
            std::cout << "[поставщик]  событие #" << id
                << " пропущено: предыдущее ещё не обработано"
                << std::endl;
        }
        return accepted;
    }
    std::unique_ptr<Event> consume() {
        std::unique_ptr<Event> e;

        {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this] { return ready_ || stopping_; });
            if (!ready_)
                return nullptr;
            e = std::move(data_);
            ready_ = false;
        }

        std::cout << "[потребитель] событие #" << e->id()
            << " получено (полезная нагрузка: " << e->size()
            << " элементов)" << std::endl;
        return e;
    }
    void stop() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stopping_ = true;
        }
        cv_.notify_all();
    }
private:
    std::mutex mtx_;
    std::condition_variable cv_;
    bool ready_ = false;
    bool stopping_ = false;
    std::unique_ptr<Event> data_;
};
class Producer {
public:
    Producer(Monitor& m, int count) : monitor_(m), count_(count) {}

    void run() {
        for (int id = 1; count_ <= 0 || id <= count_; ++id) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            monitor_.provide(std::make_unique<Event>(id, 1'000'000));
        }
        monitor_.stop();
    }
private:
    Monitor& monitor_;
    int count_;
};
class Consumer {
public:
    explicit Consumer(Monitor& m) : monitor_(m) {}

    void run() {
        while (std::unique_ptr<Event> e = monitor_.consume()) {
        }
    }
private:
    Monitor& monitor_;
};
int main(int argc, char* argv[]) {
    int count = (argc > 1) ? std::atoi(argv[1]) : 0;
    Monitor monitor;
    Producer producer(monitor, count);
    Consumer consumer(monitor);
    std::thread consumerThread(&Consumer::run, &consumer);
    std::thread producerThread(&Producer::run, &producer);
    producerThread.join();
    consumerThread.join();
    return 0;
}