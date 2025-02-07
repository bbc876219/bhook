#include <android/log.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <inttypes.h>
#include <jni.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "bytehook.h"

#define HACKER_JNI_VERSION    JNI_VERSION_1_6
#define HACKER_JNI_CLASS_NAME "com/bytedance/android/bytehook/sample/NativeHacker"
#define HACKER_TAG            "bytehook_tag"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#define LOG(fmt, ...) __android_log_print(ANDROID_LOG_INFO, HACKER_TAG, fmt, ##__VA_ARGS__)
#pragma clang diagnostic pop

// 原本线程参数
struct ThreadHookeeArgus {
    void *(*current_func)(void *);

    void *current_arg;
};


//static void *pthread(void *arg) {
//  struct ThreadHookeeArgus *temp = (struct ThreadHookeeArgus *) arg;
//  temp->current_func(temp->current_arg);
//  __android_log_print(ANDROID_LOG_WARN, HACKER_TAG, "%s", "crash 了，但被我抓住了");
//  return NULL;
//}
typedef int (*open_t)(const char *, int, mode_t);

typedef int (*open_real_t)(const char *, int, mode_t);

typedef int (*open2_t)(const char *, int);

typedef int (*close_t)(int);

typedef int (*pthread_create_t)(const pthread_t *, const pthread_attr_t *, void *(void *), void *);

typedef int (*pthread_detach_t)(pthread_t);

typedef void (*pthread_exit_t)(void *);

typedef int (*pthread_join_t)(pthread_t);

typedef int (*pthread_setname_np_t)(pthread_t, const char *);


#define OPEN_DEF(fn)                                                                                         \
  static fn##_t fn##_prev = NULL;                                                                            \
  static bytehook_stub_t fn##_stub = NULL;                                                                   \
  static void fn##_hooked_callback(bytehook_stub_t task_stub, int status_code, const char *caller_path_name, \
                                   const char *sym_name, void *new_func, void *prev_func, void *arg) {       \
    if (BYTEHOOK_STATUS_CODE_ORIG_ADDR == status_code) {                                                     \
      fn##_prev = (fn##_t)prev_func;                                                                         \
      if(bytehook_get_debug()) LOG(">>>>> save original address: %" PRIxPTR, (uintptr_t)prev_func);                                   \
    } else {                                                                                                 \
      if(bytehook_get_debug()) LOG(">>>>> hooked. stub: %" PRIxPTR                                                                    \
          ", status: %d, caller_path_name: %s, sym_name: %s, new_func: %" PRIxPTR ", prev_func: %" PRIxPTR   \
          ", arg: %" PRIxPTR,                                                                                \
          (uintptr_t)task_stub, status_code, caller_path_name, sym_name, (uintptr_t)new_func,                \
          (uintptr_t)prev_func, (uintptr_t)arg);                                                             \
    }                                                                                                        \
  }
OPEN_DEF(open)

OPEN_DEF(open_real)

OPEN_DEF(open2)

OPEN_DEF(close)

OPEN_DEF(pthread_join)

OPEN_DEF(pthread_exit)

OPEN_DEF(pthread_detach)

OPEN_DEF(pthread_create)

OPEN_DEF(pthread_setname_np)


static void debug(const char *sym, const char *pathname, int flags, int fd, void *lr) {
    Dl_info info;
    memset(&info, 0, sizeof(info));
    dladdr(lr, &info);

    LOG("proxy %s(\"%s\", %d), return FD: %d, called from: %s (%s)", sym, pathname, flags, fd,
        info.dli_fname,
        info.dli_sname);
}

static void debug_close(const char *sym, int flags, int fd, void *lr) {
    Dl_info info;
    memset(&info, 0, sizeof(info));
    dladdr(lr, &info);

    LOG("proxy %s( %d), return RS: %d, called from: %s (%s)", sym, fd, flags, info.dli_fname,
        info.dli_sname);
}

static void
debug_thread_setname(const char *sym, int flags, pthread_t pt, const char *name, void *lr) {
    Dl_info info;
    memset(&info, 0, sizeof(info));
    dladdr(lr, &info);

    LOG("proxy %s(%s), return RS: %d, pthread_t：%ld called from: %s (%s)", sym, name, flags, pt,
        info.dli_fname,
        info.dli_sname);
}

