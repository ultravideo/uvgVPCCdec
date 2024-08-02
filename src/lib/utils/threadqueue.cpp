#include <utility>
#include "uvgvpccdec/threadqueue.hpp"
#include "uvgvpccdec/log.hpp"

namespace uvgvpcc_dec
{

const std::string jobStateToStr (threadqueue_job_state s) {
    const std::map<threadqueue_job_state,std::string> stateStr {
        { threadqueue_job_state::THREADQUEUE_JOB_STATE_PAUSED, "threadqueue_job_state::THREADQUEUE_JOB_STATE_PAUSED"},
        { threadqueue_job_state::THREADQUEUE_JOB_STATE_WAITING, "threadqueue_job_state::THREADQUEUE_JOB_STATE_WAITING"},
        { threadqueue_job_state::THREADQUEUE_JOB_STATE_READY, "threadqueue_job_state::THREADQUEUE_JOB_STATE_READY"},
        { threadqueue_job_state::THREADQUEUE_JOB_STATE_RUNNING, "threadqueue_job_state::THREADQUEUE_JOB_STATE_RUNNING"},
        { threadqueue_job_state::THREADQUEUE_JOB_STATE_DONE, "threadqueue_job_state::THREADQUEUE_JOB_STATE_DONE"},
    };
    auto   it  = stateStr.find(s);
    return it == stateStr.end() ? "Out of range" : it->second;
}

void Job::addDependency(std::shared_ptr<Job> dependency) {
    Logger::log(LogLevel::DEBUG, "JOB: " + getName(), "Adding " + dependency->getName() + " as dependency\n");
    dependency->mtx_.lock();
    Logger::log(LogLevel::DEBUG, "JOB: " + getName(), "Dependency locked\n");
    if (dependency->completed_) {
        return;
    }
    Logger::log(LogLevel::DEBUG, "JOB: " + getName(), dependency->getName() + " state: " + jobStateToStr(dependency->getState()) + "\n");
    dependencies_++;
    Logger::log(LogLevel::DEBUG, "JOB: " + getName(), "Dependencies: " + std::to_string(dependencies_) + "\n");

    Logger::log(LogLevel::DEBUG, "JOB: " + getName(), dependency->getName() + " Reverse dependencies: " + std::to_string(dependency->reverseDependencies_.size()) + "\n");
    dependency->reverseDependencies_.emplace_back(this->shared_from_this());
    Logger::log(LogLevel::DEBUG, "JOB: " + getName(), dependency->getName() + " Reverse dependencies: " + std::to_string(dependency->reverseDependencies_.size()) + "\n");
    dependency->mtx_.unlock();
}

bool Job::isReady() const {
    return dependencies_.load() == 0;
}

void Job::execute() {
    func_();
}

void Job::wait() {
    std::unique_lock lock(mtx_);
    cv_.wait(lock, [this]() { Logger::log(LogLevel::DEBUG, "JOB: " + getName(), "is it ready ? " + std::string(completed_.load()? "yes" : "no") + ".\n" ) ; return completed_.load(); });
}

void Job::complete() {
    // std::unique_lock lock(mtx_);
    // state_ = threadqueue_job_state::THREADQUEUE_JOB_STATE_DONE;
    completed_ = true;
    cv_.notify_all();
}

ThreadQueue::ThreadQueue(int numThreads) : stop_(false) {
    for (int i = 0; i < numThreads; ++i) {
        threads_.emplace_back(&ThreadQueue::workerThread, this);
    }
}

ThreadQueue::~ThreadQueue() {
    stop();
}

void ThreadQueue::pushJob(std::shared_ptr<Job> job) {
    assert(job->getState() == threadqueue_job_state::THREADQUEUE_JOB_STATE_PAUSED
        || job->getState() == threadqueue_job_state::THREADQUEUE_JOB_STATE_WAITING);
    Logger::log(LogLevel::DEBUG, "ThreadQueue", "Job " + job->getName() + " pushed to the queue\n");
    job->setState(threadqueue_job_state::THREADQUEUE_JOB_STATE_READY);
    jobs_[job->priority].push_back(job);
}

void ThreadQueue::submitJob(std::shared_ptr<Job> job) {
    std::lock_guard lockQ(mtx_);
    std::lock_guard lockJ(job->mtx_);
    if (threads_.empty()) {
        job->setState(threadqueue_job_state::THREADQUEUE_JOB_STATE_READY);
        job->execute();
        job->complete();
    } else if (job->isReady()) {
        pushJob(job);
        jobAvailable_.notify_one();
    } else {
        job->setState(threadqueue_job_state::THREADQUEUE_JOB_STATE_WAITING);
    }
}

void ThreadQueue::stop() {
    {
        std::unique_lock lock(mtx_);
        stop_ = true;
        jobAvailable_.notify_all();
    }
    for (auto &thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void ThreadQueue::waitForJob(std::shared_ptr<Job> job) {
    job->wait();
}

void ThreadQueue::workerThread() {
    std::unique_lock lockQ(mtx_);
    for(;;) {
        std::shared_ptr<Job> job;
        {
            jobAvailable_.wait(lockQ, [this]() { return stop_ || std::any_of(jobs_.begin(), jobs_.end(), [](const std::deque<std::shared_ptr<Job>> &jobs) { return !jobs.empty(); }); });
            if (stop_) {
                return;
            }
            for (int i = 5; i >= 0; --i) {
                if (!jobs_[i].empty()) {
                    job = jobs_[i].front();
                    jobs_[i].pop_front();
                    break;
                }
            }
            // job = jobs_.front();
            // jobs_.pop_front();
            Logger::log(LogLevel::DEBUG, "ThreadQueue", "Job " + job->getName() + " popped from the queue\n");
        }
        std::unique_lock lockJ(job->mtx_);
        assert(job->getState() == threadqueue_job_state::THREADQUEUE_JOB_STATE_READY);
        job->setState(threadqueue_job_state::THREADQUEUE_JOB_STATE_RUNNING);
        Logger::log(LogLevel::DEBUG, "JOB: " + job->getName(), jobStateToStr(job->getState()) + "\n");
        lockJ.unlock();
        lockQ.unlock();

        job->execute();

        lockQ.lock();
        lockJ.lock();
        assert(job->getState() == threadqueue_job_state::THREADQUEUE_JOB_STATE_RUNNING);
        job->setState(threadqueue_job_state::THREADQUEUE_JOB_STATE_DONE);
        Logger::log(LogLevel::DEBUG, "JOB: " + job->getName(), jobStateToStr(job->getState()) + "\n");
        job->complete();
        jobDone_.notify_all();

        // Go through all the jobs that depend on this one, decreasing their
        // ndepends. Count how many jobs can now start executing so we know how
        // many threads to wake up.
        int readyJobs = 0;
        // for (auto &dep : job->reverseDependencies_) {
        for (auto dep = job->reverseDependencies_.begin(); dep != job->reverseDependencies_.end();) {
            std::lock_guard lockD((*dep)->mtx_);
            Logger::log(LogLevel::DEBUG, "JOB: " + job->getName(), (*dep)->getName() + "remove dependency\n");
            assert((*dep)->getState() == threadqueue_job_state::THREADQUEUE_JOB_STATE_WAITING ||
                   (*dep)->getState() == threadqueue_job_state::THREADQUEUE_JOB_STATE_PAUSED);
            assert((*dep)->dependencies_ > 0);
            (*dep)->dependencies_--;

            if ((*dep)->dependencies_ == 0 && (*dep)->getState() == threadqueue_job_state::THREADQUEUE_JOB_STATE_WAITING) {
                pushJob(*dep);
                readyJobs++;
            }
            job->reverseDependencies_.erase(dep);
        }
        for (int i = 0; i < readyJobs - 1; ++i){
            jobAvailable_.notify_one();
        }
    }
}

} // namespace uvgvpcc_dec