arm-linux-androideabi-gcc-4.7 --sysroot=/opt/android-ndk/platforms/android-14/arch-arm/ -static init.c -o init
arm-linux-androideabi-strip init

#gcc -static init.c -o init
#strip init

arm-linux-androideabi-gcc-4.7 --sysroot=/opt/android-ndk/platforms/android-14/arch-arm/ -static init-chainload.c -o init-chainload
arm-linux-androideabi-strip init-chainload
