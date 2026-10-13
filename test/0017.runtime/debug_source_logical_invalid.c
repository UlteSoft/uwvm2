// Independently rejected by the native compiler, including dead RHS operands.
#if UWVM_INVALID_CASE == 0
int main(void) { return 0 && (1.0 % 1); }
#elif UWVM_INVALID_CASE == 1
int main(void) { return 1 || (1.0 << 1); }
#elif UWVM_INVALID_CASE == 2
int main(void) { return 0 && ~1.0; }
#elif UWVM_INVALID_CASE == 3
int main(void) { return 1 || (1.0 | 1); }
#elif UWVM_INVALID_CASE == 4
int main(void) { return 0 && missing; }
#elif UWVM_INVALID_CASE == 5
int main(void) { return 1 || missing; }
#else
#error Select one of the six invalid logical expressions.
#endif
