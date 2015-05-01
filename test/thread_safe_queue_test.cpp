#include "thread_safe_queue_test.hpp"
#include "thread_safe_queue.hpp"
#include "threads.hpp"

ThreadSafeQueue<int> *queue = nullptr;

static void test_assert(bool expr, const char *explain) {
    if (!expr)
        panic("assertion failure: %s", explain);
}

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

static void dequeue_no_assert(void *userdata) {
    queue->dequeue();
}

static void dequeue_one_through_five(void *userdata) {
    for (int i = 0; i < 5; i += 1) {
        test_assert(queue->dequeue() == i, "dequeue one wrong value");
    }
}

void test_thread_safe_queue(void) {
    queue = create<ThreadSafeQueue<int>>();
    assert_no_err(queue->resize(20));

    // basic test
    queue->enqueue(25);
    test_assert(queue->dequeue() == 25, "wrong dequeue value");

    // let's get some threads going test
    Thread thread1;
    assert_no_err(thread1.start(worker_thread_1, nullptr));

    Thread thread2;
    assert_no_err(thread2.start(worker_thread_2, nullptr));

    int value_1 = queue->dequeue();
    int value_2 = queue->dequeue();
    if (value_1 + value_2 != 13 + 17)
        panic("wrong dequeue value. Got: %d and %d. Expected 13 and 17 (in any order).", value_1, value_2);

    Thread thread3;
    assert_no_err(thread3.start(dequeue_no_assert, nullptr));

    queue->wakeup_all();

    thread1.join();
    thread2.join();
    thread3.join();


    // test wraparound
    assert_no_err(queue->resize(5));
    assert_no_err(thread1.start(dequeue_one_through_five, nullptr));
    for (int i = 0; i < 5; i += 1) {
        queue->enqueue(i);
    }
    thread1.join();
    assert_no_err(thread1.start(dequeue_one_through_five, nullptr));
    for (int i = 0; i < 5; i += 1) {
        queue->enqueue(i);
    }
    thread1.join();

    // wakeup_all waking up a blocking reading thread
    assert_no_err(thread1.start(dequeue_no_assert, nullptr));
    Mutex mutex;
    MutexCond cond;
    cond.timed_wait(&mutex, 0.001);
    queue->wakeup_all();
    thread1.join();


    destroy(queue, 1);
}
