#ifndef _WIN32
#define WIN32_LEAN_AND_MEAN
#endif
#include "mbjs_runtime.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#ifndef __cdecl
#define __cdecl
#endif
typedef void* HMODULE;
#define LoadLibraryA(p) dlopen((p), RTLD_NOW)
#define FreeLibrary(h) dlclose(h)
#define GetProcAddress(h,n) dlsym(h,n)
/* On bionic, dlopen(NULL) may return NULL; dlsym(NULL,...) then crashes.
 * RTLD_DEFAULT (0) resolves the main program's exported symbols correctly. */
#define GetModuleHandleA(n) ((void*)0)
#endif
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct WasmyValue { uint8_t tag; uint8_t pad[7]; uint64_t payload; } WasmyValue;
typedef int (__cdecl *WasmyHostFunc)(void *ctx, const void *args, size_t nargs, void *result);
typedef void* (__cdecl *P_wasmy_module_parse)(const uint8_t*, size_t);
typedef void  (__cdecl *P_wasmy_module_free)(void*);
typedef void* (__cdecl *P_wasmy_runtime_new)(void*);
typedef void  (__cdecl *P_wasmy_runtime_free)(void*);
typedef int   (__cdecl *P_wasmy_instantiate)(void*);
typedef int   (__cdecl *P_wasmy_register_host)(void*, const char*, const char*, WasmyHostFunc, void*);
typedef int   (__cdecl *P_wasmy_call)(void*, uint32_t, const WasmyValue*, size_t, WasmyValue*, size_t, size_t*);
typedef int   (__cdecl *P_wasmy_export_func_index)(void*, const char*);
typedef uint8_t* (__cdecl *P_wasmy_memory_ptr)(void*);
typedef size_t (__cdecl *P_wasmy_memory_len)(void*);

static MBJSRuntime rt;
static HMODULE hWasmy = NULL;
static P_wasmy_module_parse p_module_parse; static P_wasmy_module_free p_module_free;
static P_wasmy_runtime_new p_runtime_new; static P_wasmy_runtime_free p_runtime_free;
static P_wasmy_instantiate p_instantiate; static P_wasmy_register_host p_register_host;
static P_wasmy_call p_call; static P_wasmy_export_func_index p_export_func_index;
static P_wasmy_memory_ptr p_memory_ptr; static P_wasmy_memory_len p_memory_len;

typedef MBJSValue (*P_NewObject)(MBJSContext*);
typedef MBJSValue (*P_NewCFunction2)(MBJSContext*, MBJSCFunction*, const char*, int, int, int);
typedef int (*P_SetPropertyStr)(MBJSContext*, MBJSValueConst, const char*, MBJSValue);
typedef MBJSValue (*P_GetPropertyStr)(MBJSContext*, MBJSValueConst, const char*);
typedef MBJSValue (*P_NewInt32)(MBJSContext*, int32_t);
typedef MBJSValue (*P_NewInt64)(MBJSContext*, int64_t);
typedef MBJSValue (*P_NewFloat64)(MBJSContext*, double);
typedef int (*P_ToInt32)(MBJSContext*, int32_t*, MBJSValueConst);
typedef int (*P_IsException)(MBJSValueConst);
typedef MBJSValue (*P_GetException)(MBJSContext*);
typedef void (*P_FreeValue)(MBJSContext*, MBJSValue);
typedef MBJSValue (*P_DupValue)(MBJSContext*, MBJSValueConst);
typedef MBJSValue (*P_NewArrayBufferCopy)(MBJSContext*, const uint8_t*, size_t);
typedef uint8_t* (*P_GetArrayBuffer)(MBJSContext*, size_t*, MBJSValueConst);
typedef MBJSValue (*P_NewUint8ArrayCopy)(MBJSContext*, const uint8_t*, size_t);
typedef uint8_t* (*P_GetUint8Array)(MBJSContext*, size_t*, MBJSValueConst);
typedef MBJSValue (*P_GetGlobalObject)(MBJSContext*);
typedef MBJSValue (*P_Call)(MBJSContext*, MBJSValueConst, MBJSValueConst, int, MBJSValueConst*);
typedef int (*P_IsFunction)(MBJSContext*, MBJSValueConst);
typedef int (*P_IsObject)(MBJSValueConst);
typedef int (*P_IsUndefined)(MBJSValueConst);
typedef int (*P_IsNumber)(MBJSValueConst);

