/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2025
   Matthias P. Braendli, matthias.braendli@mpb.li

   An implementation for a threadsafe queue, depends on C++11

   When creating a ThreadsafeQueue, one can specify the minimal number
   of elements it must contain before it is possible to take one
   element out.
 */
/*
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <functional>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <utility>
#include <cassert>

/* This queue is meant to be used by two threads. One producer
 * that pushes elements into the queue, and one consumer that
 * retrieves the elements.
 *
 * The queue can make the consumer block until an element
 * is available, or a wakeup requested.
 */

/* Class thrown by blocking pop to tell the consumer
 * that there's a wakeup requested. */
class ThreadsafeQueueWakeup {};

template<typename T>
class ThreadsafeQueue
{
public:
    /* Push one element into the queue, and notify another thread that
     * might be waiting.
     *
     * if max_size > 0 and the queue already contains at least max_size elements,
     * the element gets discarded.
     *
     * returns the new queue size.
     */
    size_t push(T const& val, size_t max_size = 0)
    {
        std::unique_lock<std::mutex> lock(the_mutex);
        size_t queue_size_before = the_queue.size();
        if (max_size == 0) {
            the_queue.push_back(val);
        }
        else if (queue_size_before < max_size) {
            the_queue.push_back(val);
        }
        size_t queue_size = the_queue.size();
        lock.unlock();
        the_rx_notification.notify_one();

        return queue_size;
    }

    size_t push(T&& val, size_t max_size = 0)
    {
        std::unique_lock<std::mutex> lock(the_mutex);
        size_t queue_size_before = the_queue.size();
        if (max_size == 0) {
            the_queue.emplace_back(std::move(val));
        }
        else if (queue_size_before < max_size) {
            the_queue.emplace_back(std::move(val));
        }
        size_t queue_size = the_queue.size();
        lock.unlock();

        the_rx_notification.notify_one();

        return queue_size;
    }

    struct push_overflow_result { bool overflowed; size_t new_size; };

    /* Push one element into the queue, and if queue is
     * full remove one element from the other end.
     *
     * max_size == 0 is not allowed.
     *
     * returns the new queue size and a flag if overflow occurred.
     */
    push_overflow_result push_overflow(T const& val, size_t max_size)
    {
        assert(max_size > 0);
        std::unique_lock<std::mutex> lock(the_mutex);

        bool overflow = false;
        while (the_queue.size() >= max_size) {
            overflow = true;
            the_queue.pop_front();
        }
        the_queue.push_back(val);
        const size_t queue_size = the_queue.size();
        lock.unlock();

        the_rx_notification.notify_one();

        return {overflow, queue_size};
    }

    push_overflow_result push_overflow(T&& val, size_t max_size)
    {
        assert(max_size > 0);
        std::unique_lock<std::mutex> lock(the_mutex);

        bool overflow = false;
        while (the_queue.size() >= max_size) {
            overflow = true;
            the_queue.pop_front();
        }
        the_queue.emplace_back(std::move(val));
        const size_t queue_size = the_queue.size();
        lock.unlock();

        the_rx_notification.notify_one();

        return {overflow, queue_size};
    }


    /* Push one element into the queue, but wait until the
     * queue size goes below the threshold.
     *
     * returns the new queue size.
     */
    size_t push_wait_if_full(T const& val, size_t threshold)
    {
        std::unique_lock<std::mutex> lock(the_mutex);
        while (the_queue.size() >= threshold) {
            the_tx_notification.wait(lock);
        }
        the_queue.push_back(val);
        size_t queue_size = the_queue.size();
        lock.unlock();

        the_rx_notification.notify_one();

        return queue_size;
    }

    /* Trigger a wakeup event on a blocking consumer, which
     * will receive a ThreadsafeQueueWakeup exception.
     */
    void trigger_wakeup(void)
    {
        std::unique_lock<std::mutex> lock(the_mutex);
        wakeup_requested = true;
        lock.unlock();
        the_rx_notification.notify_one();
    }

    /* Send a notification for the receiver thread */
    void notify(void)
    {
        the_rx_notification.notify_one();
    }

    bool empty() const
    {
        std::unique_lock<std::mutex> lock(the_mutex);
        return the_queue.empty();
    }

    size_t size() const
    {
        std::unique_lock<std::mutex> lock(the_mutex);
        return the_queue.size();
    }

    bool try_pop(T& popped_value)
    {
        std::unique_lock<std::mutex> lock(the_mutex);
        if (the_queue.empty()) {
            return false;
        }

        popped_value = the_queue.front();
        the_queue.pop_front();

        lock.unlock();
        the_tx_notification.notify_one();

        return true;
    }

    void wait_and_pop(T& popped_value, size_t prebuffering = 1)
    {
        std::unique_lock<std::mutex> lock(the_mutex);
        while (the_queue.size() < prebuffering and
                not wakeup_requested) {
            the_rx_notification.wait(lock);
        }

        if (wakeup_requested) {
            wakeup_requested = false;
            throw ThreadsafeQueueWakeup();
        }
        else {
            std::swap(popped_value, the_queue.front());
            the_queue.pop_front();

            lock.unlock();
            the_tx_notification.notify_one();
        }
    }

    template<typename R>
    std::vector<R> map(std::function<R(const T&)> func) const
    {
        std::vector<R> result;
        std::unique_lock<std::mutex> lock(the_mutex);
        for (const T& elem : the_queue) {
            result.push_back(func(elem));
        }
        return result;
    }

private:
    std::deque<T> the_queue;
    mutable std::mutex the_mutex;
    std::condition_variable the_rx_notification;
    std::condition_variable the_tx_notification;
    bool wakeup_requested = false;
};

