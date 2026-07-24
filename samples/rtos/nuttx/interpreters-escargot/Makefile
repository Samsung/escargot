############################################################################
# apps/interpreters/escargot/Makefile
#
# This file is the escargot NSH app itself, meant to be dropped into (or
# symlinked as) apps/interpreters/escargot/ of your own NuttX + apps
# checkout -- see docs/porting/RTOS_PORTING_GUIDE.md in the Escargot repo
# this came from (samples/rtos/nuttx/). NuttX itself and the `apps`
# monorepo it plugs into are NOT vendored here (they're not a
# library-style dependency the way FreeRTOS-Kernel is for the FreeRTOS
# sample -- a NuttX app fundamentally needs to live inside a full
# NuttX+apps source tree, not standalone).
#
# The two paths below have NO portable default -- they point at wherever
# YOU keep your own Escargot checkout and your own out-of-tree CMake build
# of it (see ../escargot-lib-cmake/CMakeLists.txt, also in this samples
# directory, for that second piece). Override both, e.g.:
#   make ESCARGOT_ROOT=/path/to/escargot ESCARGOT_PREBUILT=/path/to/escargot-lib-cmake-build
# or export them as environment variables before building NuttX.
############################################################################

include $(APPDIR)/Make.defs

ifndef ESCARGOT_ROOT
$(error ESCARGOT_ROOT is not set -- point it at your own Escargot checkout, e.g. "make ESCARGOT_ROOT=/path/to/escargot ESCARGOT_PREBUILT=/path/to/escargot-lib-cmake-build")
endif
ifndef ESCARGOT_PREBUILT
$(error ESCARGOT_PREBUILT is not set -- point it at your own build directory of ../escargot-lib-cmake (see that CMakeLists.txt in this same samples/rtos/nuttx/ directory))
endif

PROGNAME  = escargot
PRIORITY  = $(CONFIG_INTERPRETERS_ESCARGOT_PRIORITY)
STACKSIZE = $(CONFIG_INTERPRETERS_ESCARGOT_STACKSIZE)
MODULE    = $(CONFIG_INTERPRETERS_ESCARGOT)

MAINSRC = escargot_main.cxx
CXXSRCS = escargot_platform.cxx
# OSAllocator implementation comes from libescargot.a (built by
# ESCARGOT_PREBUILT below, from third_party/yarr/OSAllocatorBaremetal.cpp,
# self-guarded behind OS_BAREMETAL) -- no port-local copy needed here
# anymore (this app used to carry its own, byte-for-byte-identical
# OSAllocatorNuttx.cxx).

CXXFLAGS += -I$(ESCARGOT_ROOT)/src
CXXFLAGS += -I$(ESCARGOT_ROOT)/src/api
CXXFLAGS += -I$(ESCARGOT_ROOT)/build/escargot_generated
CXXFLAGS += -I$(ESCARGOT_ROOT)/third_party
CXXFLAGS += -I$(ESCARGOT_ROOT)/third_party/GCutil/include
CXXFLAGS += -I$(ESCARGOT_ROOT)/third_party/libbf
CXXFLAGS += -I$(ESCARGOT_ROOT)/third_party/robin_map/include
CXXFLAGS += -I$(ESCARGOT_ROOT)/third_party/yarr
CXXFLAGS += -I$(ESCARGOT_ROOT)/third_party/double_conversion
CXXFLAGS += -I$(ESCARGOT_ROOT)/third_party/rapidjson/include
CXXFLAGS += -I$(ESCARGOT_ROOT)/third_party/runtime_icu_binder
CXXFLAGS += -I$(ESCARGOT_ROOT)/third_party/xsum
CXXFLAGS += -I$(ESCARGOT_ROOT)/third_party/simdutf
CXXFLAGS += -I$(ESCARGOT_ROOT)/third_party/checked_arithmetic
CXXFLAGS += -I$(ESCARGOT_ROOT)/third_party/lz4
CXXFLAGS += -fexceptions
CXXFLAGS += -DOS_BAREMETAL=1 -DESCARGOT_SMALL_CONFIG -DNDEBUG
CXXFLAGS += -DESCARGOT -DESCARGOT_32=1

# Drop the prebuilt (external CMake, same arch flags) static libs into
# apps/staging so they get swept up by arch/arm/src/Makefile's
# EXTRA_LIBS += $(wildcard $(APPDIR)/staging/*$(LIBEXT)) at final link.
# NOTE: this must NOT be an install:: rule — in a CONFIG_BUILD_FLAT build
# apps/Makefile's "install" target is never invoked by the normal build
# chain (only "all", via tools/LibTargets.mk's $(APPDIR)/libapps.a rule).
# context:: *is* run automatically (confirmed via the "Register: escargot"
# message during `make context`), so hook the copy there instead.
context::
	$(Q) mkdir -p $(APPDIR)/staging
	$(Q) cp -up $(ESCARGOT_PREBUILT)/libescargot.a $(APPDIR)/staging/
	$(Q) cp -up $(ESCARGOT_PREBUILT)/libgc.a $(APPDIR)/staging/

include $(APPDIR)/Application.mk