static P_NewObject S_NewObject; static P_NewCFunction2 S_NewCFunction2; static P_SetPropertyStr S_SetPropertyStr;
static P_GetPropertyStr S_GetPropertyStr; static P_NewInt32 S_NewInt32; static P_NewInt64 S_NewInt64;
static P_NewFloat64 S_NewFloat64; static P_ToInt32 S_ToInt32; static P_IsException S_IsException;
static P_GetException S_GetException; static P_FreeValue S_FreeValue; static P_DupValue S_DupValue;
static P_NewArrayBufferCopy S_NewArrayBufferCopy; static P_GetArrayBuffer S_GetArrayBuffer;
static P_NewUint8ArrayCopy S_NewUint8ArrayCopy; static P_GetUint8Array S_GetUint8Array;
static P_GetGlobalObject S_GetGlobalObject; static P_Call S_Call; static P_IsFunction S_IsFunction;
static P_IsObject S_IsObject; static P_IsUndefined S_IsUndefined; static P_IsNumber S_IsNumber;

static void *sym(HMODULE h, const char *name) { return (void*)GetProcAddress(h, name); }
typedef MBJSValue (*P_NewNumber)(MBJSContext*, double);
static P_NewNumber S_NewNumber;
static MBJSValue my_NewInt32(MBJSContext *ctx, int32_t v){ (void)ctx; return MBJS_MKVAL(MBJS_TAG_INT, v); }
static MBJSValue my_NewFloat64(MBJSContext *ctx, double v){ if(S_NewNumber) return S_NewNumber(ctx,v); return MBJS_UNDEFINED; }
static MBJSValue my_NewInt64(MBJSContext *ctx, int64_t v){ if(rt.NewBigInt64) return rt.NewBigInt64(ctx,v); if(S_NewNumber) return S_NewNumber(ctx,(double)v); return my_NewInt32(ctx,(int32_t)v); }

