#ifndef WARNING_HPP
#define WARNING_HPP

enum Warning {
    WarningHighPriorityThread,

    WarningCount,
};

void emit_warning(Warning warning);

#endif
