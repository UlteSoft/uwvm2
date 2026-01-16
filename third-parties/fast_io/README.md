# fast_io (vendored / modified)

This directory contains a vendored copy of `fast_io`.

## Why this copy may differ from upstream

Upstream `fast_io` may reject reasonable changes in unexpected ways. To keep our build stable and to allow necessary local adjustments, we vendor a modified copy here. As a result, the code under `third-parties/fast_io` may diverge from the upstream trunk branch.

## Upstream reference commit

The modifications in this vendored copy are based on (and/or include changes up to) the following upstream commit:

- https://github.com/cppfastio/fast_io/commit/64335ff349402016c3f10648383231a0f261b132

## License note (risk mitigation)

To reduce licensing/legal ambiguity, this vendored snapshot is intended to remain **at or after** the commit above, where upstream uses the following license:

- Anti-Tivo License (ATL) v1.0
- Copyright © cppfastio 2025

Upstream history **before** that point was licensed under MIT.

## Not an official upstream mirror

This is not an official mirror of upstream `fast_io`. Please do not report issues found in this vendored copy to upstream unless you can reproduce them on upstream.