static const char *resolve_extra(void) {
    HMODULE host = GetModuleHandleA(NULL); /* RTLD_DEFAULT (0) on POSIX */
#define R(T,N,S) do{ void *p=sym(host,S); if(!p) return S; N=(T)p; }while(0)
#define RO(T,N,S) do{ void *p=sym(host,S); N=(T)p; }while(0)
    R(P_NewObject,S_NewObject,"MBJS_NewObject"); RO(P_NewCFunction2,S_NewCFunction2,"MBJS_NewCFunction2");
    R(P_SetPropertyStr,S_SetPropertyStr,"MBJS_SetPropertyStr"); R(P_GetPropertyStr,S_GetPropertyStr,"MBJS_GetPropertyStr");
    RO(P_NewInt32,S_NewInt32,"MBJS_NewInt32"); RO(P_NewInt64,S_NewInt64,"MBJS_NewInt64");
    RO(P_NewFloat64,S_NewFloat64,"MBJS_NewFloat64"); RO(P_NewNumber,S_NewNumber,"MBJS_NewNumber");
    RO(P_ToInt32,S_ToInt32,"MBJS_ToInt32");
    RO(P_IsException,S_IsException,"MBJS_IsException"); R(P_GetException,S_GetException,"MBJS_GetException");
    R(P_FreeValue,S_FreeValue,"MBJS_FreeValue"); R(P_DupValue,S_DupValue,"MBJS_DupValue");
    R(P_NewArrayBufferCopy,S_NewArrayBufferCopy,"MBJS_NewArrayBufferCopy"); R(P_GetArrayBuffer,S_GetArrayBuffer,"MBJS_GetArrayBuffer");
    R(P_NewUint8ArrayCopy,S_NewUint8ArrayCopy,"MBJS_NewUint8ArrayCopy"); R(P_GetUint8Array,S_GetUint8Array,"MBJS_GetUint8Array");
    R(P_GetGlobalObject,S_GetGlobalObject,"MBJS_GetGlobalObject"); R(P_Call,S_Call,"MBJS_Call");
    RO(P_IsFunction,S_IsFunction,"MBJS_IsFunction"); RO(P_IsObject,S_IsObject,"MBJS_IsObject");
    RO(P_IsUndefined,S_IsUndefined,"MBJS_IsUndefined"); RO(P_IsNumber,S_IsNumber,"MBJS_IsNumber");
#undef R
#undef RO
    if(!S_NewInt32) S_NewInt32 = my_NewInt32;
    if(!S_NewFloat64) S_NewFloat64 = my_NewFloat64;
    if(!S_NewInt64) S_NewInt64 = my_NewInt64;
    if(!S_IsException) { /* fallback: check tag == MBJS_TAG_EXCEPTION (-1) */ }
    return NULL;
}
static int load_wasmy(const char *dll_path) {
    if (hWasmy) return 1;
    hWasmy = LoadLibraryA(dll_path); if (!hWasmy) return 0;
#define L(T,N,S) do{ void *p=sym(hWasmy,S); if(!p){FreeLibrary(hWasmy);hWasmy=NULL;return 0;} N=(T)p; }while(0)
    L(P_wasmy_module_parse,p_module_parse,"wasmy_module_parse"); L(P_wasmy_module_free,p_module_free,"wasmy_module_free");
    L(P_wasmy_runtime_new,p_runtime_new,"wasmy_runtime_new"); L(P_wasmy_runtime_free,p_runtime_free,"wasmy_runtime_free");
    L(P_wasmy_instantiate,p_instantiate,"wasmy_instantiate"); L(P_wasmy_register_host,p_register_host,"wasmy_register_host");
    L(P_wasmy_call,p_call,"wasmy_call"); L(P_wasmy_export_func_index,p_export_func_index,"wasmy_export_func_index");
    L(P_wasmy_memory_ptr,p_memory_ptr,"wasmy_memory_ptr"); L(P_wasmy_memory_len,p_memory_len,"wasmy_memory_len");
#undef L
    return 1;
}
static MBJSValue js_throw(MBJSContext *ctx, const char *msg){ rt.ThrowTypeError(ctx,"mochiwasmy: %s",msg); return MBJS_EXCEPTION; }
static uint64_t get_ptr_arg(MBJSContext *ctx, MBJSValueConst v){ int64_t i64=0; if(rt.ToBigInt64 && rt.ToBigInt64(ctx,&i64,v)==0) return (uint64_t)i64; double d=0; if(rt.ToFloat64(ctx,&d,v)==0) return (uint64_t)(int64_t)d; int32_t i32=0; if(S_ToInt32(ctx,&i32,v)==0) return (uint64_t)(uint32_t)i32; return 0; }
static MBJSValue make_ptr(MBJSContext *ctx, uint64_t p){ if(rt.NewBigInt64) return rt.NewBigInt64(ctx,(int64_t)p); return S_NewFloat64(ctx,(double)(int64_t)p); }

