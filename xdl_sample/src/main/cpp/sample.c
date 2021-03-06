#include <fcntl.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <jni.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <android/api-level.h>
#include <android/log.h>
#include "xdl.h"

#define SAMPLE_JNI_VERSION    JNI_VERSION_1_6
#define SAMPLE_JNI_CLASS_NAME "io/hexhacking/xdl/sample/NativeSample"

// log
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#define LOG(fmt, ...) __android_log_print(ANDROID_LOG_DEBUG, "xdl_tag", fmt, ##__VA_ARGS__)
#pragma clang diagnostic pop
#define LOG_END "*** --------------------------------------------------------------"

#if defined(__LP64__)
#define BASENAME_LINKER   "linker64"
#define PATHNAME_LIBC     "/system/lib64/libc.so"
#define PATHNAME_LIBC_Q   "/apex/com.android.runtime/lib64/bionic/libc.so"
#define PATHNAME_LIBCPP   "/system/lib64/libc++.so"
#else
#define BASENAME_LINKER   "linker"
#define PATHNAME_LIBC     "/system/lib/libc.so"
#define PATHNAME_LIBC_Q   "/apex/com.android.runtime/lib/bionic/libc.so"
#define PATHNAME_LIBCPP   "/system/lib/libc++.so"
#endif

#define PATHNAME_LIBC_FIXED (android_get_device_api_level() < __ANDROID_API_Q__ ? PATHNAME_LIBC : PATHNAME_LIBC_Q)

static int callback(struct dl_phdr_info *info, size_t size, void *arg)
{
    (void)size, (void)arg;

    LOG(">>> %"PRIxPTR" %s", (uintptr_t)info->dlpi_addr, info->dlpi_name);

    return 0;
}

static void sample_test_iterate(void)
{
    LOG("+++ xdl_iterate_phdr(XDL_DEFAULT)");
    xdl_iterate_phdr(callback, NULL, XDL_DEFAULT);
    LOG(LOG_END);
    usleep(100 * 1000);

    LOG("+++ xdl_iterate_phdr(XDL_WITH_LINKER)");
    xdl_iterate_phdr(callback, NULL, XDL_WITH_LINKER);
    LOG(LOG_END);
    usleep(100 * 1000);

    LOG("+++ xdl_iterate_phdr(XDL_FULL_PATHNAME)");
    xdl_iterate_phdr(callback, NULL, XDL_FULL_PATHNAME);
    LOG(LOG_END);
    usleep(100 * 1000);

    LOG("+++ xdl_iterate_phdr(XDL_WITH_LINKER | XDL_FULL_PATHNAME)");
    xdl_iterate_phdr(callback, NULL, XDL_WITH_LINKER | XDL_FULL_PATHNAME);
    LOG(LOG_END);
    usleep(100 * 1000);
}

static void sample_test_dlsym(const char *filename, const char *symbol, bool debug_symbol)
{
    LOG("+++ xdl_open + %s + xdl_addr", debug_symbol ? "xdl_dsym" : "xdl_sym");

    // xdl_open
    void *symbol_addr = NULL;
    void *handle = xdl_open(filename);
    LOG(">>> xdl_open(%s) : handle %"PRIxPTR, filename, (uintptr_t)handle);

    // xdl_dsym / xdl_sym
    if(NULL != handle)
    {
        symbol_addr = (debug_symbol ? xdl_dsym : xdl_sym)(handle, symbol);
        xdl_close(handle);
    }
    LOG(">>> %s(%s) : addr %"PRIxPTR, debug_symbol ? "xdl_dsym" : "xdl_sym", symbol, (uintptr_t)symbol_addr);

    // xdl_addr
    Dl_info info;
    char buf[512];
    if(0 == xdl_addr(symbol_addr, &info, buf, sizeof(buf)))
        LOG(">>> xdl_addr(%"PRIxPTR") : FAILED", (uintptr_t)symbol_addr);
    else
        LOG(">>> xdl_addr(%"PRIxPTR") : %"PRIxPTR" %s, %"PRIxPTR" %s", (uintptr_t)symbol_addr,
            (uintptr_t)info.dli_fbase, (NULL == info.dli_fname ? "(NULL)" : info.dli_fname),
            (uintptr_t)info.dli_saddr, (NULL == info.dli_sname ? "(NULL)" : info.dli_sname));

    LOG(LOG_END);
}

static void sample_test(JNIEnv *env, jobject thiz)
{
    (void)env;
    (void)thiz;

    // iterate
    sample_test_iterate();

    // linker
    sample_test_dlsym(BASENAME_LINKER, "__dl__ZL10g_dl_mutex", true);

    // libc.so
    sample_test_dlsym(PATHNAME_LIBC_FIXED, "android_set_abort_message", false);
    sample_test_dlsym(PATHNAME_LIBC_FIXED, "je_mallctl", true);

    // libc++.so
    sample_test_dlsym(PATHNAME_LIBCPP, "_ZNSt3__14cerrE", false);
    sample_test_dlsym(PATHNAME_LIBCPP, "abort_message", true);
}

static JNINativeMethod sample_jni_methods[] = {
    {
        "nativeTest",
        "()V",
        (void *)sample_test
    }
};

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
    JNIEnv *env;
    jclass cls;

    (void)reserved;

    if(NULL == vm) return JNI_ERR;
    if(JNI_OK != (*vm)->GetEnv(vm, (void **)&env, SAMPLE_JNI_VERSION)) return JNI_ERR;
    if(NULL == env || NULL == *env) return JNI_ERR;
    if(NULL == (cls = (*env)->FindClass(env, SAMPLE_JNI_CLASS_NAME))) return JNI_ERR;
    if(0 != (*env)->RegisterNatives(env, cls, sample_jni_methods, sizeof(sample_jni_methods) / sizeof(sample_jni_methods[0]))) return JNI_ERR;

    return SAMPLE_JNI_VERSION;
}
