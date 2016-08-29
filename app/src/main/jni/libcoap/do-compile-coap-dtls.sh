#!/bin/bash

#Start---------
ABI_ARCH=$1
echo "$ABI_ARCH"

if [ -z "$ABI_ARCH" ]; then
    echo "You must specific an architecture 'arm, armv7a, arm64'."
    echo ""
    exit 1
fi

OUTPUT_DIR=$(pwd)/build/$ABI_ARCH

#mkdir $OUTPUT_DIR

NDK=$ANDROID_NDK
FF_CFLAGS=

FF_EXTRA_CFLAGS="-D_GNU_SOURCE -DWITH_POSIX"
FF_EXTRA_LDFLAGS=
SYSROOT=
FF_CC=
FF_CROSS_PLATFORM=
FF_ANDROID_PLATFORM=android-15

#----- armv7a begin -----
if [ "$ABI_ARCH" = "armv7a" ]; then

	FF_EXTRA_CFLAGS="$FF_EXTRA_CFLAGS -march=armv7-a -mcpu=cortex-a8 -mfpu=vfpv3-d16 -mfloat-abi=softfp -mthumb"
    FF_EXTRA_LDFLAGS="$FF_EXTRA_LDFLAGS -Wl,--fix-cortex-a8"
	FF_CROSS_PLATFORM=arm-linux-androideabi
	SYSROOT=$NDK/platforms/$FF_ANDROID_PLATFORM/arch-arm
	#CFLAGS="-mthumb"
#	export CFLAGS="-mthumb -D_GNU_SOURCE -DWITH_POSIX"
#	export LDFLAGS="-Wl,--fix-cortex-a8"
	#export CC="$NDK/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-gcc --sysroot=$SYSROOT"
	FF_CC="$NDK/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-gcc --sysroot=$SYSROOT"
	#export LD="$NDK/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-ld"
	#export PLATFORM=posix

elif [ "$ABI_ARCH" = "armv5" ]; then
    FF_EXTRA_CFLAGS="$FF_EXTRA_CFLAGS -march=armv5te -mtune=arm9tdmi -msoft-float"
    FF_EXTRA_LDFLAGS="$FF_EXTRA_LDFLAGS"
	FF_CROSS_PLATFORM=arm-linux-androideabi
	SYSROOT=$NDK/platforms/$FF_ANDROID_PLATFORM/arch-arm
	FF_CC="$NDK/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-gcc --sysroot=$SYSROOT"

elif [ "$ABI_ARCH" = "arm64" ]; then
	FF_EXTRA_CFLAGS="$FF_EXTRA_CFLAGS"
    FF_EXTRA_LDFLAGS="$FF_EXTRA_LDFLAGS"

	FF_CROSS_PLATFORM=aarch64-linux-android
	SYSROOT=$NDK/platforms/android-21/arch-arm64

	FF_CC="$NDK/toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64/bin/aarch64-linux-android-gcc --sysroot=$SYSROOT"

else
    echo "unknown architecture $ABI_ARCH";
    exit 1
fi

export CC=$FF_CC
export CFLAGS="$FF_CFLAGS $FF_EXTRA_CFLAGS"
export LDFLAGS="$FF_EXTRA_LDFLAGS"
export PLATFORM=posix

./configure \
	ac_cv_func_malloc_0_nonnull=yes \
	ac_cv_func_realloc_0_nonnull=yes \
	--host=$FF_CROSS_PLATFORM \
	--disable-documentation \
	--disable-tests \
	--disable-examples \
	--enable-shared \
	--with-posix \
	--prefix=$OUTPUT_DIR \
	--exec-prefix=$OUTPUT_DIR

make clean
make 
make install

echo "=========finish===="



