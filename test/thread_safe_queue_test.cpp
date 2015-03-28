#include "thread_safe_queue_test.hpp"
#include "thread_safe_queue.hpp"
#include <assert.h>

static void assert_no_err(int err) {
    if (err)
        panic("Error: %s", genesis_error_string(err));
}

void test_thread_safe_queue(void) {
    ThreadSafeQueue<int> queue;
    assert_no_err(queue.resize(20));

    queue.enqueue(25);
    int item = queue.dequeue();

    assert(item == 25);
}
