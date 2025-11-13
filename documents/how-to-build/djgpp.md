# DJGPP

## Prerequisites
- Install xmake: https://xmake.io
- Install the DJGPP cross GCC toolchain and make sure it is in PATH.
  - Example triplet names used by distributions: `i586-msdosdjgpp-*` (some packages spell it similarly to what you mentioned: `i586-msdosdjgpp-*`).
  - Ensure you have: `i586-msdosdjgpp-gcc`, `i586-msdosdjgpp-g++`, `i586-msdosdjgpp-ar`, `i586-msdosdjgpp-as`, `i586-msdosdjgpp-ranlib`.
  - Reference: https://www.delorie.com/djgpp/

## Examples
```shell
# Configure to use the DJGPP GCC toolchain
xmake f -m release \
  --cc=i586-msdosdjgpp-gcc \
  --cxx=i586-msdosdjgpp-g++ \
  --ar=i586-msdosdjgpp-ar \
  --as=i586-msdosdjgpp-as \
  --ld=i586-msdosdjgpp-gcc \
  --ranlib=i586-msdosdjgpp-ranlib
# or: $ xmake f -m release --sdk=$MSDOSDJGPP_TOOLCHAIN_PATH

# Build
xmake

# Install (adjust path as needed)
xmake i -o ./out/djgpp
```

### Additional Options
- `--static` Static links (typical for DOS/DJGPP builds)
- `--march` Defaults to native; adjust as needed
- `--use-cxx-module=y` Use C++ modules if your compiler supports them

## Caveats
- DOS environment limitations: no dynamic linking, limited APIs.
- Some POSIX features may be unavailable; static linking is the norm.
