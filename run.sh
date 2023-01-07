#!/bin/bash

set -e

# ------------------------------------------------------------------ Environment

PPWD="$(cd "$(dirname "$0")"; pwd)"

SHELL_SCRIPTS_ROOT_DIR="$PPWD/modules/shell-scripts"
export BUILD_CONTRIB_SCRIPT_DIR="$SHELL_SCRIPTS_ROOT_DIR/build-contrib"
export TOOLCHAIN_CONFIG_DIR="$SHELL_SCRIPTS_ROOT_DIR/toolchain-config"

# ------------------------------------------------------------ Parse Commandline

CONFIG="asan"
TARGET="grpc-p2300"
TOOLCHAIN="gcc"
VERBOSE="False"
NO_BUILD="False"
GDB="False"
LLDB="False"
BUILD_ONLY="False"
BUILD_TESTS="False"
BENCHMARK="False"
BUILD_EXAMPLES="False"
LTO="False"
UNITY_BUILD="False"
VALGRIND="False"
HELGRIND="False"
PYTHON_BINDINGS="False"
COVERAGE="False"
COVERAGE_HTML="False"
RULE="all"
CXXSTD="-std=c++2b"
STDLIB="stdcxx"
COMPDB="False"
TARGET_OVERRIDE=""
MAKEFILE="config/run.makefile"
NPROC="-j$(nproc)"

show_usage()
{
    cat <<EOF

   $(basename $0) [OPTIONS...]* [-- other arguments]?

   Compiler options:
      clang, clang-15, gcc-12 (default)

   Configuration options:
      asan (default), usan, tsan, debug, release, reldbg, valgrind, helgrind, gdb
      
      If gdb, valgrind, or helgrind is selected, then builds debug and 
      runs under the tool.

   Standard library:
      stdcxx        build with gcc's stdcxx
      libcxx        build with llvm's libcxx

   Other options:
      clean         ie, make clean for the configuration
      info          print out important environment variables for the build
      verbose       verbose output
      quiet         no output
      unity         do a unity build
      lto           enable lto
      no-lto        disable lto
      build         build but do not run
      example       build the examples
      test          build and run test cases
      coverage      build and run test cases with text code-coverage output
      coveragehtml  build and run test cases with html code-coverage output

   Examples:

      # Build and run the testcases in tsan mode
      > $(basename $0) tsan test

      # Build and run under gdb
      > $(basename $0) gdb

      # Make a unity build in release mode
      > $(basename $0) unity release

      # Make html test coverage using clang-14, passing arguments "1" "2" "3" to the executable
      > $(basename $0) clang-15 coverage -- 1 2 3

EOF
}

for ARG in "$@" ; do
    [ "$ARG" = "-h" ] || [ "$ARG" = "--help" ] && show_usage && exit 0
done

while [ "$#" -gt "0" ] ; do
    
    # Compiler
    [ "$1" = "clang" ]     && TOOLCHAIN="clang"     && shift && continue
    [ "$1" = "clang-16" ]  && TOOLCHAIN="clang"     && shift && continue
    [ "$1" = "gcc" ]       && TOOLCHAIN="gcc"       && shift && continue
    [ "$1" = "gcc-12" ]    && TOOLCHAIN="gcc"       && shift && continue

    # Configuration
    [ "$1" = "asan" ]      && CONFIG="asan"         && shift && continue
    [ "$1" = "usan" ]      && CONFIG="usan"         && shift && continue
    [ "$1" = "tsan" ]      && CONFIG="tsan"         && shift && continue
    [ "$1" = "debug" ]     && CONFIG="debug"        && shift && continue
    [ "$1" = "reldbg" ]    && CONFIG="reldbg"       && shift && continue
    [ "$1" = "release" ]   && CONFIG="release"      && shift && continue
    [ "$1" = "valgrind" ]  && CONFIG="debug"        && VALGRIND="True" && shift && continue
    [ "$1" = "helgrind" ]  && CONFIG="debug"        && HELGRIND="True" && shift && continue
    [ "$1" = "gdb" ]       && CONFIG="debug"        && GDB="True"      && shift && continue

    # Stdlib
    [ "$1" = "libcxx" ]    && STDLIB="libcxx"       && shift && continue
    [ "$1" = "stdcxx" ]    && STDLIB="stdcxx"       && shift && continue
    
    # Other options
    [ "$1" = "clean" ]     && RULE="clean"          && shift && continue
    [ "$1" = "info" ]      && RULE="info"           && shift && continue
    [ "$1" = "verbose" ]   && VERBOSE="True"        && shift && continue
    [ "$1" = "quiet" ]     && VERBOSE="False"       && shift && continue
    [ "$1" = "unity" ]     && UNITY_BUILD="True"    && shift && continue
    [ "$1" = "lto" ]       && LTO="True"            && shift && continue
    [ "$1" = "no-lto" ]    && LTO="False"           && shift && continue
    [ "$1" = "build" ]     && BUILD_ONLY="True"     && shift && continue    
    [ "$1" = "test" ]      && BUILD_TESTS="True"    && BUILD_EXAMPLES="True" && shift && continue
    [ "$1" = "bench" ]     && BENCHMARK="True"      && shift && continue
    [ "$1" = "examples" ]  && BUILD_EXAMPLES="True" && shift && continue
    [ "$1" = "compdb"   ]  && COMPDB="True" \
        && BUILD_TESTS="True" && BUILD_EXAMPLES="True" && CONFIG="debug" && TOOLCHAIN="clang" \
        && shift && continue
    [ "$1" = "coverage" ]  \
        && BUILD_TESTS="True"  && COVERAGE="True"  && CONFIG="debug" && shift && continue

    [ "${1:0:2}" = "-j" ]  && NPROC="$1"            && shift && continue
    
    [ "$1" = "--" ]        && shift && break
    
    echo "Unexpected keyword: '$1'" 1>&2 && exit 1
