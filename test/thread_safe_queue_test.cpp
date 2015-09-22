#include "thread_safe_queue_test.hpp"
#include "thread_safe_queue.hpp"
#include "os.hpp"

ThreadSafeQueue<int> *queue = nullptr;

static void test_assert(bool expr, const char *explain) {
    if (!expr)
        panic("assertion failure: %s", explain);
}

static void assert_no_err(int err) {
    if (err)
        panic("Error: %s", genesis_strerror(err));
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
    OsThread *thread1;
    assert_no_err(os_thread_create(worker_thread_1, nullptr, false, &thread1));

    OsThread *thread2;
    assert_no_err(os_thread_create(worker_thread_2, nullptr, false, &thread2));

    int value_1 = queue->dequeue();
    int value_2 = queue->dequeue();
    if (value_1 + value_2 != 13 + 17)
        panic("wrong dequeue value. Got: %d and %d. Expected 13 and 17 (in any order).", value_1, value_2);

    OsThread *thread3;
    assert_no_err(os_thread_create(dequeue_no_assert, nullptr, false, &thread3));

    queue->wakeup_all();

    os_thread_destroy(thread1);
    os_thread_destroy(thread2);
    os_thread_destroy(thread3);


    // test wraparound
    assert_no_err(queue->resize(5));
    assert_no_err(os_thread_create(dequeue_one_through_five, nullptr, false, &thread1));
    for (int i = 0; i < 5; i += 1) {
        queue->enqueue(i);
    }
    os_thread_destroy(thread1);
    assert_no_err(os_thread_create(dequeue_one_through_five, nullptr, false, &thread1));
    for (int i = 0; i < 5; i += 1) {
        queue->enqueue(i);
    }
    os_thread_destroy(thread1);

    // wakeup_all waking up a blocking reading thread
    assert_no_err(os_thread_create(dequeue_no_assert, nullptr, false, &thread1));
    OsMutex *mutex = ok_mem(os_mutex_create());
    OsCond *cond = ok_mem(os_cond_create());
    os_cond_timed_wait(cond, mutex, 0.001);
    queue->wakeup_all();
    os_thread_destroy(thread1);

    os_mutex_destroy(mutex);
    os_cond_destroy(cond);

    destroy(queue, 1);
}