static MBJSValue js_load(MBJSContext *ctx, MBJSValueConst this_val, int argc, MBJSValueConst *argv){
    (void)this_val; if(argc<1) return js_throw(ctx,"load(dllPath) required");
    size_t len=0; const char *p=rt.ToCStringLen(ctx,&len,argv[0]); if(!p) return js_throw(ctx,"bad path");
    char *tmp=(char*)malloc(len+1); memcpy(tmp,p,len); tmp[len]=0; rt.FreeCString(ctx,p);
    int ok=load_wasmy(tmp); free(tmp); if(!ok) return js_throw(ctx,"failed to load wasmyberry.dll");
    return S_NewInt32(ctx,1);
}
static MBJSValue js_parse(MBJSContext *ctx, MBJSValueConst this_val, int argc, MBJSValueConst *argv){
    (void)this_val; if(!hWasmy) return js_throw(ctx,"not loaded"); if(argc<1) return js_throw(ctx,"bytes required");
    size_t sz=0; uint8_t *buf=S_GetUint8Array(ctx,&sz,argv[0]); if(!buf) buf=S_GetArrayBuffer(ctx,&sz,argv[0]);
    if(!buf) return js_throw(ctx,"expected Uint8Array/ArrayBuffer");
    void *m=p_module_parse(buf,sz); if(!m) return js_throw(ctx,"parse failed");
    return make_ptr(ctx,(uint64_t)(uintptr_t)m);
}
static MBJSValue js_module_free(MBJSContext *ctx, MBJSValueConst this_val, int argc, MBJSValueConst *argv){ (void)this_val; if(argc<1||!hWasmy) return MBJS_UNDEFINED; p_module_free((void*)(uintptr_t)get_ptr_arg(ctx,argv[0])); return MBJS_UNDEFINED; }
static MBJSValue js_runtime_new(MBJSContext *ctx, MBJSValueConst this_val, int argc, MBJSValueConst *argv){ (void)this_val; if(argc<1||!hWasmy) return js_throw(ctx,"mod required"); void *r=p_runtime_new((void*)(uintptr_t)get_ptr_arg(ctx,argv[0])); if(!r) return js_throw(ctx,"runtime_new failed"); return make_ptr(ctx,(uint64_t)(uintptr_t)r); }
static MBJSValue js_runtime_free(MBJSContext *ctx, MBJSValueConst this_val, int argc, MBJSValueConst *argv){ (void)this_val; if(argc<1||!hWasmy) return MBJS_UNDEFINED; p_runtime_free((void*)(uintptr_t)get_ptr_arg(ctx,argv[0])); return MBJS_UNDEFINED; }
static MBJSValue js_instantiate(MBJSContext *ctx, MBJSValueConst this_val, int argc, MBJSValueConst *argv){ (void)this_val; if(argc<1||!hWasmy) return js_throw(ctx,"rt required"); return S_NewInt32(ctx, p_instantiate((void*)(uintptr_t)get_ptr_arg(ctx,argv[0]))); }
static MBJSValue js_export_func_index(MBJSContext *ctx, MBJSValueConst this_val, int argc, MBJSValueConst *argv){
    (void)this_val; if(argc<2||!hWasmy) return js_throw(ctx,"mod,name required");
    size_t len=0; const char *n=rt.ToCStringLen(ctx,&len,argv[1]); if(!n) return js_throw(ctx,"bad name");
    char *tmp=(char*)malloc(len+1); memcpy(tmp,n,len); tmp[len]=0; rt.FreeCString(ctx,n);
    int idx=p_export_func_index((void*)(uintptr_t)get_ptr_arg(ctx,argv[0]),tmp); free(tmp); return S_NewInt32(ctx,idx);
}
static MBJSValue js_memory_len(MBJSContext *ctx, MBJSValueConst this_val, int argc, MBJSValueConst *argv){ (void)this_val; if(argc<1||!hWasmy) return S_NewInt32(ctx,0); return S_NewFloat64(ctx,(double)p_memory_len((void*)(uintptr_t)get_ptr_arg(ctx,argv[0]))); }
static MBJSValue js_memory_read(MBJSContext *ctx, MBJSValueConst this_val, int argc, MBJSValueConst *argv){
    (void)this_val; if(argc<3||!hWasmy) return js_throw(ctx,"rt,off,len required");
    void *r=(void*)(uintptr_t)get_ptr_arg(ctx,argv[0]); uint32_t off=0,len=0; S_ToInt32(ctx,(int32_t*)&off,argv[1]); S_ToInt32(ctx,(int32_t*)&len,argv[2]);
    uint8_t *mem=p_memory_ptr(r); size_t mlen=p_memory_len(r); if(!mem || (size_t)off+len>mlen) return js_throw(ctx,"oob");
    return S_NewUint8ArrayCopy(ctx, mem+off, len);
}
static MBJSValue js_memory_write(MBJSContext *ctx, MBJSValueConst this_val, int argc, MBJSValueConst *argv){
    (void)this_val; if(argc<3||!hWasmy) return js_throw(ctx,"rt,off,bytes required");
    void *r=(void*)(uintptr_t)get_ptr_arg(ctx,argv[0]); uint32_t off=0; S_ToInt32(ctx,(int32_t*)&off,argv[1]);
    size_t sz=0; uint8_t *buf=S_GetUint8Array(ctx,&sz,argv[2]); if(!buf) buf=S_GetArrayBuffer(ctx,&sz,argv[2]); if(!buf) return js_throw(ctx,"bytes required");
    uint8_t *mem=p_memory_ptr(r); size_t mlen=p_memory_len(r); if(!mem || (size_t)off+sz>mlen) return js_throw(ctx,"oob");
    memcpy(mem+off,buf,sz); return S_NewInt32(ctx,(int32_t)sz);
}