static void debug_thread(const char *sym, int flags, pthread_t pt, void *lr) {
    Dl_info info;
    memset(&info, 0, sizeof(info));
    dladdr(lr, &info);

    LOG("proxy %s(), return RS: %d, pthread_t：%ld called from: %s (%s)", sym, flags, pt,
        info.dli_fname,
        info.dli_sname);
}

static int open_proxy_auto(const char *pathname, int flags, mode_t modes) {
    // In automatic mode, if you need to call the original function,
    // please always use the BYTEHOOK_CALL_PREV() macro.
    int fd = BYTEHOOK_CALL_PREV(open_proxy_auto, open_t, pathname, flags, modes);
    if (bytehook_get_debug()) {
        debug("open", pathname, flags, fd, BYTEHOOK_RETURN_ADDRESS());
    }

    // Make sure to call BYTEHOOK_POP_STACK() macro before returning.
    // In the CPP source file, you can also call BYTEHOOK_STACK_SCOPE() macro
    // at the beginning of the proxy function instead.
    BYTEHOOK_POP_STACK();
    return fd;
}

static int open_real_proxy_auto(const char *pathname, int flags, mode_t modes) {
    int fd = BYTEHOOK_CALL_PREV(open_real_proxy_auto, open_real_t, pathname, flags, modes);
    if (bytehook_get_debug()) {
        debug("__open_real", pathname, flags, fd, BYTEHOOK_RETURN_ADDRESS());
    }
    BYTEHOOK_POP_STACK();
    return fd;
}

static int open2_proxy_auto(const char *pathname, int flags) {
    int fd = BYTEHOOK_CALL_PREV(open2_proxy_auto, open2_t, pathname, flags);
    if (bytehook_get_debug()) {
        debug("__open_2", pathname, flags, fd, BYTEHOOK_RETURN_ADDRESS());
    }
    BYTEHOOK_POP_STACK();
    return fd;
}

static int close_proxy_auto(int fd) {
    int rs = BYTEHOOK_CALL_PREV(close_proxy_auto, close_t, fd);
    if (bytehook_get_debug()) {
        debug_close("close", rs, fd, BYTEHOOK_RETURN_ADDRESS());
    }
    BYTEHOOK_POP_STACK();
    return rs;
}

static int pthread_setname_np_proxy_auto(pthread_t pt, const char *name) {
    int rs = BYTEHOOK_CALL_PREV(pthread_setname_np_proxy_auto, pthread_setname_np_t, pt, name);
    if (bytehook_get_debug()) {
        debug_thread_setname("pthread_setname_np", rs, pt, name, BYTEHOOK_RETURN_ADDRESS());
    }
    BYTEHOOK_POP_STACK();
    return rs;
}

static int pthread_join_proxy_auto(pthread_t pt) {
    int rs = BYTEHOOK_CALL_PREV(pthread_join_proxy_auto, pthread_join_t, pt);
    if (bytehook_get_debug()) {
        debug_thread("pthread_join", rs, pt, BYTEHOOK_RETURN_ADDRESS());
    }
    BYTEHOOK_POP_STACK();
    return rs;
}

static int pthread_detach_proxy_auto(pthread_t pt) {
    int rs = BYTEHOOK_CALL_PREV(pthread_detach_proxy_auto, pthread_detach_t, pt);
    if (bytehook_get_debug()) {
        debug_thread("pthread_detach", rs, pt, BYTEHOOK_RETURN_ADDRESS());
    }
    BYTEHOOK_POP_STACK();
    return rs;
}

static void pthread_exit_proxy_auto(void *return_value) {
    BYTEHOOK_CALL_PREV(pthread_exit_proxy_auto, pthread_exit_t, return_value);
    if (bytehook_get_debug()) {
        debug_thread("pthread_exit", 0, (pthread_t) NULL, BYTEHOOK_RETURN_ADDRESS());
    }
    BYTEHOOK_POP_STACK();
    return;
}

