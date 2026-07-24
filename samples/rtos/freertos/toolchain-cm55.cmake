set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER   arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_AR           arm-none-eabi-ar)
set(CMAKE_RANLIB       arm-none-eabi-ranlib)
set(CMAKE_OBJCOPY      arm-none-eabi-objcopy)
set(CMAKE_SIZE         arm-none-eabi-size)

# Skip link test during compiler detection
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CPU_FLAGS "-mcpu=cortex-m55 -mthumb -mfloat-abi=hard -mfpu=fpv5-d16")
# -flto: link-time optimization. Applied to every object (this toolchain is used
# for the add_subdirectory()'d engine too), so the linker can inline and drop
# dead code across translation-unit boundaries -- beyond what --gc-sections does
# at section granularity. CMake also passes these flags on the compiler-driver
# link line, so LTO codegen sees the correct -mcpu/-mfpu.
set(CMAKE_C_FLAGS_INIT   "${CPU_FLAGS} -Wall -Os -flto -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS_INIT "${CPU_FLAGS} -Wall -Os -flto -ffunction-sections -fdata-sections -fexceptions -fno-rtti")
