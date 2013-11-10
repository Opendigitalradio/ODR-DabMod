/*
   Copyright (C) 2007, 2008, 2009, 2010, 2011 Her Majesty the Queen in
   Right of Canada (Communications Research Center Canada)

   Copyright (C) 2013
   Matthias P. Braendli, matthias.braendli@mpb.li

   An implementation for a threadsafe queue using boost thread library

   When creating a ThreadsafeQueue, one can specify the minimal number
   of elements it must contain before it is possible to take one
   element out.
 */
/*
   This file is part of CRC-DADMOD.

   CRC-DADMOD is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   CRC-DADMOD is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with CRC-DADMOD.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef THREADSAFE_QUEUE_H
#define THREADSAFE_QUEUE_H

#include <boost/thread.hpp>
#include <queue>

template<typename T>
class ThreadsafeQueue
{
private:
    std::queue<T> the_queue;
    mutable boost::mutex the_mutex;
    boost::condition_variable the_condition_variable;
    size_t the_required_size;
public:

    ThreadsafeQueue() : the_required_size(1) {}

    ThreadsafeQueue(size_t required_size) : the_required_size(required_size) {}

    size_t push(T const& val)
    {
        boost::mutex::scoped_lock lock(the_mutex);
        the_queue.push(val);
        size_t queue_size = the_queue.size();
        lock.unlock();
        the_condition_variable.notify_one();

        return queue_size;
    }

    void notify()
    {
        the_condition_variable.notify_one();
    }

    bool empty() const
    {
        boost::mutex::scoped_lock lock(the_mutex);
        return the_queue.empty();
    }

    bool try_pop(T& popped_value)
    {
        boost::mutex::scoped_lock lock(the_mutex);
        if(the_queue.size() < the_required_size)
        {
            return false;
        }

        popped_value = the_queue.front();
        the_queue.pop();
        return true;
    }

    void wait_and_pop(T& popped_value)
    {
        boost::mutex::scoped_lock lock(the_mutex);
        while(the_queue.size() < the_required_size)
        {
            the_condition_variable.wait(lock);
        }

        popped_value = the_queue.front();
        the_queue.pop();
    }
};

#endif

