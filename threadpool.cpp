#include <iostream>
#include <pthread.h>
#include <memory>
#include <vector>
#include <functional>
#include <unordered_map>
#include <queue>
#include <pthread.h>

#include "mutexLock.hpp"
#include "timer.h"
#include "requestData.h"
#include "util.h"
#include "threadpool.h"


using namespace std;

pthread_mutex_t ThreadPool::lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ThreadPool::notify = PTHREAD_COND_INITIALIZER;
std::vector<pthread_t> ThreadPool::threads;
std::vector<ThreadTask> ThreadPool::queue;
int ThreadPool::thread_count = 0;
int ThreadPool::queue_size = 0;
int ThreadPool::head = 0;
int ThreadPool::tail = 0;
int ThreadPool::count = 0;
bool ThreadPool::shutdown = false;
int ThreadPool::start_num = 0;

int ThreadPool::threadpool_create(int _thread_num, int _queue_size)
{
	if(_thread_num <= 0 || _thread_num > THREAD_MAX_NUM || _queue_size <= 0 || _queue_size > QUEUE_MAX_SIZE)
	{
		_thread_num = 4;
		_queue_size = 1024;
	}

	thread_count = 0;
	queue_size = _queue_size;
	head = tail = count = 0;
    shutdown = false;	
	start_num = 0;
	
	threads.resize(_thread_num);
	queue.resize(_queue_size);	
	
	
	for(int i = 0; i < _thread_num; ++i)
	{
		if(pthread_create(&threads[i], NULL, threadpool_main, (void*)(0)) != 0)
		{
			return -1;
		}
		++thread_count;
		++start_num;
	}
	return 0;	
}

int ThreadPool::threadpool_add(shared_ptr<void> args, function<void(shared_ptr<void>)> fun)
{
	int next, err = 0;
	
	if(pthread_mutex_lock(&lock) != 0)
		return THREADPOOL_LOCK_FAILURE;
	
	do
	{
		next = (tail + 1) % queue_size;	
		//queue is full
		if(count == queue_size)
		{
			err = THREADPOOL_QUEUE_FULL;
			break;
		}
		//webserver is close
		if(shutdown)
		{
			err = THREADPOOL_SHUTDOWN;
			break;
		}
		queue[tail].fun = fun;
		queue[tail].args = args;
		tail = next;
		++count;

        if(pthread_cond_signal(&notify) != 0) 
        {
            err = THREADPOOL_LOCK_FAILURE;
            break;
        }
	}while(false);

    if(pthread_mutex_unlock(&lock) != 0)
    	err = THREADPOOL_LOCK_FAILURE;
	
	return err;
}

void* ThreadPool::threadpool_main(void *args)
{
	while(true)
	{
		ThreadTask task;
		pthread_mutex_lock(&lock);
		while((count == 0) && (!shutdown))
		{
			pthread_cond_wait(&notify, &lock);
		}
		if((shutdown == immediate_shutdown) ||
			((shutdown == graceful_shutdown) && (count == 0)))
		{
			break;
		}
		task.fun = queue[head].fun;
		task.args = queue[head].args;
		queue[head].fun = NULL;
		queue[head].args.reset();
		head = (head + 1) % queue_size;
		--count;
		pthread_mutex_unlock(&lock);
		(task.fun)(task.args);
	}
	
	pthread_mutex_lock(&lock);
	--start_num;
	pthread_mutex_unlock(&lock);
	
	cout<<"This threadpool thread finished!"<<endl;
	
	pthread_exit(NULL);
	return NULL;
}

void myHandler(shared_ptr<void> req)
{
	shared_ptr<RequestData> request = std::static_pointer_cast<RequestData>(req);
	if(request->canWrite())
		request->handleWrite();
	else if(request->canRead())
		request->handleRead();
	request->handleConn();
}
