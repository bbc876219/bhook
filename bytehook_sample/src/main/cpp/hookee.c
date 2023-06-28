#include <android/log.h>
#include <fcntl.h>
#include <jni.h>
#include <stdlib.h>
#include <unistd.h>
#include "threads.h"

#define HOOKEE_JNI_VERSION    JNI_VERSION_1_6
#define HOOKEE_JNI_CLASS_NAME "com/bbc/NativeHookeeA"
#define HOOKEE_TAG            "bytehook_tag"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#define LOG(fmt, ...) __android_log_print(ANDROID_LOG_INFO, HOOKEE_TAG, fmt, ##__VA_ARGS__)
#pragma clang diagnostic pop

#pragma clang optimize off


 static void thread_1(void ){
     LOG("我是线程一 开始");
    int i = 0;
    while (1) {
        i+=1;
        sleep(1);
        LOG("2B我是线程一 %d次\n",i);
        if (i>5){
            break;
        }
    }
    LOG("我是线程一 结束");



}

 static void thread_2(void){
     LOG("我是线程二 开始");
    int i = 0;
    while (1) {
        i+=1;
        sleep(2);
        LOG("我是线程二 %d次\n",i);
        if (i>5){
            break;
        }
    }
     LOG("我是线程二 结束");
}


 static void  thread_3(void){
     LOG("2B我是线程三 开始");
    int i = 0;
    while (3) {
        i+=1;
        sleep(3);
        LOG("我是线程三 %d次\n",i);
        if (i>5){
            break;
        }
    }
     LOG("2B我是线程三 结束");

}

static void hookee_test(JNIEnv *env, jobject thiz) {
    (void) env, (void) thiz;

    LOG("source libhookee.so test open() start ");
    mode_t f_attrib;
    f_attrib = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH; // 声明mode_t
    int fd = open("/dev/null", O_RDWR);
    if (fd >= 0) close(fd);
    int fd1 = open64("/dev/null", O_RDWR, f_attrib);
    if (fd1 >= 0) close(fd1);

    LOG("source libhookee.so test open() end ");
    //线程开始的地址
    pthread_t star_location_1, star_location_2, star_location_3;
    int ret;
    //创建线程
    ret = pthread_create(&star_location_1, NULL, (void *(*)(void *)) thread_1, NULL);
    if (ret != 0) {
        LOG("star_location_1 pthread_create fail ");
    }

    ret = pthread_create(&star_location_2, NULL, (void *(*)(void *)) thread_2, NULL);
    if (ret != 0) {
        LOG("star_location_2 pthread_create fail ");
    }

    ret = pthread_create(&star_location_3, NULL, (void *(*)(void *)) thread_3, NULL);
    if (ret != 0) {
        LOG("star_location_3 pthread_create fail ");
    }

//设置线程为分离属性
    ret = pthread_detach(star_location_1);
    if (ret != 0) {
        LOG("pthread_detach fail");
    }
    pthread_setname_np(star_location_1,"thread_line_1");
    pthread_setname_np(star_location_2,"thread_line_2");
    pthread_setname_np(star_location_3,"thread_line_3");
    //子线程设置分离属性,则pthread_join不再阻塞,立刻返回
    /*等待线程结束*/
    ret = pthread_join(star_location_1, NULL);
    if (ret != 0) {
        LOG("pthread_join star_location_1 fail");
    }
    ret = pthread_join(star_location_2, NULL);
    if (ret != 0) {
        LOG("pthread_join star_location_2 fail");
    }
    ret = pthread_join(star_location_3, NULL);
    if (ret != 0) {
        LOG("pthread_join star_location_3 fail");
    }
    sleep(3);
    LOG("主线程退出");


}

#pragma clang optimize on

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
  (void)reserved;

  if (NULL == vm) return JNI_ERR;

  JNIEnv *env;
  if (JNI_OK != (*vm)->GetEnv(vm, (void **)&env, HOOKEE_JNI_VERSION)) return JNI_ERR;
  if (NULL == env || NULL == *env) return JNI_ERR;

  jclass cls;
  if (NULL == (cls = (*env)->FindClass(env, HOOKEE_JNI_CLASS_NAME))) return JNI_ERR;

  JNINativeMethod m[] = {{"nativeTest", "()V", (void *)hookee_test}};
  if (0 != (*env)->RegisterNatives(env, cls, m, sizeof(m) / sizeof(m[0]))) return JNI_ERR;

  return HOOKEE_JNI_VERSION;
}