static int pthread_create_proxy_auto(pthread_t *thread, pthread_attr_t *attr,
                                     void *(*start_routine)(void *), void *arg) {
//  struct ThreadHookeeArgus *params;
//  params = (struct ThreadHookeeArgus *) malloc(sizeof(struct ThreadHookeeArgus));
//  params->current_func = start_routine;
//  params->current_arg = arg;

//  __android_log_print(ANDROID_LOG_INFO, HACKER_TAG, "%s", "call pthread_create");
    int rs = BYTEHOOK_CALL_PREV(pthread_create_proxy_auto, pthread_create_t, thread, attr,
                                start_routine, (void *) arg);
    //int rs = BYTEHOOK_CALL_PREV(pthread_create_proxy_auto,pthread_create_t, thread, attr,pthread,(void *) params);
    if (bytehook_get_debug()) {
        debug_thread("pthread_create", rs, (pthread_t) thread, BYTEHOOK_RETURN_ADDRESS());
    }
    BYTEHOOK_POP_STACK();

    return rs;
}


static int open_proxy_manual(const char *pathname, int flags, mode_t modes) {
    int fd = open_prev(pathname, flags, modes);
    debug("open", pathname, flags, fd, BYTEHOOK_RETURN_ADDRESS());
    return fd;
}

static int open_real_proxy_manual(const char *pathname, int flags, mode_t modes) {
    int fd = open_real_prev(pathname, flags, modes);
    debug("__open_real", pathname, flags, fd, BYTEHOOK_RETURN_ADDRESS());
    return fd;
}

static int open2_proxy_manual(const char *pathname, int flags) {
    int fd = open2_prev(pathname, flags);
    debug("__open_2", pathname, flags, fd, BYTEHOOK_RETURN_ADDRESS());
    return fd;
}

static int close_proxy_manual(int fd) {
    int rs = close_prev(fd);
    debug_close("close", rs, fd, BYTEHOOK_RETURN_ADDRESS());
    return fd;
}

static bool allow_filter(const char *caller_path_name, void *arg) {
    (void) arg;

    if (NULL != strstr(caller_path_name, "libc.so")) return false;
    if (NULL != strstr(caller_path_name, "libbase.so")) return false;
    if (NULL != strstr(caller_path_name, "liblog.so")) return false;
    if (NULL != strstr(caller_path_name, "libunwindstack.so")) return false;
    if (NULL != strstr(caller_path_name, "libutils.so")) return false;
    // ......

    return true;
}

static bool allow_filter_for_hook_all(const char *caller_path_name, void *arg) {
    (void) arg;

    if (NULL != strstr(caller_path_name, "liblog.so")) return false;

    return true;
}

