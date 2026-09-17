// Metadata-only CMake target. Never compiled, linked or executed by xmake.
// CMake's file API describes how an actual ROS consumer links patched LLVM.
int main() { return 0; }
