#include "state.hpp"
#include "globals.hpp"

bool init_state() {
    // Allocate State on the heap to ensure it outlives this function scope.
    // The previous stack allocation caused a dangling pointer and heap
    // corruption (free(): invalid pointer) shortly after startup.
    if (!ss) ss = new State{};
    ss->mode = modes::TITLE;
    return true;
}

void cleanup_state() {
    if (ss) {
        delete ss;
        ss = nullptr;
    }
}
