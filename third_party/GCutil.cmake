INCLUDE (ExternalProject)


SET (GC_CFLAGS_COMMON "-g3 -fdata-sections -ffunction-sections -DHAVE_CONFIG_H -DESCARGOT -DIGNORE_DYNAMIC_LOADING -DGC_DONT_REGISTER_MAIN_STATIC_DATA -Wno-unused-variable")

IF (${ESCARGOT_ARCH} STREQUAL "x86")
    SET (GC_CFLAGS_ARCH "-m32")
    SET (GC_LDFLAGS_ARCH "-m32")
ELSEIF (${ESCARGOT_ARCH} STREQUAL "arm")
    SET (GC_CFLAGS_ARCH "-march=armv7-a -mthumb -finline-limit=64")
ENDIF()

IF (${ESCARGOT_MODE} STREQUAL "debug")
    SET (GC_CFLAGS_MODE "-O0 -DGC_DEBUG")
ELSE()
    SET (GC_CFLAGS_MODE "-O2")
ENDIF()

SET (GC_CFLAGS "${GC_CFLAGS_COMMON} ${GC_CFLAGS_ARCH} ${GC_CFLAGS_MODE} $ENV{CFLAGS}")
SET (GC_LDFLAGS "${GC_LDFLAGS_ARCH} ${GC_CFLAGS}")

SET (GC_CONFFLAGS_COMMON --enable-munmap --disable-parallel-mark --enable-large-config --disable-pthread --disable-threads)
IF (${ESCARGOT_MODE} STREQUAL "debug")
    SET (GC_CONFFLAGS_MODE --enable-debug --enable-gc-debug)
ELSE()
    SET (GC_CONFFLAGS_MODE --disable-debug --disable-gc-debug)
ENDIF()
SET (GC_CONFFLAGS
    ${GC_CONFFLAGS_COMMON}
    ${GC_CONFFLAGS_MODE}
)


EXTERNALPROJECT_ADD (gc-project
    URL ${CMAKE_CURRENT_SOURCE_DIR}/GCutil

    CONFIGURE_COMMAND autoreconf -vif <SOURCE_DIR>/bdwgc
              COMMAND <SOURCE_DIR>/bdwgc/configure --prefix=<INSTALL_DIR> ${GC_CONFFLAGS} CFLAGS=${GC_CFLAGS} LDFLAGS=${GC_LDFLAGS}
    BUILD_COMMAND make
    INSTALL_COMMAND make install
    BUILD_BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/install/lib/libgc.a

    DOWNLOAD_DIR CMakeFiles/download
    STAMP_DIR CMakeFiles/stamp
    SOURCE_DIR CMakeFiles/src/gc-project
    BINARY_DIR CMakeFiles/build/gc-project
    TMP_DIR CMakeFiles/tmp
    INSTALL_DIR CMakeFiles/install
)

ADD_LIBRARY (gc STATIC IMPORTED GLOBAL)
SET_TARGET_PROPERTIES (gc PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/install/lib/libgc.a
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/GCutil;${CMAKE_CURRENT_SOURCE_DIR}/GCutil/bdwgc/include"
)
ADD_DEPENDENCIES (gc gc-project)
