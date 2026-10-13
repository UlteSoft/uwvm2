"""Evidence checks for real Wasm debugger replies; no runtime emulation."""
import re


def query_evidence(command, reply, *, participant, stop, module=0):
    """Do not count errors, stale replies or unavailable state as supported queries."""
    result = {"command": command, "available": False, "rows": 0}
    if not isinstance(reply, bytes) or len(reply) > 2**20:
        result["reason"] = "invalid or oversized reply"
        return result
    if re.search(rb"^error:", reply, re.M):
        result["reason"] = "debugger returned an error"
        return result
    if command.startswith("bt "):
        ids = re.findall(rb"^stop-id ([0-9]+)$", reply, re.M)
        threads = re.findall(rb"^thread ([0-9]+) module=([0-9]+) ", reply, re.M)
        frames = re.findall(rb"^  #([0-9]+) module=", reply, re.M)
        valid = (ids == [str(stop).encode()] and threads == [(str(participant).encode(), str(module).encode())]
                 and frames and frames[0] == b"0"
                 and b"backtrace unavailable for this stop" not in reply)
        result.update(available=bool(valid), rows=len(frames))
    else:
        ids = re.findall(rb"^wasm-stop ([0-9]+)$", reply, re.M)
        headers = re.findall(rb"^Wasm state thread=([0-9]+) module=([0-9]+) epoch=([0-9]+) first=([0-9]+) total=([0-9]+)$", reply, re.M)
        valid = (ids == [str(stop).encode()] and len(headers) == 1
                 and headers[0][0] == str(participant).encode() and headers[0][1] == str(module).encode()
                 and int(headers[0][2]) != 0 and int(headers[0][3]) == 0
                 and not re.search(rb"^Wasm state unavailable:", reply, re.M))
        row_labels = {"locals": b"local", "operands": b"operand", "globals": b"global", "controls": b"control", "handlers": b"handler", "saved": b"saved-parameter"}
        label = row_labels.get(command.split()[0])
        indices = re.findall(rb"^" + label + rb" ([0-9]+) ", reply, re.M) if label else []
        total = int(headers[0][4]) if len(headers) == 1 else 0
        valid = valid and label is not None and [int(v) for v in indices] == list(range(min(total, 16)))
        result.update(available=bool(valid), rows=len(indices), total_values=total)
    if not result["available"]:
        result["reason"] = "missing, unavailable or mismatched current state"
    return result
