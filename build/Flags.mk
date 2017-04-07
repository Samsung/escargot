#######################################################
# common flags
#######################################################
ESCARGOT_CXXFLAGS_COMMON += -DESCARGOT
ESCARGOT_CXXFLAGS_COMMON += -std=c++0x -g3
ESCARGOT_CXXFLAGS_COMMON += -fno-rtti -fno-math-errno -I$(ESCARGOT_ROOT)/src/
ESCARGOT_CXXFLAGS_COMMON += -fdata-sections -ffunction-sections
ESCARGOT_CXXFLAGS_COMMON += -frounding-math -fsignaling-nans
ESCARGOT_CXXFLAGS_COMMON += -fno-omit-frame-pointer

ESCARGOT_CXXFLAGS_COMMON += -Wno-unused-but-set-variable -Wno-unused-but-set-parameter -Wno-unused-parameter
ESCARGOT_CXXFLAGS_COMMON += -Wno-type-limits -Wno-unused-result -Wno-unused-variable -Wno-invalid-offsetof
ESCARGOT_CXXFLAGS_COMMON += -Wno-deprecated-declarations

ifeq (,$(findstring cctest, $(MAKECMDGOALS)))
  ESCARGOT_CXXFLAGS_COMMON += -DESCARGOT_ENABLE_PROMISE
endif

ESCARGOT_CXXFLAGS_COMMON += -DESCARGOT_ENABLE_TYPEDARRAY

#ESCARGOT_CXXFLAGS_COMMON += -DPROFILE_MASSIF
#ESCARGOT_CXXFLAGS_COMMON += -DPROFILE_BDWGC
ifeq ($(HOST), linux)
  ESCARGOT_LDFLAGS_COMMON += -lpthread
  ESCARGOT_LDFLAGS_COMMON += -lrt
else ifeq ($(HOST), tizen_obs)
  ESCARGOT_LDFLAGS_COMMON += -lpthread
  ESCARGOT_LDFLAGS_COMMON += -lrt
else ifneq (,$(findstring tizen_,$(HOST)))
  ESCARGOT_LDFLAGS_COMMON += -lpthread
  ESCARGOT_LDFLAGS_COMMON += -lrt
else ifeq ($(HOST), android)
  ESCARGOT_CXXFLAGS_COMMON +=  -fPIE -march=armv7-a -mfloat-abi=softfp -mfpu=neon
  ESCARGOT_LDFLAGS_COMMON +=  -fPIE -pie  -march=armv7-a -Wl,--fix-cortex-a8
endif

#######################################################
# flags for $(HOST) : linux / tizen*
#######################################################

ESCARGOT_CXXFLAGS_TIZEN += -DESCARGOT_SMALL_CONFIG=1 -DESCARGOT_TIZEN

#######################################################
# flags for $(ARCH) : x64/x86/arm
#######################################################
ESCARGOT_CXXFLAGS_X64 += -DESCARGOT_64=1
ESCARGOT_LDFLAGS_X64 =

# https://gcc.gnu.org/onlinedocs/gcc-4.8.0/gcc/i386-and-x86_002d64-Options.html
ESCARGOT_CXXFLAGS_X86 += -DESCARGOT_32=1
ifneq ($(HOST),tizen_obs)
ESCARGOT_CXXFLAGS_X86 += -m32 -mfpmath=sse -msse -msse2
ESCARGOT_LDFLAGS_X86 += -m32
endif

ESCARGOT_CXXFLAGS_ARM += -DESCARGOT_32=1
ifneq ($(HOST),tizen_obs)
ESCARGOT_CXXFLAGS_ARM += -march=armv7-a -mthumb
ESCARGOT_LDFLAGS_ARM =
endif

#######################################################
#######################################################
# flags for $(MODE) : debug/release
#######################################################
ESCARGOT_CXXFLAGS_DEBUG += -O0 -D_GLIBCXX_DEBUG -Wall -Wextra -Werror
ESCARGOT_CXXFLAGS_RELEASE += -O2 -DNDEBUG -fno-stack-protector

ifneq (,$(findstring tizen,$(HOST)))
  ESCARGOT_CXXFLAGS_RELEASE += -Os -finline-limit=64
  ifeq ($(HOST),tizen_obs)
    ESCARGOT_CXXFLAGS_DEBUG += -O1 # _FORTIFY_SOURCE requires compiling with optimization
  endif
endif

