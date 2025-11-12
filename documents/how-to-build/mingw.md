# MinGW

## Use GCC
1. Install [[xmake]](https://github.com/xmake-io/xmake/)
2. Install [[GCC]](https://sourceforge.net/projects/mingw-w64/)
3. Build
```shell
$ xmake f -m [debug|release|releasedbg|minsizerel]
$ xmake
```
4. Install UWVM2
```shell
$ xmake i -o <install_path>
```

### Additional Options
1. `--static` Static links
2. `--march` The default is native, which uses the cpu designator to control it
3. `--mingw-min` Minimum windows compatible version, supported from Win11 all the way to Win95
4. `--use-cxx-module=y` Use cpp module to compile, compiler may not be supported

## Use LLVM
1. Install [[xmake]](https://github.com/xmake-io/xmake/)
2. Install [[llvm]](https://github.com/llvm/llvm-project/releases)
3. Build
```shell
$ xmake f -m [debug|release|releasedbg|minsizerel] --use-llvm=y
$ xmake
```
4. Install UWVM2
```shell
$ xmake i -o <install_path>
```

### Additional Options
1. `--static` Static links
2. `--march` The default is native, which uses the cpu designator to control it
3. `--mingw-min` Minimum windows compatible version, supported from Win11 all the way to Win95
4. `--use-cxx-module=y` Use cpp module to compile, compiler may not be supported