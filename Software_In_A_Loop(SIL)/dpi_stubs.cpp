// DPI stub functions required by Renode
// These are called from renode_bus.cpp and must have C linkage

extern "C" {

void handleMainMessage(void* ptr) {
    // Called by Renode at runtime via DPI
}

void handleSenderMessage(void* ptr) {
    // Called by Renode at runtime via DPI
}

void receive(void* ptr) {
    // Called by Renode at runtime via DPI
}

}

// SystemC time stamp required by Verilator
double sc_time_stamp() {
    return 0.0;
}
