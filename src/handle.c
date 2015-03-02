#include <curl/curl.h>
#include <Rinternals.h>
#include <stdlib.h>
#include "utils.h"

void fin_handle(SEXP ptr){
  reference *ref = (reference*) R_ExternalPtrAddr(ptr);
  if(ref){
    if(ref->headers)
      curl_slist_free_all(ref->headers);
    if(ref->form)
      curl_formfree(ref->form);
    if(ref->handle)
      curl_easy_cleanup(ref->handle);
    free(ref);
  }
  R_ClearExternalPtr(ptr);
}

/* These are defaulst that we always want to set */
void set_handle_defaults(CURL *handle){

  /* needed to support compressed responses */
  assert(curl_easy_setopt(handle, CURLOPT_ENCODING, "gzip, deflate"));

  /* do not validate SSL certificates by default */
  assert(curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0));
  assert(curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0));

  /* follow redirect */
  assert(curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1));

  /* a sensible timeout (10s) */
  assert(curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, 10*1000));

  /* needed to start the cookie engine */
  assert(curl_easy_setopt(handle, CURLOPT_COOKIEFILE, ""));
  assert(curl_easy_setopt(handle, CURLOPT_FILETIME, 1));

  /* a default user agent */
  assert(curl_easy_setopt(handle, CURLOPT_USERAGENT, "r/curl/jeroen"));
}

SEXP R_new_handle(){
  reference *ref = calloc(1, sizeof(reference));
  ref->handle = curl_easy_init();
  set_handle_defaults(ref->handle);
  SEXP ptr = PROTECT(R_MakeExternalPtr(ref, R_NilValue, R_NilValue));
  R_RegisterCFinalizerEx(ptr, fin_handle, 1);
  setAttrib(ptr, R_ClassSymbol, mkString("curl_handle"));
  UNPROTECT(1);
  return ptr;
}

SEXP R_handle_reset(SEXP ptr){
  //reset all fields
  reference *ref = get_ref(ptr);
  set_form(ref, NULL);
  set_headers(ref, NULL);
  curl_easy_reset(ref->handle);

  //restore default settings
  set_handle_defaults(ref->handle);
  return ScalarLogical(1);
}

SEXP R_handle_setopt(SEXP ptr, SEXP keys, SEXP values){
  CURL *handle = get_handle(ptr);
  SEXP optnames = getAttrib(values, R_NamesSymbol);

  if(!isInteger(keys))
    error("Argument 'names' must be numeric.");

  if(!isVector(values))
    error("Argument 'values' must be List.");

  for(int i = 0; i < length(keys); i++){
    int key = INTEGER(keys)[i];
    const char* optname = CHAR(STRING_ELT(optnames, i));
    SEXP val = VECTOR_ELT(values, i);
    if(val == R_NilValue){
      assert(curl_easy_setopt(handle, key, NULL));
    } else if(key < 10000){
      if(!isNumeric(val)){
        error("Value for %s (%d) must be numeric.", optname, key);
      }
      assert(curl_easy_setopt(handle, key, asInteger(val)));
    } else if(key < 20000){
      if(!isString(val)){
        error("Value for %s (%d) must be a string.", optname, key);
      }
      assert(curl_easy_setopt(handle, key, CHAR(asChar(val))));
    } else {
      error("Option %s (%d) not supported.", optname, key);
    }
  }
  return ScalarLogical(1);
}

SEXP R_handle_setheaders(SEXP ptr, SEXP vec){
  if(!isString(vec))
    error("header vector must be a string.");
  set_headers(get_ref(ptr), vec_to_slist(vec));
  return ScalarLogical(1);
}

SEXP R_handle_setform(SEXP ptr, SEXP form){
  if(!isVector(form))
    error("Form must be a list.");
  set_form(get_ref(ptr), make_form(form));
  return ScalarLogical(1);
}