static int hacker_hook(JNIEnv *env, jobject thiz, jint type) {
    (void) env, (void) thiz;

    if (NULL != open_stub || NULL != open_real_stub || NULL != open2_stub) return -1;
    void *open_proxy, *open_real_proxy, *open2_proxy, *close_proxy, *pthread_join_proxy, *pthread_detach_proxy, *pthread_exit_proxy, *pthread_create_proxy, *pthread_setname_np_proxy;
    //void *open_proxy, *open_real_proxy, *open2_proxy,*close_proxy;
    if (BYTEHOOK_MODE_MANUAL == bytehook_get_mode()) {
        open_proxy = (void *) open_proxy_manual;
        open_real_proxy = (void *) open_real_proxy_manual;
        open2_proxy = (void *) open2_proxy_manual;
        close_proxy = (void *) close_proxy_manual;
        pthread_join_proxy = (void *) pthread_join_proxy_auto;
        pthread_detach_proxy = (void *) pthread_detach_proxy_auto;
        pthread_exit_proxy = (void *) pthread_exit_proxy_auto;
        pthread_create_proxy = (void *) pthread_create_proxy_auto;
        pthread_setname_np_proxy = (void *) pthread_setname_np_proxy_auto;
    } else {
        open_proxy = (void *) open_proxy_auto;
        open_real_proxy = (void *) open_real_proxy_auto;
        open2_proxy = (void *) open2_proxy_auto;
        close_proxy = (void *) close_proxy_auto;
        pthread_join_proxy = (void *) pthread_join_proxy_auto;
        pthread_detach_proxy = (void *) pthread_detach_proxy_auto;
        pthread_exit_proxy = (void *) pthread_exit_proxy_auto;
        pthread_create_proxy = (void *) pthread_create_proxy_auto;
        pthread_setname_np_proxy = (void *) pthread_setname_np_proxy_auto;
    }

    if (0 == type) {
        open_stub = bytehook_hook_single("libhookee.so", NULL, "open", open_proxy,
                                         open_hooked_callback, NULL);
        open_real_stub = bytehook_hook_single("libhookee.so", NULL, "__open_real", open_real_proxy,
                                              open_real_hooked_callback, NULL);
        open2_stub =
                bytehook_hook_single("libhookee.so", NULL, "__open_2", open2_proxy,
                                     open2_hooked_callback, NULL);
        close_stub = bytehook_hook_single("libhookee.so", NULL, "close", close_proxy,
                                          close_hooked_callback, NULL);
    } else if (1 == type) {
        open_stub =
                bytehook_hook_partial(allow_filter, NULL, NULL, "open", open_proxy,
                                      open_hooked_callback, NULL);
        open_real_stub = bytehook_hook_partial(allow_filter, NULL, NULL, "__open_real",
                                               open_real_proxy,
                                               open_real_hooked_callback, NULL);
        open2_stub =
                bytehook_hook_partial(allow_filter, NULL, NULL, "__open_2", open2_proxy,
                                      open2_hooked_callback, NULL);
        close_stub =
                bytehook_hook_partial(allow_filter, NULL, NULL, "close", close_proxy,
                                      close_hooked_callback, NULL);
        pthread_join_stub = bytehook_hook_partial(allow_filter, NULL, NULL, "pthread_join",
                                                  pthread_join_proxy, pthread_join_hooked_callback,
                                                  NULL);
        pthread_detach_stub = bytehook_hook_partial(allow_filter, NULL, NULL, "pthread_detach",
                                                    pthread_detach_proxy,
                                                    pthread_detach_hooked_callback, NULL);
        pthread_exit_stub = bytehook_hook_partial(allow_filter, NULL, NULL, "pthread_exit",
                                                  pthread_exit_proxy, pthread_exit_hooked_callback,
                                                  NULL);
        pthread_create_stub = bytehook_hook_partial(allow_filter, NULL, NULL, "pthread_create",
                                                    pthread_create_proxy,
                                                    pthread_create_hooked_callback, NULL);
        pthread_setname_np_stub = bytehook_hook_partial(allow_filter, NULL, NULL,
                                                        "pthread_setname_np",
                                                        pthread_setname_np_proxy,
                                                        pthread_setname_np_hooked_callback, NULL);


    } else if (2 == type) {
        // Here we are not really using bytehook_hook_all().
        //
        // In the sample app, we use logcat to output debugging information, so we need to
        // filter out liblog.so when hook all.
        //
        // Because in some Android versions, liblog.so will call open() when executing
        // __android_log_print(). This is not a problem in itself, but it will indirectly
        // cause the InitLogging() function of libartbase.so to re-enter and cause a deadlock,
        // eventually leading to ANR.
        //
        // In this sample app, don't do this:
        //
        // open_stub = bytehook_hook_all(NULL, "open", open_proxy, open_hooked_callback, NULL);
        // open_real_stub = bytehook_hook_all(NULL, "__open_real", open_real_proxy, open_real_hooked_callback,
        // NULL); open2_stub = bytehook_hook_all(NULL, "__open_2", open2_proxy, open2_hooked_callback, NULL);

        open_stub = bytehook_hook_partial(allow_filter_for_hook_all, NULL, NULL, "open", open_proxy,
                                          open_hooked_callback, NULL);
        open_real_stub = bytehook_hook_partial(allow_filter_for_hook_all, NULL, NULL, "__open_real",
                                               open_real_proxy, open_real_hooked_callback, NULL);
        open2_stub = bytehook_hook_partial(allow_filter_for_hook_all, NULL, NULL, "__open_2",
                                           open2_proxy,
                                           open2_hooked_callback, NULL);
        close_stub = bytehook_hook_partial(allow_filter_for_hook_all, NULL, NULL, "close",
                                           close_proxy, close_hooked_callback, NULL);
        pthread_join_stub = bytehook_hook_partial(allow_filter_for_hook_all, NULL, NULL,
                                                  "pthread_join", pthread_join_proxy,
                                                  pthread_join_hooked_callback, NULL);
        pthread_detach_stub = bytehook_hook_partial(allow_filter_for_hook_all, NULL, NULL,
                                                    "pthread_detach", pthread_detach_proxy,
                                                    pthread_detach_hooked_callback, NULL);
        pthread_exit_stub = bytehook_hook_partial(allow_filter_for_hook_all, NULL, NULL,
                                                  "pthread_exit", pthread_exit_proxy,
                                                  pthread_exit_hooked_callback, NULL);
        pthread_create_stub = bytehook_hook_partial(allow_filter_for_hook_all, NULL, NULL,
                                                    "pthread_create", pthread_create_proxy,
                                                    pthread_create_hooked_callback, NULL);
        pthread_setname_np_stub = bytehook_hook_partial(allow_filter_for_hook_all, NULL, NULL,
                                                        "pthread_setname_np",
                                                        pthread_setname_np_proxy,
                                                        pthread_setname_np_hooked_callback, NULL);
    }

    return 0;
}

