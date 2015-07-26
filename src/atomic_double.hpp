#ifndef ATOMIC_DOUBLE_HPP
#define ATOMIC_DOUBLE_HPP

#include <atomic>

using std::atomic;

class AtomicDouble {
public:
    AtomicDouble() { }

    void add(double x) {
        double current_value = _value.load();
        while (!_value.compare_exchange_weak(current_value, current_value + x)) {}
    }

    void store(double x) {
        _value.store(x);
    }

    double load() const {
        return _value.load();
    }

private:
    atomic<double> _value;
};

#endif
