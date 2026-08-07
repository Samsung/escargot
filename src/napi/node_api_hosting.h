#ifndef __NODE_API_HOSTING_H__
#define __NODE_API_HOSTING_H__

#include <node_api.h>

#if !defined(ESCARGOT_NAPI_EXPORT)
#if defined(_MSC_VER)
#define ESCARGOT_NAPI_EXPORT __declspec(dllexport)
#else
#define ESCARGOT_NAPI_EXPORT __attribute__((visibility("default")))
#endif
#endif

// Forward declarations for Escargot N-API hosting and loop pumping APIs
typedef struct napi_platform__* napi_platform;

EXTERN_C_START

ESCARGOT_NAPI_EXPORT napi_status NAPI_CDECL
napi_create_platform(int argc, char** argv, napi_platform* result);

ESCARGOT_NAPI_EXPORT napi_status NAPI_CDECL
napi_destroy_platform(napi_platform platform);

ESCARGOT_NAPI_EXPORT napi_status NAPI_CDECL
napi_create_environment(napi_platform platform, napi_env* result);

ESCARGOT_NAPI_EXPORT napi_status NAPI_CDECL
napi_destroy_environment(napi_env env);

ESCARGOT_NAPI_EXPORT napi_status NAPI_CDECL
escargot_napi_perform_microtask_checkpoint(napi_env env);

ESCARGOT_NAPI_EXPORT napi_status NAPI_CDECL
escargot_napi_pump_message_loop(napi_env env, bool* out_has_more_work);

EXTERN_C_END

#endif // __NODE_API_HOSTING_H__
