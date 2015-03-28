#include "thread_safe_queue_test.hpp"
#include "thread_safe_queue.hpp"
#include "threads.hpp"
#include <assert.h>

ThreadSafeQueue<int> *queue = nullptr;

static void assert_no_err(int err) {
    if (err)
        panic("Error: %s", genesis_error_string(err));
}

static void worker_thread_1(void *userdata) {
    queue->enqueue(13);
}

static void worker_thread_2(void *userdata) {
    queue->enqueue(17);
}

/*
static void worker_thread_3(void *userdata) {
    queue->dequeue();
}
*/

void test_thread_safe_queue(void) {
    queue = create<ThreadSafeQueue<int>>();
    assert_no_err(queue->resize(20));

    queue->enqueue(25);
    int item = queue->dequeue();

    assert(item == 25);

    Thread thread1;
    assert_no_err(thread1.start(worker_thread_1, nullptr));

    Thread thread2;
    assert_no_err(thread2.start(worker_thread_2, nullptr));

    assert(queue->dequeue() + queue->dequeue() == 13 + 17);

    /*
    Thread thread3;
    assert_no_err(thread3.start(worker_thread_3, nullptr));

    queue->wakeup_all();
    */

    thread1.join();
    thread2.join();
    //thread3.join();

    destroy(queue, 1);
}
