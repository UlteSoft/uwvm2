# Threaded profile qualification source

The current test source is [qualification-r3](qualification-r3/README.md),
with manifest SHA256
`441b604a8b37f3b51f6c1f311321997c1e712d41bab3a3c598b1f34681163b58`.
It declares 43 CLI cases: 39 applicable to INT and 43 to LLVM, plus the real
public-API embedding workflows. These are planned cases, with zero new builds,
WAT conversions, Guest runs, native runs or qualification results.

The added cold-rejection case first requires independent full validation of the
same Wasm, then checks actual profile rejection for a defined local memory
under a zero local-memory budget. Its paired existing zero-local positive case
remains necessary. It does not invent child counts or claim a specific internal
byte-loop branch was reached.

All execution must use the coordinated SSH Linux window and the existing
64 GiB cgroup. Pin the actual final source/build/product, tools, boot, cgroup and
runner manifest. Record failures and real ownership/retirement. Ordinary and
ROS language/DBG/security/performance regressions require fresh final products;
this packet does not qualify them. The current threaded product rejects the
ordinary DBG channel, so child-frame DBG remains an implementation and test
requirement.

The packet retains its private authoring-time provenance byte for byte. Its
README's statement that public repositories were not changed refers to that
earlier authoring stage. This directory now contains an explicitly recorded
local source installation in all three repositories. Older qualification-r2 and
NEXT-LINUX-COHORT.md files, where present, remain historical source plans; their
counts and source identities do not describe r3 or the future cache successor.

The [C++ thread/EH foundation](cpp-eh-foundation/README.md) adds actual
std::thread/TLS/exception test source. It has not been compiled or run, does not
qualify upstream rclcpp, and does not supply missing threaded DBG support.
