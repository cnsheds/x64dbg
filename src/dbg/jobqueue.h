#pragma once

#include "bridgemain.h"
#include "concurrentqueue/blockingconcurrentqueue.h"
#include <functional>

class JobQueue
{
    moodycamel::BlockingConcurrentQueue<std::function<bool()>> mQueue;
    std::thread mThread;

public:
    JobQueue()
    {
        start();
    }

    ~JobQueue()
    {
        stop();
    }

    JobQueue(const JobQueue &) = delete;
    JobQueue & operator=(const JobQueue &) = delete;
    JobQueue(JobQueue &&) = delete;
    JobQueue & operator=(JobQueue &&) = delete;

    void start()
    {
        if(mThread.joinable())
        {
            return;
        }

        mThread = std::thread([this]()
        {
            while(true)
            {
                std::function<bool()> job;
                mQueue.wait_dequeue(job);
                if(!job())
                {
                    break;
                }
            }
        });
    }

    void stop()
    {
        if(!mThread.joinable())
        {
            return;
        }

        mQueue.enqueue([]()
        {
            return false;
        });
        mThread.join();
    }

    bool async(std::function<void()> job)
    {
        return mQueue.enqueue([job = std::move(job)]()
        {
            job();
            return true;
        });
    }

    // Non-void return
    template < typename F,
               typename R = decltype(std::declval<F>()()),
               typename std::enable_if < !std::is_void<R>::value, int >::type = 0 >
    R await(F && job)
    {
        R result = {};
        impl_await([job = std::move(job), &result]()
        {
            result = job();
        });
        return result;
    }

    // Void return
    template <typename F,
              typename R = decltype(std::declval<F>()()),
              typename std::enable_if<std::is_void<R>::value, int>::type = 0>
    void await(F && job)
    {
        impl_await(std::forward<F>(job));
    }

private:
    template<class Func>
    void impl_await(Func && job)
    {
        auto event = CreateEventW(nullptr, false, false, nullptr);
        auto func = [job = std::move(job), event]()
        {
            job();
            SetEvent(event);
            return true;
        };
        if(!mQueue.enqueue(std::move(func)))
        {
            CloseHandle(event);
            return;
        }
        while(WaitForSingleObject(event, 100) == WAIT_TIMEOUT)
        {
            //workaround for scripts being executed on the GUI thread
            GuiProcessEvents();
        }
        CloseHandle(event);
        return;
    }
};
