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
set(CMAKE_C_FLAGS_INIT   "${CPU_FLAGS} -Wall -Os -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS_INIT "${CPU_FLAGS} -Wall -Os -ffunction-sections -fdata-sections -fexceptions -fno-rtti")
