//  Copyright (C) 2009 Tim Blechmann
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//[queue_example
#include <boost/thread/thread.hpp>
#include <boost/lockfree/queue.hpp>
#include <iostream>

#include <boost/atomic.hpp>

boost::atomic_int producer_count(0);
boost::atomic_int consumer_count(0);

const int iterations = 1000 * 1000;
const int producer_thread_count = 4;
const int consumer_thread_count = 4;

struct Context{
	Context():strcnt(0), intcnt(0), intv(0), strsz(0){}
	size_t	strcnt;
	size_t	intcnt;
	size_t	intv;
	size_t	strsz;
};

class Task{
public:
	virtual ~Task(){}
	virtual void run(Context &) = 0;
};

boost::lockfree::queue<Task*> queue(128);

class StringTask: public Task{
	std::string str;
	virtual void run(Context &_rctx){
		++_rctx.strcnt;
		_rctx.strsz += str.size();
	}
public:
	StringTask(const std::string &_str):str(_str){}
};

class IntTask: public Task{
	int v;
	virtual void run(Context &_rctx){
		++_rctx.strcnt;
		_rctx.intv += v;
	}
public:
	IntTask(int _v = 0):v(_v){}
};


const char *strs[] = {
	"one",
	"two",
	"three",
	"four",
	"five",
	"six",
	"seven",
	"eight",
	"nine",
	"ten"
};


void producer(void)
{
	for(int i = 0; i < iterations; ++i){
		Task *pt = nullptr;
		if(i % 2){
			pt = new IntTask(i);
		}else{
			size_t stridx = (i/2) % (sizeof(strs)/sizeof(const char*));
			pt = new StringTask(strs[stridx]);
		}
		++producer_count;
		while (!queue.push(pt))
            ;
	}
}

boost::atomic<bool> done (false);

void consumer(void)
{
	Context		ctx;
	Task *value = nullptr;
	while (!done) {
		while (queue.pop(value)){
			++consumer_count;
			value->run(ctx);
			delete value;
		}
	}

	while (queue.pop(value)){
		++consumer_count;
		value->run(ctx);
		delete value;
	}
}

int main(int argc, char* argv[])
{
    using namespace std;
    cout << "boost::lockfree::queue is ";
    if (!queue.is_lock_free())
        cout << "not ";
    cout << "lockfree" << endl;

    boost::thread_group producer_threads, consumer_threads;

    for (int i = 0; i != producer_thread_count; ++i)
        producer_threads.create_thread(producer);

    for (int i = 0; i != consumer_thread_count; ++i)
        consumer_threads.create_thread(consumer);

    producer_threads.join_all();
    done = true;

    consumer_threads.join_all();

    cout << "produced " << producer_count << " objects." << endl;
    cout << "consumed " << consumer_count << " objects." << endl;
}
//]