#######################################################
# flags for $(OUTPUT) : bin/shared_lib/static_lib
#######################################################
ESCARGOT_CXXFLAGS_BIN += -fvisibility=hidden -DESCARGOT_STANDALONE
ESCARGOT_LDFLAGS_BIN += -Wl,--gc-sections

ESCARGOT_CXXFLAGS_SHAREDLIB += -fPIC
ESCARGOT_LDFLAGS_SHAREDLIB += -ldl

ESCARGOT_CXXFLAGS_STATICLIB += -fPIC
ESCARGOT_LDFLAGS_STATICLIB += -Wl,--gc-sections

#######################################################
# flags for LTO
#######################################################
ESCARGOT_CXXFLAGS_LTO += -flto -ffat-lto-objects
ESCARGOT_LDFLAGS_LTO += -flto

#######################################################
# flags for TEST
#######################################################
ESCARGOT_CXXFLAGS_VENDORTEST += -DESCARGOT_ENABLE_VENDORTEST

#######################################################
# flags for $(THIRD_PARTY)
#######################################################
# icu
ifeq ($(HOST), linux)
  ESCARGOT_CXXFLAGS_COMMON += -DENABLE_ICU
  ifeq ($(ARCH), x64)
	ESCARGOT_CXXFLAGS_THIRD_PARTY += $(shell pkg-config --cflags icu-i18n)
	ESCARGOT_LDFLAGS_THIRD_PARTY += $(shell pkg-config --libs icu-i18n)
  else ifeq ($(ARCH), x86)
	ESCARGOT_CXXFLAGS_THIRD_PARTY += -I$(ESCARGOT_ROOT)/deps/x86-linux/include
	ESCARGOT_LDFLAGS_THIRD_PARTY += -Ldeps/x86-linux/lib -Wl,-rpath,'$$ORIGIN/deps/x86-linux/lib/'
	ESCARGOT_LDFLAGS_THIRD_PARTY += -licuio -licui18n -licuuc -licudata
  endif
else ifeq ($(HOST), tizen_obs)
  ESCARGOT_CXXFLAGS_COMMON += -DENABLE_ICU
	ESCARGOT_CXXFLAGS_THIRD_PARTY += $(shell pkg-config --cflags icu-uc icu-i18n)
	ESCARGOT_LDFLAGS_THIRD_PARTY += $(shell pkg-config --libs icu-uc icu-i18n)
else ifneq (,$(findstring tizen_,$(HOST)))
  ESCARGOT_CXXFLAGS_COMMON += -DENABLE_ICU
  ifeq ($(ARCH), arm)
	ESCARGOT_CXXFLAGS_THIRD_PARTY += -I$(ESCARGOT_ROOT)/deps/tizen/include
	ESCARGOT_LDFLAGS_THIRD_PARTY += -Ldeps/tizen/lib/tizen-wearable-$(VERSION)-target-arm
	ESCARGOT_LDFLAGS_THIRD_PARTY += -licuio -licui18n -licuuc -licudata
  else ifeq ($(ARCH), i386)
	ESCARGOT_CXXFLAGS_THIRD_PARTY += -I$(ESCARGOT_ROOT)/deps/tizen/include
	ESCARGOT_LDFLAGS_THIRD_PARTY += -Ldeps/tizen/lib/tizen-wearable-$(VERSION)-emulator-x86
	ESCARGOT_LDFLAGS_THIRD_PARTY += -licuio -licui18n -licuuc -licudata
  endif
endif

# bdwgc
ESCARGOT_CXXFLAGS_THIRD_PARTY += -I$(ESCARGOT_ROOT)/third_party/GCutil/bdwgc/include/
ifeq ($(MODE), debug)
  ESCARGOT_CXXFLAGS_THIRD_PARTY += -DGC_DEBUG
endif

# checked arithmetic
ESCARGOT_CXXFLAGS_THIRD_PARTY += -I$(ESCARGOT_ROOT)/third_party/checked_arithmetic/

# v8's fast-dtoa
ESCARGOT_CXXFLAGS_THIRD_PARTY += -I$(ESCARGOT_ROOT)/third_party/double_conversion/

# rapidjson
ESCARGOT_CXXFLAGS_THIRD_PARTY += -I$(ESCARGOT_ROOT)/third_party/rapidjson/include/

# yarr
ESCARGOT_CXXFLAGS_THIRD_PARTY += -I$(ESCARGOT_ROOT)/third_party/yarr/

# GCutil
ESCARGOT_CXXFLAGS_THIRD_PARTY += -I$(ESCARGOT_ROOT)/third_party/GCutil/