typedef struct HostCb { MBJSContext *ctx; MBJSValue fn; } HostCb;
static int __cdecl host_trampoline(void *vctx, const void *args, size_t nargs, void *result) {
    HostCb *cb=(HostCb*)vctx; WasmyValue *a=(WasmyValue*)args; WasmyValue *res=(WasmyValue*)result;
    MBJSValue argv[16]; size_t n=nargs>16?16:nargs;
    for(size_t i=0;i<n;i++){
        if(a[i].tag==1) argv[i]=S_NewInt64(cb->ctx,(int64_t)a[i].payload);
        else if(a[i].tag==2){ float fv; uint32_t u=(uint32_t)a[i].payload; memcpy(&fv,&u,4); argv[i]=S_NewFloat64(cb->ctx,(double)fv); }
        else if(a[i].tag==3){ double dv; memcpy(&dv,&a[i].payload,8); argv[i]=S_NewFloat64(cb->ctx,dv); }
        else argv[i]=S_NewInt32(cb->ctx,(int32_t)(uint32_t)a[i].payload);
    }
    MBJSValue global=S_GetGlobalObject(cb->ctx);
    MBJSValue ret=S_Call(cb->ctx, cb->fn, global, (int)n, argv);
    S_FreeValue(cb->ctx, global);
    for(size_t i=0;i<n;i++) S_FreeValue(cb->ctx,argv[i]);
    if((MBJS_VALUE_GET_TAG(ret)==MBJS_TAG_EXCEPTION)){ S_FreeValue(cb->ctx,S_GetException(cb->ctx)); res->tag=0; res->payload=0; return -1; }
    double d=0; if((MBJS_VALUE_GET_TAG(ret)==MBJS_TAG_INT || MBJS_TAG_IS_FLOAT64(MBJS_VALUE_GET_TAG(ret)))) rt.ToFloat64(cb->ctx,&d,ret);
    res->tag=0; res->payload=(uint32_t)(int32_t)d; S_FreeValue(cb->ctx,ret); return 0;
}

static MBJSValue js_register_host(MBJSContext *ctx, MBJSValueConst this_val, int argc, MBJSValueConst *argv){
    (void)this_val; if(argc<4||!hWasmy) return js_throw(ctx,"rt,mod,name,fn required");
    void *r=(void*)(uintptr_t)get_ptr_arg(ctx,argv[0]);
    size_t ml=0,nl=0; const char *m=rt.ToCStringLen(ctx,&ml,argv[1]); const char *n=rt.ToCStringLen(ctx,&nl,argv[2]);
    if(!m||!n){ if(m) rt.FreeCString(ctx,m); if(n) rt.FreeCString(ctx,n); return js_throw(ctx,"bad strings"); }
    char *mm=(char*)malloc(ml+1); memcpy(mm,m,ml); mm[ml]=0; rt.FreeCString(ctx,m);
    char *nn=(char*)malloc(nl+1); memcpy(nn,n,nl); nn[nl]=0; rt.FreeCString(ctx,n);
    if(!S_IsFunction(ctx,argv[3])){ free(mm); free(nn); return js_throw(ctx,"fn must be function"); }
    HostCb *cb=(HostCb*)malloc(sizeof(HostCb)); cb->ctx=ctx; cb->fn=S_DupValue(ctx,argv[3]);
    int rc=p_register_host(r,mm,nn,host_trampoline,cb); free(mm); free(nn); return S_NewInt32(ctx,rc);
}

