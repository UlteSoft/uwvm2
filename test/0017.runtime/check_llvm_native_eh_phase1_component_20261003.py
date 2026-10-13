#!/usr/bin/env python3
"""Strict semantic receipt only; provider/ELF/cgroup authority is external."""
import argparse
import json
from pathlib import Path


def semantic_receipt(payload: object, *, observer_enabled: bool) -> dict:
    keys = {"family", "observer_enabled", "checks", "cases", "workers", "rounds",
            "reported_frames", "handler0_c_frames", "original_cfi_matched_frames",
            "first_search_finishes", "first_search_prefix_only",
            "complete_original_trace_observed", "diagnostic_replacement",
            "vm_qualified", "performance_qualified"}
    if type(payload) is not dict or set(payload) != keys:
        raise ValueError("wrong component schema/extra or missing fields")
    if payload["family"] != "native-eh-phase1-observer-component":
        raise ValueError("wrong component family")
    for key, value in {"observer_enabled": observer_enabled, "first_search_prefix_only": True,
                       "complete_original_trace_observed": False, "diagnostic_replacement": False,
                       "vm_qualified": False, "performance_qualified": False}.items():
        if type(payload[key]) is not bool or payload[key] is not value:
            raise ValueError(f"wrong exact boolean {key}")
    for key in ("checks", "cases", "workers", "rounds", "reported_frames",
                "handler0_c_frames", "original_cfi_matched_frames", "first_search_finishes"):
        if type(payload[key]) is not int or not 0 <= payload[key] <= (1 << 32) - 1:
            raise ValueError(f"invalid uint32 {key}")
    if (payload["cases"], payload["workers"], payload["rounds"]) != (150, 4, 32):
        raise ValueError("wrong actual semantic case/thread extent")
    if payload["checks"] < payload["cases"]:
        raise ValueError("missing semantic checks")
    if payload["first_search_finishes"] != (136 if observer_enabled else 0):
        raise ValueError("wrong real first-search finish count")
    frames, plain, matched = (payload[key] for key in
                              ("reported_frames", "handler0_c_frames", "original_cfi_matched_frames"))
    if observer_enabled:
        # Exact source-required minimum: serial 5+48+4, then 4*32*4.
        if not 569 <= matched <= plain <= frames <= 136 * 64:
            raise ValueError("missing genuine handler0/original-CFI prefix overlap")
    elif frames != 0 or plain != 0 or matched != 0:
        raise ValueError("OFF control unexpectedly reports observer records")
    return {"schema": "uwvm-native-eh-phase1-component-semantic-receipt-v1",
            "semantic_payload_valid": True, "observer_enabled": observer_enabled,
            "cases": payload["cases"], "first_search_finishes": payload["first_search_finishes"],
            "actual_provider_authenticated": False, "actual_binary_authenticated": False,
            "actual_cgroup_authenticated": False, "complete_original_trace_qualified": False,
            "vm_qualified": False, "performance_qualified": False}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--observer", choices=("on", "off"), required=True)
    args = parser.parse_args()
    data = json.loads(args.input.read_text())
    print(json.dumps(semantic_receipt(data, observer_enabled=args.observer == "on"),
                     indent=2, sort_keys=True))
