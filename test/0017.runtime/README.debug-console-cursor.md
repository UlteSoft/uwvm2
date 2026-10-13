# Console cursor regression

`debug_console_cursor.cc` exercises the production `read_console_line`, native
keyboard owner and ordinary/TUI renderers. It starts no guest VM. Build it with
C++26, the repository and FastIO/bizwen/Boost include paths, `-DUWVM=2`,
`-DUWVM_DISABLE_INT -DUWVM_DISABLE_JIT`, the normal version definitions and
`-pthread`. These flags isolate the input component; they do not change ROS
product modes or qualify JIT execution.

Run only in the original SSH Linux 64 GiB keeper cgroup:

```sh
python3 test/0017.runtime/run_debug_console_cursor.py \
  --binary /path/to/debug_console_cursor \
  --expected-cgroup /original/cgroup/path \
  --output /owned/tmp/cursor-result.json
```

The runner uses real PTYs and production input/render code. It verifies CSI and
SS3 arrows, insertion/deletion in the middle, UTF-8/CJK, Home/End, history,
horizontal scrolling, resize and terminals of 1/2/8/14 columns, for ordinary
input and TUI. It checks the final submitted text and restored termios. Its
small VT model checks rendered text/cursor coordinates and deliberately ignores
optional CSI s/u; this catches the previous cursor-restoration dependency.
The unit portion checks every viewport width from 1 to 240 cells and a UTF-8
history-search prompt. This is console input coverage, not a guest/native-debug
or full product integration qualification.
