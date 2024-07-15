#pragma once

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <deque>
#include <atomic>
#include <cassert>
#include <map>

namespace uvgvpcc_dec
{

enum threadqueue_job_state {
  /**
   * \brief Job has been submitted, but is not allowed to run yet.
   */
  THREADQUEUE_JOB_STATE_PAUSED,

  /**
   * \brief Job is waiting for dependencies.
   */
  THREADQUEUE_JOB_STATE_WAITING,

  /**
   * \brief Job is ready to run.
   */
  THREADQUEUE_JOB_STATE_READY,

  /**
   * \brief Job is running.
   */
  THREADQUEUE_JOB_STATE_RUNNING,

  /**
   * \brief Job is completed.
   */
  THREADQUEUE_JOB_STATE_DONE,
};

class ThreadQueue;

class Job : public std::enable_shared_from_this<Job> {
public: 
    using JobFunction = std::function<void()>;
    // Variadic template constructor
    template <typename Func, typename... Args>
    Job(std::string name, uint8_t priority, Func&& func, Args&&... args)
        : name_(name),
          func_(std::bind(std::forward<Func>(func), std::forward<Args>(args)...)),
          state_(threadqueue_job_state::THREADQUEUE_JOB_STATE_PAUSED),
          dependencies_(0),
          priority(priority),
          completed_(false) {}
    void execute();
    void addDependency(std::shared_ptr<Job> dependency);
    bool isReady() const;
    void wait();
    void complete();
    std::string getName() const { return name_; }
    threadqueue_job_state getState() const { return state_; }
    void setState(threadqueue_job_state state) { state_ = state;}

    mutable std::mutex mtx_;
    std::vector<std::shared_ptr<Job>> reverseDependencies_;
    std::string name_;
    JobFunction func_;
    threadqueue_job_state state_;
    std::atomic<int> dependencies_;
    std::atomic<uint8_t> priority;
private:
    std::condition_variable cv_;
    std::atomic<bool> completed_;
};

class ThreadQueue {
public:
    ThreadQueue(int numThreads);
    ~ThreadQueue();

    void submitJob(std::shared_ptr<Job> job);
    void pushJob(std::shared_ptr<Job> job);
    void stop();
    void waitForJob(std::shared_ptr<Job> job);

private:
    void workerThread();
    std::mutex mtx_;
    std::condition_variable jobAvailable_;
    std::condition_variable jobDone_;
    std::vector<std::thread> threads_;
    std::array<std::deque<std::shared_ptr<Job>>, 6> jobs_;
    std::atomic<bool> stop_;
};

const std::string jobStateToStr (threadqueue_job_state s);

} // namespace uvgvpcc_dec