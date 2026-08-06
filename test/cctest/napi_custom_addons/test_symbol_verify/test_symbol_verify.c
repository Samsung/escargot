#include <js_native_api.h>
#include <stdlib.h>
#include "common.h"
#include "entry_point.h"

static napi_value CreateSymbolFor(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  char str[256];
  size_t length;
  NODE_API_CALL(env, napi_get_value_string_utf8(env, args[0], str, sizeof(str), &length));

  napi_value result;
  // Use node_api_symbol_for to resolve against the global registry
  NODE_API_CALL(env, node_api_symbol_for(env, str, length, &result));
  return result;
}

static napi_value CreateLocalSymbol(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  napi_value result;
  napi_value desc;
  NODE_API_CALL(env, napi_create_string_utf8(env, "local", NAPI_AUTO_LENGTH, &desc));
  NODE_API_CALL(env, napi_create_symbol(env, desc, &result));
  return result;
}

static void FinalizeCallback(napi_env env, void* finalize_data, void* finalize_hint) {
  napi_ref cb_ref = (napi_ref)finalize_hint;
  napi_value cb;
  napi_value global;
  
  if (napi_get_reference_value(env, cb_ref, &cb) != napi_ok) return;
  if (napi_get_global(env, &global) != napi_ok) return;
  
  napi_call_function(env, global, cb, 0, NULL, NULL);
  napi_delete_reference(env, cb_ref);
}

static napi_value AttachWeakFinalizer(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
  
  napi_value target_symbol = args[0];
  napi_value js_cb = args[1];
  
  napi_ref cb_ref;
  NODE_API_CALL(env, napi_create_reference(env, js_cb, 1, &cb_ref));

  // Attach finalizer onto the symbol itself
  NODE_API_CALL(env, napi_add_finalizer(env, target_symbol, NULL, FinalizeCallback, cb_ref, NULL));
  
  return NULL;
}

napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NODE_API_PROPERTY("createSymbolFor", CreateSymbolFor),
    DECLARE_NODE_API_PROPERTY("createLocalSymbol", CreateLocalSymbol),
    DECLARE_NODE_API_PROPERTY("attachWeakFinalizer", AttachWeakFinalizer),
  };
  NODE_API_CALL(env, napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));
  return exports;
}


