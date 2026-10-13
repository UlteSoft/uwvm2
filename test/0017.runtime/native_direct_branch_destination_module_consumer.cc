import uwvm2test.native_branch_destination;
static_assert(branch_destination_data_not_permission());
int main() { return branch_destination_data_not_permission() ? 0 : 1; }
