#!/usr/bin/env bash
# Admit this actual process in the remote Linux cgroup. A host controller and
# a container namespace may expose different roots of the same cgroup v2 tree.
uwvm_test_fail() { echo "ERROR: $1" >&2; exit 2; }
IFS= read -r uwvm_test_kernel < /proc/sys/kernel/ostype 2>/dev/null ||
    uwvm_test_fail 'this test requires the remote Linux sandbox'
[[ $uwvm_test_kernel == Linux ]] || uwvm_test_fail 'this test requires the remote Linux sandbox'

uwvm_test_v2_path= uwvm_test_v2_count=0
while IFS=: read -r uwvm_test_hierarchy uwvm_test_controllers uwvm_test_path; do
    if [[ $uwvm_test_hierarchy == 0 && -z $uwvm_test_controllers ]]; then
        uwvm_test_v2_path=$uwvm_test_path
        ((uwvm_test_v2_count += 1))
    fi
done < /proc/self/cgroup
[[ $uwvm_test_v2_count == 1 && $uwvm_test_v2_path == /* &&
   $uwvm_test_v2_path != *\\* && ! $uwvm_test_v2_path =~ [[:cntrl:]] ]] ||
    uwvm_test_fail 'one unambiguous actual unified cgroup path is required'
case /${uwvm_test_v2_path#/}/ in
    */../*|*/./*) uwvm_test_fail 'invalid actual cgroup path' ;;
esac

uwvm_test_mount_root= uwvm_test_mount_count=0
while IFS= read -r uwvm_test_mount_line; do
    read -r -a uwvm_test_fields <<< "$uwvm_test_mount_line"
    [[ ${#uwvm_test_fields[@]} -ge 8 && ${uwvm_test_fields[4]} == /sys/fs/cgroup ]] || continue
    for ((uwvm_test_i=6; uwvm_test_i<${#uwvm_test_fields[@]}-1; ++uwvm_test_i)); do
        if [[ ${uwvm_test_fields[uwvm_test_i]} == - && ${uwvm_test_fields[uwvm_test_i+1]} == cgroup2 ]]; then
            uwvm_test_mount_root=${uwvm_test_fields[3]}
            ((uwvm_test_mount_count += 1))
            break
        fi
    done
done < /proc/self/mountinfo
[[ $uwvm_test_mount_count == 1 && $uwvm_test_mount_root == /* &&
   $uwvm_test_mount_root != *\\* && ! $uwvm_test_mount_root =~ [[:cntrl:]] ]] ||
    uwvm_test_fail 'one unambiguous visible cgroup v2 mount is required'
case /${uwvm_test_mount_root#/}/ in
    */../*|*/./*) uwvm_test_fail 'invalid visible cgroup mount root' ;;
esac

# Namespace-relative and mount-root-relative spelling are only candidates.
# The kernel's cgroup.procs MUST contain this exact live Bash PID; neither a
# guessed root nor an ancestor's effective resource limits grants admission.
uwvm_test_candidates=("/sys/fs/cgroup${uwvm_test_v2_path%/}")
if [[ $uwvm_test_v2_path == "$uwvm_test_mount_root" ]]; then
    uwvm_test_candidates+=(/sys/fs/cgroup)
elif [[ $uwvm_test_mount_root != / && $uwvm_test_v2_path == "$uwvm_test_mount_root/"* ]]; then
    uwvm_test_candidates+=("/sys/fs/cgroup${uwvm_test_v2_path#"$uwvm_test_mount_root"}")
fi
uwvm_test_cgroup=
for uwvm_test_candidate in "${uwvm_test_candidates[@]}"; do
    [[ -r $uwvm_test_candidate/cgroup.procs ]] || continue
    uwvm_test_member=false
    while IFS= read -r uwvm_test_pid; do
        [[ $uwvm_test_pid == "$BASHPID" ]] && uwvm_test_member=true
    done < "$uwvm_test_candidate/cgroup.procs"
    if [[ $uwvm_test_member == true ]]; then
        [[ -z $uwvm_test_cgroup || $uwvm_test_cgroup == "$uwvm_test_candidate" ]] ||
            uwvm_test_fail 'ambiguous actual process cgroup membership'
        uwvm_test_cgroup=$uwvm_test_candidate
    fi
done
[[ -n $uwvm_test_cgroup && -r $uwvm_test_cgroup/memory.max &&
   -r $uwvm_test_cgroup/memory.swap.max && -r $uwvm_test_cgroup/cpuset.cpus.effective ]] ||
    uwvm_test_fail 'actual process cgroup with memory and cpuset limits is required'
IFS= read -r uwvm_test_limit < "$uwvm_test_cgroup/memory.max" || uwvm_test_fail 'cannot read actual memory limit'
IFS= read -r uwvm_test_swap < "$uwvm_test_cgroup/memory.swap.max" || uwvm_test_fail 'cannot read actual swap limit'
IFS= read -r uwvm_test_cpus < "$uwvm_test_cgroup/cpuset.cpus.effective" || uwvm_test_fail 'cannot read actual CPU set'
[[ $uwvm_test_limit =~ ^[0-9]{1,11}$ ]] || uwvm_test_fail 'require at most 64 GiB and no swap'
[[ $((10#$uwvm_test_limit)) -gt 0 && $((10#$uwvm_test_limit)) -le 68719476736 && $uwvm_test_swap == 0 ]] ||
    uwvm_test_fail 'require at most 64 GiB and no swap'
[[ -n ${UWVM_TEST_CPUSET:-} ]] || uwvm_test_fail 'set the verified 16 E-core plus 4 P-core CPU list'
[[ $uwvm_test_cpus == "$UWVM_TEST_CPUSET" ]] ||
    uwvm_test_fail 'effective cgroup CPU set does not match the verified topology'
