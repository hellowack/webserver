#pragma once

const int THREADPOOL_INVALID = -1;
const int THREADPOOL_LOCK_FAILURE = -2;
const int THREADPOOL_QUEUE_FULL = -3;
const int THREADPOOL_SHUTDOWN = -4;
const int THREADPOOL_THREAD_FAILURE = -5;
const int THREADPOOL_GRACEFUL = 1;

const int THREAD_MAX_NUM = 1024;
const int QUEUE_MAX_SIZE = 65535;

struct ThreadTask
{
	std::function<void(std::shared_ptr<void>)> fun;
	std::shared_ptr<void> args;
};

typedef enum
{
    immediate_shutdown = 1,
    graceful_shutdown  = 2
} ShutDownOption;

void myHandler(std::shared_ptr<void> req);

class ThreadPool
{
private:
	static pthread_mutex_t lock;
	static pthread_cond_t notify;

	static std::vector<pthread_t> threads;
	static std::vector<ThreadTask> queue;
	static int thread_count;
	static int queue_size;
	static int head;
	//tail points to the next node of queue
	static int tail;
	static int count;
	static bool shutdown;
	static int start_num;
	
public:
	static int threadpool_create(int thread_count, int queue_size);
	static int threadpool_add(std::shared_ptr<void> args, std::function<void(std::shared_ptr<void>)> fun = myHandler);
	static void* threadpool_main(void *args);
};