static MBJSValue js_call(MBJSContext *ctx, MBJSValueConst this_val, int argc, MBJSValueConst *argv){
    (void)this_val; if(argc<2||!hWasmy) return js_throw(ctx,"rt,funcIdx,args required");
    void *r=(void*)(uintptr_t)get_ptr_arg(ctx,argv[0]); int32_t funcIdx=0; S_ToInt32(ctx,&funcIdx,argv[1]);
    WasmyValue argsv[32]; size_t nargs=0;
    if(argc>=3 && (MBJS_VALUE_GET_TAG(argv[2])==MBJS_TAG_OBJECT) && !(MBJS_VALUE_GET_TAG(argv[2])==MBJS_TAG_UNDEFINED)){
        MBJSValue lv=S_GetPropertyStr(ctx,argv[2],"length"); int32_t len=0; if(!(MBJS_VALUE_GET_TAG(lv)==MBJS_TAG_EXCEPTION)){ double d=0; rt.ToFloat64(ctx,&d,lv); len=(int32_t)d; } S_FreeValue(ctx,lv);
        if(len>32) len=32;
        for(int i=0;i<len;i++){ char key[16]; snprintf(key,sizeof(key),"%d",i); MBJSValue v=S_GetPropertyStr(ctx,argv[2],key); double d=0; if(!(MBJS_VALUE_GET_TAG(v)==MBJS_TAG_EXCEPTION)) rt.ToFloat64(ctx,&d,v); else S_FreeValue(ctx,S_GetException(ctx)); argsv[nargs].tag=0; memset(argsv[nargs].pad,0,7); argsv[nargs].payload=(uint32_t)(int32_t)d; nargs++; S_FreeValue(ctx,v); }
    }
    WasmyValue out[8]; size_t outn=0; int rc=p_call(r,(uint32_t)funcIdx,argsv,nargs,out,8,&outn);
    if(rc!=0) return js_throw(ctx,"wasm call failed");
    MBJSValue ret=S_NewObject(ctx); S_SetPropertyStr(ctx,ret,"rc",S_NewInt32(ctx,0)); S_SetPropertyStr(ctx,ret,"count",S_NewInt32(ctx,(int32_t)outn));
    if(outn>0){ if(out[0].tag==1) S_SetPropertyStr(ctx,ret,"value",S_NewInt64(ctx,(int64_t)out[0].payload)); else if(out[0].tag==3){ double dv; memcpy(&dv,&out[0].payload,8); S_SetPropertyStr(ctx,ret,"value",S_NewFloat64(ctx,dv)); } else S_SetPropertyStr(ctx,ret,"value",S_NewInt32(ctx,(int32_t)(uint32_t)out[0].payload)); }
    return ret;
}

static const MBJSCFunctionListEntry wasmy_tab[] = {
    MBJS_CFUNC_DEF("load",1,js_load), MBJS_CFUNC_DEF("parseModule",1,js_parse), MBJS_CFUNC_DEF("moduleFree",1,js_module_free),
    MBJS_CFUNC_DEF("runtimeNew",1,js_runtime_new), MBJS_CFUNC_DEF("runtimeFree",1,js_runtime_free), MBJS_CFUNC_DEF("instantiate",1,js_instantiate),
    MBJS_CFUNC_DEF("registerHost",4,js_register_host), MBJS_CFUNC_DEF("call",3,js_call), MBJS_CFUNC_DEF("exportFuncIndex",2,js_export_func_index),
    MBJS_CFUNC_DEF("memoryLen",1,js_memory_len), MBJS_CFUNC_DEF("memoryRead",3,js_memory_read), MBJS_CFUNC_DEF("memoryWrite",3,js_memory_write),
};
#define WASMY_TAB_N ((int)(sizeof(wasmy_tab) / sizeof(wasmy_tab[0])))

static int module_init(MBJSContext *ctx, MBJSModuleDef *m)
{
    return rt.SetModuleExportList(ctx, m, wasmy_tab, WASMY_TAB_N);
}
MBJS_MODULE_EXTERN MBJSModuleDef *mbjs_init_module(MBJSContext *ctx, const char *name){
    const char *missing=mbjs_runtime_resolve(&rt); if(missing){ rt.ThrowTypeError(ctx,"mochiwasmy: missing %s",missing); return NULL; } { const char *miss=resolve_extra(); if(miss){ FILE*f=fopen("C:/Users/Administrator/AppData/Local/Temp/mochiwasmy_miss.txt","w"); if(f){fputs(miss,f);fclose(f);} rt.ThrowTypeError(ctx,"mochiwasmy: missing %s",miss); return NULL; } }
    MBJSModuleDef *m=rt.NewCModule(ctx, name?name:"mochiwasmy", module_init); if(!m) return NULL;
    if(rt.AddModuleExportList(ctx,m,wasmy_tab,WASMY_TAB_N)!=0) return NULL;
    return m;
}
