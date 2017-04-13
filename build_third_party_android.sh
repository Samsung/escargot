#!/bin/bash
#set -x

if [ ! -f /proc/cpuinfo ]; then
    echo "Is this Linux? Cannot find or read /proc/cpuinfo"
    exit 1
fi
NUMPROC=$(grep 'processor' /proc/cpuinfo | wc -l)

INCREMENTAL=false
if [[ $1 == incremental ]]; then
    INCREMENTAL=true
fi

#LTO=true
LTO=false
#COMPILER_VERSION_MAJOR=4.9
#COMPILER_VERSION_MINOR=4.9.2
COMPILER_VERSION_MAJOR=4.6
COMPILER_VERSION_MINOR=4.6.4

###########################################################
# GC build
###########################################################
cd third_party/GCutil/bdwgc/
if [[ $INCREMENTAL == false ]]; then
    autoreconf -vif
    automake --add-missing
    #./autogen.sh
    #make distclean
fi

# Common flags --------------------------------------------

GCCONFFLAGS_COMMON=" --enable-munmap --disable-parallel-mark --enable-large-config --disable-parallel-mark " # --enable-thread --enable-pthread --enable-large-config --enable-cplusplus"
CFLAGS_COMMON=" -g3 -march=armv7-a -mfloat-abi=softfp -mfpu=neon -mthumb "
CFLAGS_COMMON+=" -DESCARGOT "
CFLAGS_COMMON+=" -fdata-sections -ffunction-sections " # To exclude unused code from final binary
FLAGS_COMMON+=" -DIGNORE_DYNAMIC_LOADING -DGC_DONT_REGISTER_MAIN_STATIC_DATA " # Everything in global data is false reference
LDFLAGS_COMMON=

# HOST flags : linux / wearable / mobile / tv -------------

GCCONFFLAGS_wearable=
CFLAGS_wearable=" -Os "
LDFLAGS_wearable=

# ARCH flags : x86 / arm ----------------------------------
# CAUTION: these flags are NOT used when building for tizen_obs

GCCONFFLAGS_x86=
CFLAGS_x86=" -m32 "
LDFLAGS_x86=" -m32 "

GCCONFFLAGS_arm=
CFLAGS_arm=
LDFLAGS_arm=

# MODE flags : debug / release ----------------------------

GCCONFFLAGS_release=" --disable-debug --disable-gc-debug "
GCCONFFLAGS_debug=" --enable-debug --enable-gc-debug "
CFLAGS_release=' -O2 '
CFLAGS_debug=' -O0 '

# LIBTYPE flags : shared / static -------------------------

GCCONFFLAGS_static=
GCCONFFLAGS_shared=
CFLAGS_static=' -fPIC'
CFLAGS_shared=' -fPIC'

NDK=$ANDROID_NDK_STANDALONE
echo NDK
echo $NDK

function build_gc_for_tizen() {

    for arch in arm; do
    for mode in debug release; do
    for libtype in static shared; do


        TIZEN_SYSROOT=$NDK/sysroot
        COMPILER_PREFIX=arm-linux-androideabi
        TIZEN_TOOLCHAIN=$NDK

        BUILDDIR=out/android/$arch/$mode.$libtype
        rm -rf $BUILDDIR
        mkdir -p $BUILDDIR
        cd $BUILDDIR

        GCCONFFLAGS_HOST=GCCONFFLAGS_$host CFLAGS_HOST=CFLAGS_$host LDFLAGS_HOST=LDFLAGS_$host
        GCCONFFLAGS_ARCH=GCCONFFLAGS_$arch CFLAGS_ARCH=CFLAGS_$arch LDFLAGS_ARCH=LDFLAGS_$arch
        GCCONFFLAGS_MODE=GCCONFFLAGS_$mode CFLAGS_MODE=CFLAGS_$mode LDFLAGS_MODE=LDFLAGS_$mode
        GCCONFFLAGS_LIBTYPE=GCCONFFLAGS_$libtype CFLAGS_LIBTYPE=CFLAGS_$libtype LDFLAGS_LIBTYPE=LDFLAGS_$libtype

        GCCONFFLAGS="$GCCONFFLAGS_COMMON ${!GCCONFFLAGS_HOST} ${!GCCONFFLAGS_ARCH} ${!GCCONFFLAGS_MODE} ${!GCCONFFLAGS_LIBTYPE}"
        CFLAGS="$CFLAGS_COMMON ${!CFLAGS_HOST} ${!CFLAGS_ARCH} ${!CFLAGS_MODE} ${!CFLAGS_LIBTYPE} --sysroot=$TIZEN_SYSROOT "
        LDFLAGS="$LDFLAGS_COMMON ${!LDFLAGS_HOST} ${!LDFLAGS_ARCH} ${!LDFLAGS_MODE} ${!LDFLAGS_LIBTYPE}"

        ../../../../configure --host=$COMPILER_PREFIX $GCCONFFLAGS CFLAGS="$CFLAGS" LDFLAGS="$LDFLAGS" \
            ARFLAGS="$PLUGINFLAGS" NMFLAGS="$PLUGINFLAGS" RANLIBFLAGS="$PLUGINFLAGS" \
            CC=$TIZEN_TOOLCHAIN/bin/$COMPILER_PREFIX-gcc \
            CXX=$TIZEN_TOOLCHAIN/bin/$COMPILER_PREFIX-g++ \
            AR=$TIZEN_TOOLCHAIN/bin/${COMPILER_PREFIX}${TOOLCHAIN_GCC_WRAPPER}-ar \
            NM=$TIZEN_TOOLCHAIN/bin/${COMPILER_PREFIX}${TOOLCHAIN_GCC_WRAPPER}-nm \
            RANLIB=$TIZEN_TOOLCHAIN/bin/${COMPILER_PREFIX}${TOOLCHAIN_GCC_WRAPPER}-ranlib \
            LD=$TIZEN_TOOLCHAIN/bin/$COMPILER_PREFIX-ld
        make -j$NUMPROC

        echo Building bdwgc for android $version $host $arch $mode $libtype done
        cd -
    done
    done
    done
}

build_gc_for_tizen
