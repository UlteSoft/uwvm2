static volatile int observed;
__attribute__((noinline, export_name("policy_leaf"))) int policy_leaf(int value) {
    volatile int cookie = value + 10;
    observed = cookie; /* POLICY_LEAF_READY */
    return cookie;
}
__attribute__((noinline, export_name("policy_outer"))) int policy_outer(int value) {
    int child = policy_leaf(value); /* POLICY_PHYSICAL_CALL */
    observed = child; /* POLICY_AFTER_CALL */
    return child;
}
__attribute__((export_name("_start"))) void _start(void) {
    int total = 0;
    for(int value = 0; value != 4; ++value) total += policy_outer(value);
    if(total != 46) __builtin_trap();
}