done

if [ "$BENCHMARK" = "True" ] && [ "$BUILD_TESTS" = "True" ] ; then
    echo "Cannot benchmark and build tests at the same time."
    exit 1
fi

# ---------------------------------------------------------------------- Execute

export TARGET="$TARGET"
export VERBOSE="$VERBOSE"
export TOOLCHAIN="$TOOLCHAIN"
export BUILD_CONFIG="$CONFIG"
export CXXSTD="-std=c++2b"
export UNITY_BUILD="$UNITY_BUILD"
export BUILD_TESTS="$BUILD_TESTS"
export BUILD_EXAMPLES="$BUILD_EXAMPLES"
export BENCHMARK="$BENCHMARK"
export COVERAGE="$COVERAGE"
export STDLIB="$STDLIB"
export LTO="$LTO"

if [ "$COVERAGE" = "True" ] ; then
    RULE="$([ "$TOOLCHAIN" = "gcc" ] && echo "coverage_html" || echo "llvm_coverage_html")"
fi

if [ "$COMPDB" = "True" ] && [ "$RULE" != "clean" ] ; then
    RULE="compile_commands.json"
fi

do_make()
{    
    make -f "$MAKEFILE" $NPROC $RULE
    RET="$?"
    [ "$RET" != "0" ] && exit $RET   || true
}
do_make

[ "$RULE" = "clean" ]      && exit 0 || true
[ "$RULE" = "info" ]       && exit 0 || true
[ "$BUILD_ONLY" = "True" ] && exit 0 || true
[ "$COVERAGE" = "True" ]   && exit 0 || true
[ "$COMPDB" = "True" ]     && exit 0 || true

if [ "$TARGET_OVERRIDE" = "" ] ; then

    SUPP_DIR="config/suppressions"
    
    export ASAN_OPTIONS="suppressions=$SUPP_DIR/asan.supp,protect_shadow_gap=0,detect_leaks=0"
    export TSAN_OPTIONS="suppressions=$SUPP_DIR/tsan.supp"
    export UBSAN_OPTIONS="suppressions=$SUPP_DIR/usan.supp"
    export TF_CPP_MIN_LOG_LEVEL="1"
    export AUTOGRAPH_VERBOSITY="1"

    if [ "$CONFIG" = "asan" ] ; then
        export MallocNanoZone=0
    fi
    PRODUCT="$(make -f "$MAKEFILE" info | grep -E ^PRODUCT | awk '{ print $2 }')"

    if [ ! -x "$PRODUCT" ] ; then
        rm -f "$PRODUCT"
        exit 1
    fi
    
    source "$BUILD_CONTRIB_SCRIPT_DIR/env/platform-env.sh"
    VALGRIND_EXE="$TOOLS_DIR/bin/valgrind"
    
    RET=0    
    if [ "$VALGRIND" = "True" ] ; then        
        $VALGRIND_EXE --demangle=yes --tool=memcheck --leak-check=full --track-origins=yes --verbose --log-file=valgrind.log --gen-suppressions=all --suppressions=$SUPP_DIR/valgrind.supp "$PRODUCT" "$@"
        RET=$?
        cat valgrind.log | tail -n 1
    elif [ "$HELGRIND" = "True" ] ; then        
        $VALGRIND_EXE --demangle=yes --tool=helgrind --verbose --log-file=helgrind.log --gen-suppressions=all --suppressions=$SUPP_DIR/helgrind.supp "$PRODUCT" "$@"
        RET=$?
        cat helgrind.log | tail -n 1
    elif [ "$GDB" = "True" ] && (( $# != 0 )) ; then        
        gdb -x project-config/gdbinit -silent -return-child-result -statistics --args "$PRODUCT" "$@"
        RET=$?
    elif [ "$GDB" = "True" ] ; then
        gdb "$PRODUCT"
        RET=$?        
    else
        "$PRODUCT" "$@"
        RET=$?        
    fi

    exit $RET

fi