static int hacker_unhook(JNIEnv *env, jobject thiz) {
    (void) env, (void) thiz;

    if (NULL != open_stub) {
        bytehook_unhook(open_stub);
        open_stub = NULL;
    }
    if (NULL != open_real_stub) {
        bytehook_unhook(open_real_stub);
        open_real_stub = NULL;
    }
    if (NULL != open2_stub) {
        bytehook_unhook(open2_stub);
        open2_stub = NULL;
    }
    if (NULL != close_stub) {
        bytehook_unhook(close_stub);
        close_stub = NULL;
    }
    if (NULL != pthread_exit_stub) {
        bytehook_unhook(pthread_exit_stub);
        pthread_exit_stub = NULL;
    }
    if (NULL != pthread_join_stub) {
        bytehook_unhook(pthread_join_stub);
        pthread_join_stub = NULL;
    }
    if (NULL != pthread_detach_stub) {
        bytehook_unhook(pthread_detach_stub);
        pthread_detach_stub = NULL;
    }
    if (NULL != pthread_create_stub) {
        bytehook_unhook(pthread_create_stub);
        pthread_create_stub = NULL;
    }
    if (NULL != pthread_setname_np_stub) {
        bytehook_unhook(pthread_setname_np_stub);
        pthread_setname_np_stub = NULL;
    }


    return 0;
}

static void hacker_dump_records(JNIEnv *env, jobject thiz, jstring pathname) {
    (void) thiz;

    const char *c_pathname = (*env)->GetStringUTFChars(env, pathname, 0);
    if (NULL == c_pathname) return;

    int fd = open(c_pathname, O_CREAT | O_WRONLY | O_CLOEXEC | O_TRUNC | O_APPEND,
                  S_IRUSR | S_IWUSR);
    if (fd >= 0) {
        bytehook_dump_records(fd, BYTEHOOK_RECORD_ITEM_ALL);
        close(fd);
    }

    (*env)->ReleaseStringUTFChars(env, pathname, c_pathname);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    (void) reserved;

    if (NULL == vm) return JNI_ERR;

    JNIEnv *env;
    if (JNI_OK != (*vm)->GetEnv(vm, (void **) &env, HACKER_JNI_VERSION)) return JNI_ERR;
    if (NULL == env || NULL == *env) return JNI_ERR;

    jclass cls;
    if (NULL == (cls = (*env)->FindClass(env, HACKER_JNI_CLASS_NAME))) return JNI_ERR;

    JNINativeMethod m[] = {{"nativeHook",        "(I)I",                  (void *) hacker_hook},
                           {"nativeUnhook",      "()I",                   (void *) hacker_unhook},
                           {"nativeDumpRecords", "(Ljava/lang/String;)V", (void *) hacker_dump_records}};
    if (0 != (*env)->RegisterNatives(env, cls, m, sizeof(m) / sizeof(m[0]))) return JNI_ERR;

    return HACKER_JNI_VERSION;
}
