/*
 * File:        sidl_Java.c
 * Copyright:   (c) 2001 The Regents of the University of California
 * Release:     $Name$
 * Revision:    @(#) $Revision$
 * Date:        $Date$
 * Description: run-time support for Java integration with the JVM
 * Copyright (c) 2000-2001, The Regents of the University of Calfornia.
 * Produced at the Lawrence Livermore National Laboratory.
 * Written by the Components Team <components@llnl.gov>
 * UCRL-CODE-2002-054
 * All rights reserved.
 * 
 * This file is part of Babel. For more information, see
 * http://www.llnl.gov/CASC/components/. Please read the COPYRIGHT file
 * for Our Notice and the LICENSE file for the GNU Lesser General Public
 * License.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License (as published by
 * the Free Software Foundation) version 2.1 dated February 1999.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and
 * conditions of the GNU Lesser General Public License for more details.
 * 
 * You should have recieved a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "sidl_Java.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include "babel_config.h"
#include "sidlType.h"
#include "sidl_BaseClass.h"
#include "sidl_BaseException.h"
#include "sidl_BaseInterface.h"
#include "sidl_DLL.h"
#include "sidl_Loader.h"
#include "sidl_String.h"

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef NULL
#define NULL 0
#endif

/*
 * Convert between jlongs and void* pointers.
 */
#if (SIZEOF_VOID_P == 8)
#define JLONG_TO_POINTER(x) ((void*)(x))
#define POINTER_TO_JLONG(x) ((jlong)(x))
#else
#define JLONG_TO_POINTER(x) ((void*)(int32_t)(x))
#define POINTER_TO_JLONG(x) ((jlong)(int32_t)(x))
#endif

/*
 * This static variable is a reference to the Java JVM.  It is either
 * taken from a currently running JVM or from creating a new JVM.
 */
static JavaVM* s_jvm = NULL;

/*
 * Static method to create a JVM if one has not already been created.  This
 * method takes the CLASSPATH from the environment.
 */
static void sidl_Java_getJVM(void)
{
  typedef jint (*jvmf_t)(JavaVM**,JNIEnv**,JavaVMInitArgs*);

  if (s_jvm == NULL) {
    JavaVMInitArgs vm_args;
    JavaVMOption*  options = NULL;
    char *         babel_jvm_flags = NULL;
    char *         index_ptr = NULL; 
    int            num_flags, count = 0;

    JNIEnv* env     = NULL;
    jvmf_t  jvmf    = NULL;
    char*   clspath = NULL;
    sidl_DLL dll;
    babel_jvm_flags = getenv("BABEL_JVM_FLAGS");
    /*This section of code takes any flags passed in through the enviornment
      variable BABEL_JVM_FLAGS and passed them to the JVM*/

    if((babel_jvm_flags != NULL) && (strlen(babel_jvm_flags) > 1)) {
      index_ptr = babel_jvm_flags;
      num_flags = 3;  /*Default 2 flags below, plus at least 1*/
      while(*index_ptr != '\0') {  /*count up flags*/
	if(*index_ptr == ';')
	  ++num_flags;
	++index_ptr;
      }
      options = (JavaVMOption*)calloc(num_flags, sizeof(JavaVMOption));
      for(count = 2; count < num_flags; ++count) {
	if(count == 2)
	  options[count].optionString = (char*)strtok(babel_jvm_flags, ";");
	else 
	  options[count].optionString = (char*)strtok(NULL, ";");
      }
    } else {
      num_flags = 2;
      options = (JavaVMOption*)calloc(num_flags, sizeof(JavaVMOption));
    }  

    clspath = sidl_String_concat2("-Djava.class.path=", getenv("CLASSPATH"));

    options[0].optionString = "-Djava.compiler=NONE";
    options[1].optionString = clspath;


    vm_args.version            = 0x00010002;
    vm_args.options            = options;
    vm_args.nOptions           = num_flags;
    vm_args.ignoreUnrecognized = 1;

    dll = sidl_DLL__create();
    if (dll) {
      if (sidl_DLL_loadLibrary(dll, "main:", TRUE, TRUE)) {
        jvmf = (jvmf_t) sidl_DLL_lookupSymbol(dll, "JNI_CreateJavaVM");
      }
      sidl_DLL_deleteRef(dll);
    }
    if (!jvmf) { /* not in main: */
#ifdef JVM_SHARED_LIBRARY
      char *url = sidl_String_concat2("file:", JVM_SHARED_LIBRARY);
      if (url) {
        dll = sidl_Loader_loadLibrary(url, TRUE, TRUE);
        if (dll) {
          jvmf = (jvmf_t) sidl_DLL_lookupSymbol(dll, "JNI_CreateJavaVM");
          sidl_DLL_deleteRef(dll);
        }
        sidl_String_free(url);
      }
#else
      fprintf(stderr, "Babel: Unable to initialized Java Virtual Machine\n\
Babel: JVM shared library not found during Babel configuration.\n");
#endif
    }
    if (jvmf != NULL) {
      if (((*jvmf)(&s_jvm, &env, &vm_args)) < 0) {
        s_jvm = NULL;
      }
    }
    sidl_String_free(clspath);
    free(options);  /*calloced above*/
  }

}

/*
 * Attach the current thread to the running JVM and return the Java
 * environment description.  If there is not a currently running JVM,
 * then one is created.
 */
JNIEnv* sidl_Java_getEnv(void)
{
  JNIEnv* env = NULL;
  if (s_jvm == NULL) {
    (void) sidl_Java_getJVM();
  }
  if (s_jvm != NULL) {
    (*s_jvm)->AttachCurrentThread(s_jvm, (void**)&env, NULL);
  }
  return env;
}

/*
 * JNI method called by Java to register sidl JNI native implementations.
 */
void Java_gov_llnl_sidl_BaseClass__1registerNatives(
  JNIEnv* env,
  jclass  cls,
  jstring name)
{
  const char* s = NULL;

  /*
   * Get a handle to the Java virtual machine if we have
   * not already done so.
   */
  if (s_jvm == NULL) {
    (*env)->GetJavaVM(env, &s_jvm);
  }

  /*
   * Extract the sidl name and convert it to linker registration
   * symbol.  Add a "__register" suffix and convert "." scope
   * separators to underscores.
   */
  
  s = (*env)->GetStringUTFChars(env, name, NULL);
  if (s) {
    sidl_DLL dll = NULL;
    void* address = NULL;
    char* symbol  = sidl_String_concat2(s, "__register");

    sidl_String_replace(symbol, '.', '_');

    /* search the global namespace first */
    dll = sidl_DLL__create();
    if (dll) {
      if (sidl_DLL_loadLibrary(dll, "main:", TRUE, FALSE)) {
        address = sidl_DLL_lookupSymbol(dll, symbol);
      }
      sidl_DLL_deleteRef(dll);
    }

    if (!address) {
      /*
       * If we find the registration function in the DLL path, then register
       * the Java types.  Otherwise, return with a unsatisfied link error.
       */
      dll = sidl_Loader_findLibrary(s, "java",
                                    sidl_Scope_SCLSCOPE,
                                    sidl_Resolve_SCLRESOLVE);
      if (dll) {
        address = sidl_DLL_lookupSymbol(dll, symbol);
        sidl_DLL_deleteRef(dll);
      }
    }
    if (address) {
      ((void(*)(JNIEnv*)) address)(env);
    } 
    else {
      jclass e = (*env)->FindClass(env, "java/lang/UnsatisfiedLinkError");
      if (e != NULL) {
        char* msg = sidl_String_concat3(
                                        "Could not find native class \"", s, "\"; check SIDL_DLL_PATH");
        (*env)->ThrowNew(env, e, msg);
        sidl_String_free(msg);
        (*env)->DeleteLocalRef(env, e);
      }
    }
    sidl_String_free(symbol);
    (*env)->ReleaseStringUTFChars(env, name, s);
  }
}

/*
 * JNI method called by Java base class to cast this sidl IOR object.
 */
jlong Java_gov_llnl_sidl_BaseClass__1cast_1ior(
  JNIEnv* env,
  jobject obj,
  jstring name)
{
  jlong ior = 0;

  if (name != (jstring) NULL) {
    jclass    cls = (*env)->GetObjectClass(env, obj);
    jmethodID mid = (*env)->GetMethodID(env, cls, "_get_ior", "()J");
    void*     ptr = JLONG_TO_POINTER((*env)->CallLongMethod(env, obj, mid));

    (*env)->DeleteLocalRef(env, cls);

    if (ptr != NULL) {
      const char* utf = (*env)->GetStringUTFChars(env, name, NULL);
      ior = POINTER_TO_JLONG(sidl_BaseInterface__cast2(ptr, utf));
      (*env)->ReleaseStringUTFChars(env, name, utf);
    }
  }

  return ior;
}

/*
 * JNI method called by Java base class to dereference sidl IOR objects.
 */
void Java_gov_llnl_sidl_BaseClass__1finalize(
  JNIEnv* env,
  jobject obj)
{
  void* ptr = NULL;
   
  /*
   * Initialize the IOR data member reference to avoid repeated lookups.
   */
  static jfieldID s_ior_field = NULL;

  if (s_ior_field == NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    s_ior_field = (*env)->GetFieldID(env, cls, "d_ior", "J");
    (*env)->DeleteLocalRef(env, cls);
  }

  /*
   * Extract the IOR reference from the object and decrement the reference
   * count.
   */
  ptr = JLONG_TO_POINTER((*env)->GetLongField(env, obj, s_ior_field));

  if (ptr != NULL) {
    struct sidl_BaseInterface__object* bi = (struct sidl_BaseInterface__object*) ptr;
    (bi->d_epv->f_deleteRef)(bi->d_object);
   
  }
  (*env)->SetLongField(env, obj, s_ior_field, (jlong) NULL);
}


/*
 * Return TRUE if the specified object is a sidl class; otherwise, return
 * FALSE.
 */
sidl_bool sidl_Java_isClass(
  JNIEnv* env,
  struct sidl_BaseInterface__object* ptr, 
  const char* type)
{
  sidl_bool is = FALSE;
  jstring holdee = sidl_Java_I2J_string(env, type);
  jobject obj = sidl_Java_I2J_cls(env, ptr, type);
  if(obj) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    jmethodID mid = (*env)->GetMethodID(env, cls, "isType", 
					"(Ljava/lang/String;)Z");
    is = (*env)->CallBooleanMethod(env, obj, mid, holdee) ? TRUE : FALSE;
    (*env)->DeleteLocalRef(env, cls);
  }
  (*env)->DeleteLocalRef(env, holdee);
  return is;
}

/*
 * Throw a Java exception if the exception argument is not null.  If the
 * appropriate Java class does not exist, then a class not found exception
 * is thrown.  The variable-argument parameter gives the possible Java type
 * strings.  It must be terminated by a NULL.
 */
void sidl_Java_CheckException(
  JNIEnv* env,
  struct sidl_BaseException__object* ex,
  ...)
{
  va_list args;
  const char* type = NULL;

  if (ex != NULL) {

    /*
     * Search the varargs list of possible exception types.  Throw a particular
     * exception type if a match is found.
     */

    va_start(args, ex);
    while ((type = va_arg(args, const char*)) != NULL) {
      void* ptr = sidl_BaseException__cast2(ex, type);
      if (ptr != NULL) {
        jthrowable obj;
        if (sidl_Java_isClass(env, ptr, type)) {
	  obj = (jthrowable) sidl_Java_I2J_cls(env, ptr, type);
	} else {
	  obj = (jthrowable) sidl_Java_I2J_ifc(env, ptr, type);
	}
	if (obj != NULL) {
	  if((*env)->Throw(env, obj) != 0)
	    fprintf(stderr, "Babel: Unable rethrow the exception recieved.\n");
	}
	break;
      }
    }
    va_end(args);

    /*
     * If we were not able to match the exception type, then throw an
     * internal error to the Java JVM.
     */

    if (type == NULL) {
      jclass e = (*env)->FindClass(env, "java/lang/InternalError");
      if (e != NULL) {
        (*env)->ThrowNew(env, e, "Unknown exception thrown by library routine");
        (*env)->DeleteLocalRef(env, e);
      }
    }
  }
}

/*
 * This test determines if a throwable object from Java is a SIDL object or not..
 */
sidl_bool sidl_Java_isSIDLException(
  JNIEnv* env,
  jobject obj)
{
  if(obj != NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    if(cls != NULL) {
      jmethodID mid = (*env)->GetMethodID(env, cls, "_get_ior", "()J");
      if(mid != NULL)
	return TRUE;
    }
  }
  return FALSE;
}

/*
 *  This function takes a SIDL exception from java as jthrowable obj, and checks if it is an 
 *  expected  exception from this function.  If it is it returns the IOR pointer, if not
 *  it returns NULL.
 */

struct sidl_BaseInterface__object* sidl_Java_catch_SIDLException(
  JNIEnv* env,
  jthrowable obj,
  ...)
{
  va_list args;
  const char* type = NULL;

  if (obj != NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    if(cls != NULL) {
      jmethodID mid = (*env)->GetMethodID(env, cls, "_get_ior", "()J");
      void*     ex = JLONG_TO_POINTER((*env)->CallLongMethod(env, obj, mid));
    /*
     * Search the varargs list of possible exception types.  Throw a particular
     * exception type if a match is found.
     */

      va_start(args, obj);
      while ((type = va_arg(args, const char*)) != NULL) {
	void* ptr = sidl_BaseException__cast2(ex, type);
	if (ptr != NULL) {
	  return ex;
	}
      }
      va_end(args);
      
    }
  }
  return NULL;
}


/*
 * Extract the boolean type from the sidl.Boolean.Holder holder class.
 */
sidl_bool sidl_Java_J2I_boolean_holder(
  JNIEnv* env,
  jobject obj)
{
  static jmethodID mid = (jmethodID) NULL;
  if (mid == (jmethodID) NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    mid = (*env)->GetMethodID(env, cls, "get", "()Z");
    (*env)->DeleteLocalRef(env, cls);
  }
  return (*env)->CallBooleanMethod(env, obj, mid) ? TRUE : FALSE;
}

/*
 * Set the boolean type in the sidl.Boolean.Holder holder class.
 */
void sidl_Java_I2J_boolean_holder(
  JNIEnv* env,
  jobject obj,
  sidl_bool value)
{
  static jmethodID mid = (jmethodID) NULL;
  if (mid == (jmethodID) NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    mid = (*env)->GetMethodID(env, cls, "set", "(Z)V");
    (*env)->DeleteLocalRef(env, cls);
  }
  (*env)->CallVoidMethod(env, obj, mid, (value ? JNI_TRUE : JNI_FALSE));
}

/*
 * Extract the character type from the sidl.Character.Holder holder class.
 */
char sidl_Java_J2I_character_holder(
  JNIEnv* env,
  jobject obj)
{
  static jmethodID mid = (jmethodID) NULL;
  if (mid == (jmethodID) NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    mid = (*env)->GetMethodID(env, cls, "get", "()C");
    (*env)->DeleteLocalRef(env, cls);
  }
  return (char) (*env)->CallCharMethod(env, obj, mid);
}

/*
 * Set the character type in the sidl.Character.Holder holder class.
 */
void sidl_Java_I2J_character_holder(
  JNIEnv* env,
  jobject obj,
  char value)
{
  static jmethodID mid = (jmethodID) NULL;
  if (mid == (jmethodID) NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    mid = (*env)->GetMethodID(env, cls, "set", "(C)V");
    (*env)->DeleteLocalRef(env, cls);
  }
  (*env)->CallVoidMethod(env, obj, mid, (jchar) value);
}

/*
 * Extract the double type from the sidl.Double.Holder holder class.
 */
double sidl_Java_J2I_double_holder(
  JNIEnv* env,
  jobject obj)
{
  static jmethodID mid = (jmethodID) NULL;
  if (mid == (jmethodID) NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    mid = (*env)->GetMethodID(env, cls, "get", "()D");
    (*env)->DeleteLocalRef(env, cls);
  }
  return (*env)->CallDoubleMethod(env, obj, mid);
}

/*
 * Set the double type in the sidl.Double.Holder holder class.
 */
void sidl_Java_I2J_double_holder(
  JNIEnv* env,
  jobject obj,
  double value)
{
  static jmethodID mid = (jmethodID) NULL;
  if (mid == (jmethodID) NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    mid = (*env)->GetMethodID(env, cls, "set", "(D)V");
    (*env)->DeleteLocalRef(env, cls);
  }
  (*env)->CallVoidMethod(env, obj, mid, (jdouble) value);
}

/*
 * Extract the float type from the sidl.Float.Holder holder class.
 */
float sidl_Java_J2I_float_holder(
  JNIEnv* env,
  jobject obj)
{
  static jmethodID mid = (jmethodID) NULL;
  if (mid == (jmethodID) NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    mid = (*env)->GetMethodID(env, cls, "get", "()F");
    (*env)->DeleteLocalRef(env, cls);
  }
  return (*env)->CallFloatMethod(env, obj, mid);
}

/*
 * Set the float type in the sidl.Float.Holder holder class.
 */
void sidl_Java_I2J_float_holder(
  JNIEnv* env,
  jobject obj,
  float value)
{
  static jmethodID mid = (jmethodID) NULL;
  if (mid == (jmethodID) NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    mid = (*env)->GetMethodID(env, cls, "set", "(F)V");
    (*env)->DeleteLocalRef(env, cls);
  }
  (*env)->CallVoidMethod(env, obj, mid, (jfloat) value);
}

/*
 * Extract the int type from the sidl.Integer.Holder holder class.
 */
int sidl_Java_J2I_int_holder(
  JNIEnv* env,
  jobject obj)
{
  static jmethodID mid = (jmethodID) NULL;
  if (mid == (jmethodID) NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    mid = (*env)->GetMethodID(env, cls, "get", "()I");
    (*env)->DeleteLocalRef(env, cls);
  }
  return (*env)->CallIntMethod(env, obj, mid);
}

/*
 * Set the int type in the sidl.Integer.Holder holder class.
 */
void sidl_Java_I2J_int_holder(
  JNIEnv* env,
  jobject obj,
  int value)
{
  static jmethodID mid = (jmethodID) NULL;
  if (mid == (jmethodID) NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    mid = (*env)->GetMethodID(env, cls, "set", "(I)V");
    (*env)->DeleteLocalRef(env, cls);
  }
  (*env)->CallVoidMethod(env, obj, mid, (jint) value);
}

/*
 * Extract the long type from the sidl.Long.Holder holder class.
 */
int64_t sidl_Java_J2I_long_holder(
  JNIEnv* env,
  jobject obj)
{
  static jmethodID mid = (jmethodID) NULL;
  if (mid == (jmethodID) NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    mid = (*env)->GetMethodID(env, cls, "get", "()J");
    (*env)->DeleteLocalRef(env, cls);
  }
  return (int64_t) (*env)->CallLongMethod(env, obj, mid);
}

/*
 * Set the long type in the sidl.Long.Holder holder class.
 */
void sidl_Java_I2J_long_holder(
  JNIEnv* env,
  jobject obj,
  int64_t value)
{
  static jmethodID mid = (jmethodID) NULL;
  if (mid == (jmethodID) NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    mid = (*env)->GetMethodID(env, cls, "set", "(J)V");
    (*env)->DeleteLocalRef(env, cls);
  }
  (*env)->CallVoidMethod(env, obj, mid, (jlong) value);
}

/*
 * Extract the opaque type from the sidl.Opaque.Holder holder class.
 */
void* sidl_Java_J2I_opaque_holder(
  JNIEnv* env,
  jobject obj)
{
  static jmethodID mid = (jmethodID) NULL;
  if (mid == (jmethodID) NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    mid = (*env)->GetMethodID(env, cls, "get", "()J");
    (*env)->DeleteLocalRef(env, cls);
  }
  return JLONG_TO_POINTER((*env)->CallLongMethod(env, obj, mid));
}

/*
 * Set the opaque type in the sidl.Opaque.Holder holder class.
 */
void sidl_Java_I2J_opaque_holder(
  JNIEnv* env,
  jobject obj,
  void* value)
{
  static jmethodID mid = (jmethodID) NULL;
  if (mid == (jmethodID) NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    mid = (*env)->GetMethodID(env, cls, "set", "(J)V");
    (*env)->DeleteLocalRef(env, cls);
  }
  (*env)->CallVoidMethod(env, obj, mid, POINTER_TO_JLONG(value));
}

/*
 * Extract the dcomplex type from the sidl.DoubleComplex.Holder holder class.
 */
struct sidl_dcomplex sidl_Java_J2I_dcomplex_holder(
  JNIEnv* env,
  jobject obj)
{
  static jmethodID mid_get  = (jmethodID) NULL;
  struct sidl_dcomplex dcomplex = { 0.0, 0.0 };
  jobject holdee = (jobject) NULL;

  if (mid_get == (jmethodID) NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    mid_get  = (*env)->GetMethodID(env, cls, "get", "()Lsidl/DoubleComplex;");
    (*env)->DeleteLocalRef(env, cls);
  }

  holdee   = (*env)->CallObjectMethod(env, obj, mid_get);
  dcomplex = sidl_Java_J2I_dcomplex(env, holdee);
  (*env)->DeleteLocalRef(env, holdee);

  return dcomplex;
}

/*
 * Set the dcomplex type in the sidl.DoubleComplex.Holder holder class.
 */
void sidl_Java_I2J_dcomplex_holder(
  JNIEnv* env,
  jobject obj,
  struct sidl_dcomplex* value)
{
  static jmethodID mid_geth = (jmethodID) NULL;
  static jmethodID mid_setc = (jmethodID) NULL;
  static jmethodID mid_seth = (jmethodID) NULL;

  jobject holdee = (jobject) NULL;

  if (mid_geth == (jmethodID) NULL) {
    jclass cls1 = (*env)->GetObjectClass(env, obj);
    jclass cls2 = (*env)->FindClass(env, "sidl/DoubleComplex");
    mid_geth = (*env)->GetMethodID(env, cls1, "get", "()Lsidl/DoubleComplex;");
    mid_setc = (*env)->GetMethodID(env, cls2, "set", "(DD)V");
    mid_seth = (*env)->GetMethodID(env, cls1, "set", "(Lsidl/DoubleComplex;)V");
    (*env)->DeleteLocalRef(env, cls1);
    (*env)->DeleteLocalRef(env, cls2);
  }

  holdee = (*env)->CallObjectMethod(env, obj, mid_geth);
  if (holdee == NULL) {
    holdee = sidl_Java_I2J_dcomplex(env, value);
    (*env)->CallVoidMethod(env, obj, mid_seth, holdee);
  } else {
    (*env)->CallVoidMethod(env,
                           holdee,
                           mid_setc,
                           value->real,
                           value->imaginary);
  }
  (*env)->DeleteLocalRef(env, holdee);
}

/*
 * Extract the fcomplex type from the sidl.FloatComplex.Holder holder class.
 */
struct sidl_fcomplex sidl_Java_J2I_fcomplex_holder(
  JNIEnv* env,
  jobject obj)
{
  static jmethodID mid_get  = (jmethodID) NULL;
  struct sidl_fcomplex fcomplex = { 0.0, 0.0 };
  jobject holdee = (jobject) NULL;

  if (mid_get == (jmethodID) NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    mid_get  = (*env)->GetMethodID(env, cls, "get", "()Lsidl/FloatComplex;");
    (*env)->DeleteLocalRef(env, cls);
  }

  holdee   = (*env)->CallObjectMethod(env, obj, mid_get);
  fcomplex = sidl_Java_J2I_fcomplex(env, holdee);
  (*env)->DeleteLocalRef(env, holdee);

  return fcomplex;
}

/*
 * Set the fcomplex type in the sidl.FloatComplex.Holder holder class.
 */
void sidl_Java_I2J_fcomplex_holder(
  JNIEnv* env,
  jobject obj,
  struct sidl_fcomplex* value)
{
  static jmethodID mid_geth = (jmethodID) NULL;
  static jmethodID mid_setc = (jmethodID) NULL;
  static jmethodID mid_seth = (jmethodID) NULL;

  jobject holdee = (jobject) NULL;

  if (mid_geth == (jmethodID) NULL) {
    jclass cls1 = (*env)->GetObjectClass(env, obj);
    jclass cls2 = (*env)->FindClass(env, "sidl/FloatComplex");
    mid_geth = (*env)->GetMethodID(env, cls1, "get", "()Lsidl/FloatComplex;");
    mid_setc = (*env)->GetMethodID(env, cls2, "set", "(FF)V");
    mid_seth = (*env)->GetMethodID(env, cls1, "set", "(Lsidl/FloatComplex;)V");
    (*env)->DeleteLocalRef(env, cls1);
    (*env)->DeleteLocalRef(env, cls2);
  }

  holdee = (*env)->CallObjectMethod(env, obj, mid_geth);
  if (holdee == NULL) {
    holdee = sidl_Java_I2J_fcomplex(env, value);
    (*env)->CallVoidMethod(env, obj, mid_seth, holdee);
  } else {
    (*env)->CallVoidMethod(env,
                           holdee,
                           mid_setc,
                           value->real,
                           value->imaginary);
  }
  (*env)->DeleteLocalRef(env, holdee);
}

/*
 * Extract the double complex type from a sidl.DoubleComplex object.
 */
struct sidl_dcomplex sidl_Java_J2I_dcomplex(
  JNIEnv* env,
  jobject obj)
{
  static jmethodID mid_real = (jmethodID) NULL;
  static jmethodID mid_imag = (jmethodID) NULL;

  struct sidl_dcomplex dcomplex = { 0.0, 0.0 };

  if ((mid_real == (jmethodID) NULL) && (obj != (jobject) NULL)) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    mid_real = (*env)->GetMethodID(env, cls, "real", "()D");
    mid_imag = (*env)->GetMethodID(env, cls, "imag", "()D");
    (*env)->DeleteLocalRef(env, cls);
  }

  if (obj != (jobject) NULL) {
    dcomplex.real      = (*env)->CallDoubleMethod(env, obj, mid_real);
    dcomplex.imaginary = (*env)->CallDoubleMethod(env, obj, mid_imag);
  }

  return dcomplex;
}

/*
 * Create and return a sidl.DoubleComplex object from a sidl double
 * complex value.
 */
jobject sidl_Java_I2J_dcomplex(
  JNIEnv* env,
  struct sidl_dcomplex* value)
{
  jclass cls = (*env)->FindClass(env, "sidl/DoubleComplex");
  jmethodID mid_ctor = (*env)->GetMethodID(env, cls, "<init>", "(DD)V");
  jobject obj = (*env)->NewObject(env,
                                  cls,
                                  mid_ctor,
                                  value->real,
                                  value->imaginary);
  (*env)->DeleteLocalRef(env, cls);
  return obj;
}

/*
 * Extract the float complex type from a sidl.FloatComplex object.
 */
struct sidl_fcomplex sidl_Java_J2I_fcomplex(
  JNIEnv* env,
  jobject obj)
{
  static jmethodID mid_real = (jmethodID) NULL;
  static jmethodID mid_imag = (jmethodID) NULL;

  struct sidl_fcomplex fcomplex = { 0.0, 0.0 };

  if ((mid_real == (jmethodID) NULL) && (obj != (jobject) NULL)) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    mid_real = (*env)->GetMethodID(env, cls, "real", "()F");
    mid_imag = (*env)->GetMethodID(env, cls, "imag", "()F");
    (*env)->DeleteLocalRef(env, cls);
  }

  if (obj != (jobject) NULL) {
    fcomplex.real      = (*env)->CallFloatMethod(env, obj, mid_real);
    fcomplex.imaginary = (*env)->CallFloatMethod(env, obj, mid_imag);
  }

  return fcomplex;
}

/*
 * Create and return a sidl.FloatComplex object from a sidl float
 * complex value.
 */
jobject sidl_Java_I2J_fcomplex(
  JNIEnv* env,
  struct sidl_fcomplex* value)
{
  jclass cls = (*env)->FindClass(env, "sidl/FloatComplex");
  jmethodID mid_ctor = (*env)->GetMethodID(env, cls, "<init>", "(FF)V");
  jobject obj = (*env)->NewObject(env,
                                  cls,
                                  mid_ctor,
                                  value->real,
                                  value->imaginary);
  (*env)->DeleteLocalRef(env, cls);
  return obj;
}

/*
 * Extract the string type from the sidl.String.Holder holder class.  The
 * string returned by this function must be freed by the system free() routine
 * or sidl_String_free().
 */
char* sidl_Java_J2I_string_holder(
  JNIEnv* env,
  jobject obj)
{
  static jmethodID mid = (jmethodID) NULL;
  jobject holdee = (jobject) NULL;
  char* string = NULL;

  if (mid == (jmethodID) NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    mid = (*env)->GetMethodID(env, cls, "get", "()Ljava/lang/String;");
    (*env)->DeleteLocalRef(env, cls);
  }

  holdee = (*env)->CallObjectMethod(env, obj, mid);
  string = sidl_Java_J2I_string(env, holdee);
  (*env)->DeleteLocalRef(env, holdee);

  return string;
}

/*
 * Set the string type in the sidl.String.Holder holder class.  An internal
 * copy is made of the string argument; therefore, the caller must free it
 * to avoid a memory leak.
 */
void sidl_Java_I2J_string_holder(
  JNIEnv* env,
  jobject obj,
  const char* value)
{
  static jmethodID mid = (jmethodID) NULL;
  jstring holdee = sidl_Java_I2J_string(env, value);

  if (mid == (jmethodID) NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    mid = (*env)->GetMethodID(env, cls, "set", "(Ljava/lang/String;)V");
    (*env)->DeleteLocalRef(env, cls);
  }

  (*env)->CallVoidMethod(env, obj, mid, holdee);
  (*env)->DeleteLocalRef(env, holdee);
}

/*
 * Extract the string type from the java.lang.String object.  The string
 * returned by this function must be freed by the system free() routine
 * or sidl_String_free().
 */
char* sidl_Java_J2I_string(
  JNIEnv* env,
  jstring str)
{
  char* string = NULL;

  if (str != (jstring) NULL) {
    const char* utf = (*env)->GetStringUTFChars(env, str, NULL);
    string = sidl_String_strdup(utf);
    (*env)->ReleaseStringUTFChars(env, str, utf);
  }

  return string;
}

/*
 * Create a java.lang.String object from the specified input string.  An
 * internal copy is made of the string argument; therefore, the caller must
 * free it to avoid a memory leak.
 */
jstring sidl_Java_I2J_string(
  JNIEnv* env,
  const char* value)
{
  return (*env)->NewStringUTF(env, value);
}

/*
 * Extract the IOR class type from the holder class.  The IOR class type
 * returned by this function will need to be cast to the appropriate IOR
 * type.  The name of the held class must be provided in the java_name.
 */
void* sidl_Java_J2I_cls_holder(
  JNIEnv* env,
  jobject obj,
  const char* java_name)
{
  jclass    cls    = (jclass) NULL;
  jmethodID mid    = (jmethodID) NULL;
  jobject   holdee = (jobject) NULL;
  void*     ptr    = NULL;

  
  char* signature = sidl_String_concat3("()L", java_name, ";");
  sidl_String_replace(signature, '.', '/');

  cls    = (*env)->GetObjectClass(env, obj);
  mid    = (*env)->GetMethodID(env, cls, "get", signature);
  holdee = (*env)->CallObjectMethod(env, obj, mid);
  ptr    = sidl_Java_J2I_cls(env, holdee);
  (*env)->DeleteLocalRef(env, cls);
  (*env)->DeleteLocalRef(env, holdee);
  sidl_String_free(signature);

  return ptr;
}

/*
 * Set the IOR class type in the holder class.  The name of the held class
 * must be provided in the java_name.
 */
void sidl_Java_I2J_cls_holder(
  JNIEnv* env,
  jobject obj,
  void* value,
  const char* java_name)
{
  
  jmethodID mid = (jmethodID) NULL;
  jobject holdee = sidl_Java_I2J_cls(env, value, java_name);
  jclass cls = (*env)->GetObjectClass(env, obj);
  char* signature = sidl_String_concat3("(L", java_name, ";)V");
  sidl_String_replace(signature, '.', '/');

  mid = (*env)->GetMethodID(env, cls, "set", signature);
  (*env)->CallVoidMethod(env, obj, mid, holdee);
  (*env)->DeleteLocalRef(env, cls);
  sidl_String_free(signature);
}

/*
 * Extract the IOR class type from the Java class wrapper.  The IOR class
 * type returned by this function will need to be cast to the appropriate
 * IOR type.
 */
void* sidl_Java_J2I_cls(
  JNIEnv* env,
  jobject obj)
{
  void* ptr = NULL;

  if (obj != NULL) {
    jclass    cls = (*env)->GetObjectClass(env, obj);    
    jmethodID mid = (*env)->GetMethodID(env, cls, "_get_ior", "()J");
    ptr = JLONG_TO_POINTER((*env)->CallLongMethod(env, obj, mid));
    (*env)->DeleteLocalRef(env, cls);
  }

  return ptr;
}

/*
 * Create a new Java class object to represent the sidl class.  The Java
 * class name must be supplied in the java_name argument.
 */
jobject sidl_Java_I2J_cls(
  JNIEnv* env,
  void* value,
  const char* java_name)
{
  struct sidl_BaseInterface__object* ptr = NULL;
  jobject obj = (jobject) NULL;
  if (value != NULL) {
    jclass cls = (jclass) NULL;
    char* name = sidl_String_strdup(java_name);
    sidl_String_replace(name, '.', '/');
    cls = (*env)->FindClass(env, name);
    if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionClear(env);
    }
    if (cls != NULL) {
      jmethodID ctor = (*env)->GetMethodID(env, cls, "<init>", "(J)V");
      if ((*env)->ExceptionCheck(env) || ctor == NULL) {
	(*env)->ExceptionClear(env);
	return NULL;
      } else {
	obj = (*env)->NewObject(env, cls, ctor, POINTER_TO_JLONG(value));
      }
      /* addRef because of the new reference to this object!*/
      ptr = sidl_BaseInterface__cast2(value, "sidl.BaseInterface");
      sidl_BaseInterface_addRef(ptr);
      (*env)->DeleteLocalRef(env, cls);
    }
    sidl_String_free(name);
  }
  return obj;
}

/*
 * Create an empty Java object of the given name.  Good for cerating holders 
 */

jobject sidl_Java_create_empty_class(
  JNIEnv* env,
  const char* java_name)
{
  jobject obj = (jobject) NULL;
  jclass cls = (jclass) NULL;
  char* name = sidl_String_strdup(java_name);
  sidl_String_replace(name, '.', '/');
  cls = (jclass) (*env)->FindClass(env, name);
  if (cls != NULL) {
    jmethodID ctor = (*env)->GetMethodID(env, cls, "<init>", "()V");
    obj = (*env)->NewObject(env, cls, ctor);
    if ((*env)->ExceptionOccurred(env)) {
      (*env)->ExceptionDescribe(env);
      return 0;
  }
    (*env)->DeleteLocalRef(env, cls);
    
  }
  sidl_String_free(name);
  return obj;
}
/*
 * Extract the IOR interface type from the holder class.  The IOR interface
 * type returned by this function will need to be cast to the appropriate IOR
 * type.  The name of the held class must be provided in the java_name.
 */
void* sidl_Java_J2I_ifc_holder(
  JNIEnv* env,
  jobject obj,
  const char* java_name)
{
  jclass    cls    = (jclass) NULL;
  jmethodID mid    = (jmethodID) NULL;
  jobject   holdee = (jobject) NULL;
  void*     ptr    = NULL;

  char* signature = sidl_String_concat3("()L", java_name, ";");
  sidl_String_replace(signature, '.', '/');
  cls    = (*env)->GetObjectClass(env, obj);
  mid    = (*env)->GetMethodID(env, cls, "get", signature);

  holdee = (*env)->CallObjectMethod(env, obj, mid);
  ptr    = sidl_Java_J2I_ifc(env, holdee, java_name);

  (*env)->DeleteLocalRef(env, cls);
  (*env)->DeleteLocalRef(env, holdee);
  sidl_String_free(signature);

  return ptr;
}

/*
 * Set the IOR interface type in the holder class.  The name of the held
 * interface must be provided in the java_name.
 */
void sidl_Java_I2J_ifc_holder(
  JNIEnv* env,
  jobject obj,
  void* value,
  const char* java_name)
{
  jmethodID mid = (jmethodID) NULL;
  jobject holdee = sidl_Java_I2J_ifc(env, value, java_name);
  jclass cls = (*env)->GetObjectClass(env, obj);
  char* signature = sidl_String_concat3("(L", java_name, ";)V");

  sidl_String_replace(signature, '.', '/');

  mid = (*env)->GetMethodID(env, cls, "set", signature);
  (*env)->CallVoidMethod(env, obj, mid, holdee);

  (*env)->DeleteLocalRef(env, cls);
  sidl_String_free(signature);
}

/*
 * Extract the IOR interface type from the Java interface wrapper.  The
 * IOR interface type returned by this function will need to be cast to the
 * appropriate IOR type.  The sidl name of the desired interface must be
 * provided in the sidl_name.
 */
void* sidl_Java_J2I_ifc(
  JNIEnv* env,
  jobject obj,
  const char* sidl_name)
{
  void* ptr = NULL;

  if (obj != NULL) {
    jclass    cls = (*env)->GetObjectClass(env, obj);
    jmethodID mid = NULL;
    void*     ior = NULL;
    mid = (*env)->GetMethodID(env, cls, "_get_ior", "()J");
    if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionClear(env);
    }
    ior = JLONG_TO_POINTER((*env)->CallLongMethod(env, obj, mid));
    if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionClear(env);
    }
    (*env)->DeleteLocalRef(env, cls);
    if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionClear(env);
    }
    ptr = sidl_BaseInterface__cast2(ior, sidl_name);
    
  } 
  
  return ptr;
}

/*
 * Create a new Java object to represent the sidl interface.  The Java
 * class name must be supplied in the java_name argument.
 *
 * FIXME: Note that this function should be smarter and use metadata from
 * the interface to create the actual concrete class instead of its wrapper.
 */
jobject sidl_Java_I2J_ifc(
  JNIEnv* env,
  void* value,
  const char* java_name)
{
  struct sidl_BaseInterface__object* ptr = NULL;
  jobject obj = (jobject) NULL;
  if (value != NULL) {
    jclass cls = (jclass) NULL;
    char* wrapper = sidl_String_concat2(java_name, "$Wrapper");
    sidl_String_replace(wrapper, '.', '/');
    cls = (*env)->FindClass(env, wrapper);
    if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionClear(env);
    }
    if (cls != NULL) {
      jmethodID ctor = (*env)->GetMethodID(env, cls, "<init>", "(J)V");
      if ((*env)->ExceptionCheck(env) || ctor == NULL) {
	(*env)->ExceptionClear(env);
	return NULL;
      }
      obj = (*env)->NewObject(env, cls, ctor, POINTER_TO_JLONG(value));
      if ((*env)->ExceptionCheck(env)) {
	(*env)->ExceptionClear(env);
      }
  
      /* Artifically add ref the Wrapper class reference*/ 
      ptr = sidl_BaseInterface__cast2(value, "sidl.BaseInterface");
      sidl_BaseInterface_addRef(ptr);
     
      (*env)->DeleteLocalRef(env, cls);
    }
    sidl_String_free(wrapper);
  }
  return obj;
}

/*
 * Create a new Java object to represent the sidl interface.  The Java
 * class name must be supplied in the java_name argument.
 *
 * This function is ONLY FOR GETTING OBJECT OUT OF ARRAYS.  It's been created 
 * as a hack to get around a refcount problem in Java.  Basically, all objects
 * on creation need to be refcounted before they are passed to java, however,
 * objects that come from arrays have already by the IOR Array.  The is the 
 * same function as sidl_Java_I2J_ifc but without the addRef.
 * 
 */
jobject sidl_Java_Array2J_ifc(
  JNIEnv* env,
  void* value,
  const char* java_name)
{
  struct sidl_BaseInterface__object* ptr = NULL;
  jobject obj = (jobject) NULL;
  if (value != NULL) {
    jclass cls = (jclass) NULL;
    char* wrapper = sidl_String_concat2(java_name, "$Wrapper");
    sidl_String_replace(wrapper, '.', '/');
    cls = (*env)->FindClass(env, wrapper);
    if ((*env)->ExceptionCheck(env)) {
      (*env)->ExceptionClear(env);
    }
    if (cls != NULL) {
      jmethodID ctor = (*env)->GetMethodID(env, cls, "<init>", "(J)V");
      if ((*env)->ExceptionCheck(env) || ctor == NULL) {
	(*env)->ExceptionClear(env);
	return NULL;
      }
      obj = (*env)->NewObject(env, cls, ctor, POINTER_TO_JLONG(value));
      if ((*env)->ExceptionCheck(env)) {
	(*env)->ExceptionClear(env);
      }
      /* Artifically add ref the Wrapper class reference 
      ptr = sidl_BaseInterface__cast2(value, "sidl.BaseInterface");
      sidl_BaseInterface_addRef(ptr);
      */
      (*env)->DeleteLocalRef(env, cls);
    }
    sidl_String_free(wrapper);
  }
  return obj;
}


/*
 * Set the IOR class type in the holder class.  The name of the held array
 * must be provided in the java_name.
 */
void sidl_Java_I2J_array_holder(
  JNIEnv* env,
  jobject obj,
  void* value,
  const char* java_name)
{
  
  jmethodID mid = (jmethodID) NULL;
  jobject holdee = sidl_Java_I2J_new_array(env, value, java_name);
  jclass cls = (*env)->GetObjectClass(env, obj);
  char* signature = sidl_String_concat3("(L", java_name, ";)V");
  sidl_String_replace(signature, '.', '/');

  mid = (*env)->GetMethodID(env, cls, "set", signature);
  (*env)->CallVoidMethod(env, obj, mid, holdee);
  (*env)->DeleteLocalRef(env, cls);
  sidl_String_free(signature);
}

/*
 * Extract the IOR array type from the holder class.  The IOR array type
 * returned by this function will need to be cast to the appropriate IOR
 * type.  The name of the held class must be provided in the java_name.
 */
void* sidl_Java_J2I_array_holder(
  JNIEnv* env,
  jobject obj,
  const char* java_name)
{
  jclass    cls    = (jclass) NULL;
  jmethodID mid    = (jmethodID) NULL;
  jobject   holdee = (jobject) NULL;
  void*     ptr    = NULL;

  char* signature = sidl_String_concat3("()L", java_name, ";");
  if(obj != NULL) {
  
    sidl_String_replace(signature, '.', '/');
    
    cls    = (*env)->GetObjectClass(env, obj);
    if(cls != NULL) {
      mid    = (*env)->GetMethodID(env, cls, "get", signature);
      holdee = (*env)->CallObjectMethod(env, obj, mid);
      if(holdee != NULL) {
	ptr    = sidl_Java_J2I_take_array(env, holdee);	
	(*env)->DeleteLocalRef(env, holdee);
	sidl_String_free(signature);
      }
      (*env)->DeleteLocalRef(env, cls);
    }
  }
  return ptr;
}

/*
 * Extract the sidl array pointer from the Java array object.  This method
 * simply "borrows" the pointer; the sidl array remains the owner of the array
 * data.  This is used for "in" arguments.
 */
void* sidl_Java_J2I_borrow_array(
  JNIEnv* env,
  jobject obj)
{
  void* array = NULL;
  if (obj != NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    jmethodID mid = (*env)->GetMethodID(env, cls, "_addRef", "()V");
    jfieldID array_field = (*env)->GetFieldID(env, cls, "d_array", "J");
    (*env)->DeleteLocalRef(env, cls);
    array = JLONG_TO_POINTER((*env)->GetLongField(env, obj, array_field));
    if(array != NULL)
      (*env)->CallVoidMethod(env, obj, mid);
  }
  return array;
}

/*
 * Extract the sidl array pointer from the Java array object.  This method
 * "takes" the pointer; responsibility for the sidl array is transferred to
 * the IOR code.  This is used for "inout" arguments.
 */
void* sidl_Java_J2I_take_array(
  JNIEnv* env,
  jobject obj)
{
  void* array = NULL;

  if (obj != NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    jfieldID array_field = (*env)->GetFieldID(env, cls, "d_array", "J");
    jfieldID owner_field = (*env)->GetFieldID(env, cls, "d_owner", "Z");

    array = JLONG_TO_POINTER((*env)->GetLongField(env, obj, array_field));

    (*env)->SetBooleanField(env, obj, owner_field, JNI_FALSE);
    (*env)->SetLongField(env, obj, array_field, POINTER_TO_JLONG(NULL));
    
    (*env)->DeleteLocalRef(env, cls);

  }

  return array;
}

/*
 * Change the current Java array object to point to the specified sidl
 * IOR object. 
 */
void sidl_Java_I2J_set_array(
  JNIEnv* env,
  jobject obj,
  void* value)
{
  jclass    cls = (*env)->GetObjectClass(env, obj);
  jmethodID mid = (*env)->GetMethodID(env, cls, "reset", "(JZ)V");
  (*env)->CallVoidMethod(env, obj, mid, POINTER_TO_JLONG(value), JNI_TRUE);
  (*env)->DeleteLocalRef(env, cls);
}

/*
 * Create a new array object from the sidl IOR object.  The array_name
 * argument must provide the name of the Java array type.  This version
 * takes the array reference.  
 */
jobject sidl_Java_I2J_new_array(
  JNIEnv* env,
  void* value,
  const char* array_name)
{
  char*   jni_name = sidl_String_strdup(array_name);
  jclass  cls      = (jclass) NULL;
  jobject obj      = (jobject) NULL;
  sidl_String_replace(jni_name, '.', '/');
  cls = (*env)->FindClass(env, jni_name);
  sidl_String_free(jni_name);

  if (cls) {
    jmethodID ctor = (*env)->GetMethodID(env, cls, "<init>", "(JZ)V");
    obj = (*env)->NewObject(env, cls, ctor, POINTER_TO_JLONG(value), JNI_TRUE);
    (*env)->DeleteLocalRef(env, cls);
  }

  return obj;
}

/*
 * Create a new array object from the sidl IOR object.  The array_name
 * argument must provide the name of the Java array type.  This version
 * borrows the reference.
 */
jobject sidl_Java_I2J_new_array_server(
  JNIEnv* env,
  void* value,
  const char* array_name)
{
  char*   jni_name = sidl_String_strdup(array_name);
  jclass  cls      = (jclass) NULL;
  jobject obj      = (jobject) NULL;
  sidl_String_replace(jni_name, '.', '/');
  cls = (*env)->FindClass(env, jni_name);
  sidl_String_free(jni_name);

  if (cls) {
    jmethodID ctor = (*env)->GetMethodID(env, cls, "<init>", "(JZ)V");
    obj = (*env)->NewObject(env, cls, ctor, POINTER_TO_JLONG(value), JNI_FALSE);
    (*env)->DeleteLocalRef(env, cls);
  }

  return obj;
}


/* 
 * Local utility function to extract the array pointer from the Java object.
 * Extract the d_array long data member and convert it to a pointer.
 */
static struct sidl_bool__array* sidl_Boolean__getptr(
  JNIEnv* env,
  jobject obj)
{
  void* ptr = NULL;
  static jfieldID s_array_field = NULL;

  if (s_array_field == NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    s_array_field = (*env)->GetFieldID(env, cls, "d_array", "J");
    (*env)->DeleteLocalRef(env, cls);
  }

  ptr = JLONG_TO_POINTER((*env)->GetLongField(env, obj, s_array_field));
  return (struct sidl_bool__array*) ptr;
}

/*
 * Local utility function to set the array pointer on the Java object.
 * Convert the pointer to a long value and set the d_array data member.
 */
static void sidl_Boolean__setptr(
  JNIEnv* env,
  jobject obj,
  struct sidl_bool__array* array)
{
  static jfieldID s_array_field = NULL;

  if (s_array_field == NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    s_array_field = (*env)->GetFieldID(env, cls, "d_array", "J");
    (*env)->DeleteLocalRef(env, cls);
  }

  (*env)->SetLongField(env, obj, s_array_field, POINTER_TO_JLONG(array));
}

/*
 * Native routine to get the dimension of the current array.  This
 * routine assumes that the array has already been initialized.  If
 * the array has not been initialized, then horrible things may happen.
 */
static jint sidl_Boolean__dim(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_bool__array* array = sidl_Boolean__getptr(env, obj);
  return (jint) sidl_bool__array_dimen(array);
}

/*
 * Native routine to fetch the specified lower bound of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_Boolean__lower(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_bool__array* array = sidl_Boolean__getptr(env, obj);
  return (jint) sidl_bool__array_lower(array, dim);
}

/*
 * Native routine to fetch the specified upper bound of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_Boolean__upper(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_bool__array* array = sidl_Boolean__getptr(env, obj);
  return (jint) sidl_bool__array_upper(array, dim);
}

/*
 * Native routine to fetch the specified stride of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_Boolean__stride(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_bool__array* array = sidl_Boolean__getptr(env, obj);
  return (jint) sidl_bool__array_stride(array, dim);
}

/*
 * Native routine to returns true if the array is Column order.  
 */
static jboolean sidl_Boolean__isColumnOrder(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_bool__array* array = sidl_Boolean__getptr(env, obj);
#if (JNI_TRUE == TRUE) && (JNI_FALSE == FALSE)
  return (jboolean) sidl_bool__array_isColumnOrder(array);
#else
  return sidl_bool__array_isColumnOrder(array) ? JNI_TRUE : JNI_FALSE;
#endif
}
/*
 * Native routine returns the if array is RowOrder.  
 */
static jint sidl_Boolean__isRowOrder(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_bool__array* array = sidl_Boolean__getptr(env, obj);
#if (JNI_TRUE == TRUE) && (JNI_FALSE == FALSE)
  return (jboolean) sidl_bool__array_isRowOrder(array);
#else
  return sidl_bool__array_isRowOrder(array) ? JNI_TRUE : JNI_FALSE;
#endif
}

/*
 * Native routine to fetch the specified value from the array.  The
 * specified array index/indices must be lie between the array lower
 * upper bounds (inclusive).  Invalid indices will have unpredictable
 * (but almost certainly bad) results.
 */
static jboolean sidl_Boolean__get(
  JNIEnv* env,
  jobject obj,
  jint i,
  jint j,
  jint k,
  jint l,
  jint m,
  jint n,
  jint o)
{
  struct sidl_bool__array* array = sidl_Boolean__getptr(env, obj);
  int32_t a[7];

  a[0] = i; a[1] = j; a[2] = k; a[3] = l; a[4] = m; a[5] = n; a[6] = o;
#if (JNI_TRUE == TRUE) && (JNI_FALSE == FALSE)
  return (jboolean) sidl_bool__array_get(array, a);
#else
  return sidl_bool__array_get(array, a) ? JNI_TRUE : JNI_FALSE;
#endif
}

/**
 * Native routine to set the specified value in the array.  The
 * specified array index/indices must be lie between the array lower
 * upper bounds (inclusive).  Invalid indices will have unpredictable
 * (but almost certainly bad) results.
 */
static void sidl_Boolean__set(
  JNIEnv* env,
  jobject obj,
  jint i,
  jint j,
  jint k,
  jint l,
  jint m, 
  jint n, 
  jint o,
  jboolean value)
{
  struct sidl_bool__array* array = sidl_Boolean__getptr(env, obj);
  int32_t a[7];

  a[0] = i; a[1] = j; a[2] = k; a[3] = l; a[4] = m; a[5] = n; a[6] = o;
#if (JNI_TRUE == TRUE) && (JNI_FALSE == FALSE)
  sidl_bool__array_set(array, a, (sidl_bool) value);
#else
  sidl_bool__array_set(array, a, ((value) ? TRUE : FALSE));
#endif
}


/*
 * Native function copies borrowed arrays, or increments the reference count
 * of non-borrowed arrays.  Good if you want to keep a copy of a passed
 * in array.  Returns an array, new if borrowed.
 */
static jobject sidl_Boolean__smartCopy(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_bool__array* array = sidl_Boolean__getptr(env, obj);
  struct sidl_bool__array* ret_ptr = NULL;
  jobject ret_array = NULL;
  
  if (array != NULL) {
    ret_ptr = sidl_bool__array_smartCopy(array);
  }

  /* Assuming our pointer comes back good, crate a new java array with the correct pointer,
     and owner and pass that back */
  if(ret_ptr != NULL) {  
    ret_array = sidl_Java_I2J_new_array(env,ret_ptr, "sidl.Boolean$Array");
  }
    return ret_array;
}


/*
 * Native function copies borrowed arrays, or increments the reference count
 * of non-borrowed arrays.  Good if you want to keep a copy of a passed
 * in array.  Returns an array, new if borrowed.
 */
static void sidl_Boolean__copy(
  JNIEnv* env,
  jobject obj,
  jobject dest)
{
  struct sidl_bool__array* csrc = sidl_Boolean__getptr(env, obj);
  struct sidl_bool__array* cdest = sidl_Boolean__getptr(env, dest);
      
  if(csrc && cdest) {
    sidl_bool__array_copy(csrc,cdest);
  } 

}

/*
 * Native function slices arrays in various ways
 */
static jobject sidl_Boolean__slice(
  JNIEnv* env,
  jobject obj, 
  jint dimen, 
  jintArray numElem, 
  jintArray srcStart, 
  jintArray srcStride,
  jintArray newStart)
{
  struct sidl_bool__array* array = sidl_Boolean__getptr(env, obj);
  struct sidl_bool__array* ret_ptr = NULL;
  jobject ret_array = NULL;
  jint* cnumElem = NULL;
  jint* csrcStart = NULL;
  jint* csrcStride = NULL;
  jint* cnewStart = NULL; 
  int i = 0; 

  if(numElem == NULL) {
    return NULL;  /*If numElem is NULL, we need to return Null, that's bad.*/
  } else {
    if((*env)->GetArrayLength(env, numElem) > 7)
      return NULL;  
    cnumElem = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      cnumElem[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, numElem, 0,
			      (*env)->GetArrayLength(env, numElem), cnumElem); 
    
  }

  if(srcStart != NULL) {
    if((*env)->GetArrayLength(env, srcStart) > 7)
      return NULL; 
    csrcStart = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      csrcStart[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, srcStart, 0, 
			    (*env)->GetArrayLength(env, srcStart), csrcStart);   
  } 

  if(srcStride != NULL) {
    if((*env)->GetArrayLength(env, srcStride) > 7)
      return NULL; 
    csrcStride = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      csrcStride[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, srcStride, 0, 
			      (*env)->GetArrayLength(env, srcStride), csrcStride);
  } 

  if(newStart != NULL) {
    if((*env)->GetArrayLength(env, newStart) > 7)
      return NULL; 
    cnewStart = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      cnewStart[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, newStart, 0, 
			      (*env)->GetArrayLength(env, newStart), cnewStart);    
  } 

  if (array != NULL) {
    /* jint should be equivalent to int32_t; Java int is 32 bits */
    ret_ptr = sidl_bool__array_slice(array, dimen, cnumElem, csrcStart, csrcStride, cnewStart);
  }

  if(ret_ptr != NULL) {  
    ret_array = sidl_Java_I2J_new_array(env,ret_ptr, "sidl.Boolean$Array");
  }

   return ret_array;
}

/*
 * Native routine to increment the current array reference count.
 */
static void sidl_Boolean__addRef(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_bool__array* array = sidl_Boolean__getptr(env, obj);
  if (array != NULL) {
    sidl_bool__array_addRef(array);
  }
}

/*
 * Native routine to decrement the current array reference count.
 */
static void sidl_Boolean__deallocate(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_bool__array* array = sidl_Boolean__getptr(env, obj);
  if (array != NULL) {
    sidl_bool__array_deleteRef(array);
  }
  /*sidl_Boolean__setptr(env, obj, NULL);*/
}

/*
 * Native routine to destroy (deallocate) the current array data.
 */
static void sidl_Boolean__destroy(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_bool__array* array = sidl_Boolean__getptr(env, obj);
  if (array != NULL) {
    sidl_bool__array_deleteRef(array);
  }
  sidl_Boolean__setptr(env, obj, NULL);
}

/*
 * Native routine to reallocate data in the array.  The specified array
 * dimension and indices must match and be within valid ranges (e.g., the
 * upper bounds must be greater than or equal to lowe rbounds.  Invalid
 * indices will have unpredictable (but almost certainly bad) results.
 * This routine will deallocate the existing array data if it is not null.
 * if argument "isRow" is true, the array will be in RowOrder, other wise
 * it will be in ColumnOrder   
 */
static void sidl_Boolean__reallocate(
  JNIEnv* env,
  jobject obj,
  jint dim,
  jarray lower,
  jarray upper,
  jboolean isRow)
{
  jint* l = NULL;
  jint* u = NULL;
  struct sidl_bool__array* array = NULL;

  sidl_Boolean__destroy(env, obj);

  l = (*env)->GetIntArrayElements(env, lower, NULL);
  u = (*env)->GetIntArrayElements(env, upper, NULL);
  if(isRow)  
    array = sidl_bool__array_createRow((int32_t) dim, (int32_t*) l,
                                          (int32_t*) u);
  else
    array = sidl_bool__array_createCol((int32_t) dim, (int32_t*) l,
                                          (int32_t*) u);
  (*env)->ReleaseIntArrayElements(env, lower, l, JNI_ABORT);
  (*env)->ReleaseIntArrayElements(env, upper, u, JNI_ABORT);

  sidl_Boolean__setptr(env, obj, array);
}

/*
 * Register JNI array methods with the Java JVM.
 */
void sidl_Boolean__register(JNIEnv* env)
{
  JNINativeMethod methods[15];
  jclass cls;

  methods[0].name      = "_dim";
  methods[0].signature = "()I";
  methods[0].fnPtr     = sidl_Boolean__dim;
  methods[1].name      = "_lower";
  methods[1].signature = "(I)I";
  methods[1].fnPtr     = sidl_Boolean__lower;
  methods[2].name      = "_upper";
  methods[2].signature = "(I)I";
  methods[2].fnPtr     = sidl_Boolean__upper;
  methods[3].name      = "_get";
  methods[3].signature = "(IIIIIII)Z";
  methods[3].fnPtr     = sidl_Boolean__get;
  methods[4].name      = "_set";
  methods[4].signature = "(IIIIIIIZ)V";
  methods[4].fnPtr     = sidl_Boolean__set;
  methods[5].name      = "_destroy";
  methods[5].signature = "()V";
  methods[5].fnPtr     = sidl_Boolean__destroy;
  methods[6].name      = "_reallocate";
  methods[6].signature = "(I[I[IZ)V";
  methods[6].fnPtr     = sidl_Boolean__reallocate;
  methods[7].name      = "_stride";
  methods[7].signature = "(I)I";
  methods[7].fnPtr     = sidl_Boolean__stride;  
  methods[8].name      = "_isColumnOrder";
  methods[8].signature = "()Z";
  methods[8].fnPtr     = sidl_Boolean__isColumnOrder;       
  methods[9].name      = "_isRowOrder";
  methods[9].signature = "()Z";
  methods[9].fnPtr     = sidl_Boolean__isRowOrder;
  methods[10].name      = "_deallocate";
  methods[10].signature = "()V";
  methods[10].fnPtr     = sidl_Boolean__deallocate;
  methods[11].name      = "_copy";
  methods[11].signature = "(Lsidl/Boolean$Array;)V";
  methods[11].fnPtr     = sidl_Boolean__copy;  
  methods[12].name      = "_smartCopy";
  methods[12].signature = "()Lsidl/Boolean$Array;";
  methods[12].fnPtr     = sidl_Boolean__smartCopy;    
  methods[13].name      = "_slice";
  methods[13].signature = "(I[I[I[I[I)Lsidl/Boolean$Array;";
  methods[13].fnPtr     = sidl_Boolean__slice;    
  methods[14].name      = "_addRef";
  methods[14].signature = "()V";
  methods[14].fnPtr     = sidl_Boolean__addRef;

  cls = (*env)->FindClass(env, "sidl/Boolean$Array");
  if (cls) {
    (*env)->RegisterNatives(env, cls, methods, 15);
    (*env)->DeleteLocalRef(env, cls);
  }
}

/* 
 * Local utility function to extract the array pointer from the Java object.
 * Extract the d_array long data member and convert it to a pointer.
 */
static struct sidl_char__array* sidl_Character__getptr(
  JNIEnv* env,
  jobject obj)
{
  void* ptr = NULL;
  static jfieldID s_array_field = NULL;

  if (s_array_field == NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    s_array_field = (*env)->GetFieldID(env, cls, "d_array", "J");
    (*env)->DeleteLocalRef(env, cls);
  }

  ptr = JLONG_TO_POINTER((*env)->GetLongField(env, obj, s_array_field));
  return (struct sidl_char__array*) ptr;
}

/*
 * Local utility function to set the array pointer on the Java object.
 * Convert the pointer to a long value and set the d_array data member.
 */
static void sidl_Character__setptr(
  JNIEnv* env,
  jobject obj,
  struct sidl_char__array* array)
{
  static jfieldID s_array_field = NULL;

  if (s_array_field == NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    s_array_field = (*env)->GetFieldID(env, cls, "d_array", "J");
    (*env)->DeleteLocalRef(env, cls);
  }

  (*env)->SetLongField(env, obj, s_array_field, POINTER_TO_JLONG(array));
}

/*
 * Native routine to get the dimension of the current array.  This
 * routine assumes that the array has already been initialized.  If
 * the array has not been initialized, then horrible things may happen.
 */
static jint sidl_Character__dim(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_char__array* array = sidl_Character__getptr(env, obj);
  return (jint) sidl_char__array_dimen(array);
}

/*
 * Native routine to fetch the specified lower bound of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_Character__lower(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_char__array* array = sidl_Character__getptr(env, obj);
  return (jint) sidl_char__array_lower(array, dim);
}

/*
 * Native routine to fetch the specified upper bound of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_Character__upper(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_char__array* array = sidl_Character__getptr(env, obj);
  return (jint) sidl_char__array_upper(array, dim);
}

/*
 * Native routine to fetch the specified stride of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_Character__stride(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_char__array* array = sidl_Character__getptr(env, obj);
  return (jint) sidl_char__array_stride(array, dim);
}

/*
 * Native routine to returns true if the array is Column order.  
 */
static jboolean sidl_Character__isColumnOrder(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_char__array* array = sidl_Character__getptr(env, obj);
#if (JNI_TRUE == TRUE) && (JNI_FALSE == FALSE)
  return (jboolean) sidl_char__array_isColumnOrder(array);
#else
  return sidl_char__array_isColumnOrder(array) ? JNI_TRUE : JNI_FALSE;
#endif
}
/*
 * Native routine returns the if array is RowOrder.  
 */
static jint sidl_Character__isRowOrder(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_char__array* array = sidl_Character__getptr(env, obj);
#if (JNI_TRUE == TRUE) && (JNI_FALSE == FALSE)
  return (jboolean) sidl_char__array_isRowOrder(array);
#else
  return sidl_char__array_isRowOrder(array) ? JNI_TRUE : JNI_FALSE;
#endif
}

/*
 * Native routine to fetch the specified value from the array.  The
 * specified array index/indices must be lie between the array lower
 * upper bounds (inclusive).  Invalid indices will have unpredictable
 * (but almost certainly bad) results.
 */
static jchar sidl_Character__get(
  JNIEnv* env,
  jobject obj,
  jint i,
  jint j,
  jint k,
  jint l,
  jint m,
  jint n,
  jint o)
{
  struct sidl_char__array* array = sidl_Character__getptr(env, obj);
  int32_t a[7];

  a[0] = i; a[1] = j; a[2] = k; a[3] = l; a[4] = m; a[5] = n; a[6] = o;
  return (jchar) sidl_char__array_get(array, a);
}

/**
 * Native routine to set the specified value in the array.  The
 * specified array index/indices must be lie between the array lower
 * upper bounds (inclusive).  Invalid indices will have unpredictable
 * (but almost certainly bad) results.
 */
static void sidl_Character__set(
  JNIEnv* env,
  jobject obj,
  jint i,
  jint j,
  jint k,
  jint l,
  jint m, 
  jint n, 
  jint o,
  jchar value)
{
  struct sidl_char__array* array = sidl_Character__getptr(env, obj);
  int32_t a[7];

  a[0] = i; a[1] = j; a[2] = k; a[3] = l; a[4] = m; a[5] = n; a[6] = o;
  sidl_char__array_set(array, a, (char) value);
}


/*
 * Native function copies borrowed arrays, or increments the reference count
 * of non-borrowed arrays.  Good if you want to keep a copy of a passed
 * in array.  Returns an array, new if borrowed.
 */
static jobject sidl_Character__smartCopy(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_char__array* array = sidl_Character__getptr(env, obj);
  struct sidl_char__array* ret_ptr = NULL;
  jobject ret_array = NULL;
  
  if (array != NULL) {
    ret_ptr = sidl_char__array_smartCopy(array);
  }

  /* Assuming our pointer comes back good, crate a new java array with the correct pointer,
     and owner and pass that back */
  if(ret_ptr != NULL) {  
    ret_array = sidl_Java_I2J_new_array(env,ret_ptr, "sidl.Character$Array");
  }
    return ret_array;
}


/*
 * Native function copies borrowed arrays, or increments the reference count
 * of non-borrowed arrays.  Good if you want to keep a copy of a passed
 * in array.  Returns an array, new if borrowed.
 */
static void sidl_Character__copy(
  JNIEnv* env,
  jobject obj,
  jobject dest)
{
  struct sidl_char__array* csrc = sidl_Character__getptr(env, obj);
  struct sidl_char__array* cdest = sidl_Character__getptr(env, dest);
      
  if(csrc && cdest) {
    sidl_char__array_copy(csrc,cdest);
  } 

}

/*
 * Native function slices arrays in various ways
 */
static jobject sidl_Character__slice(
  JNIEnv* env,
  jobject obj, 
  jint dimen, 
  jintArray numElem, 
  jintArray srcStart, 
  jintArray srcStride,
  jintArray newStart)
{
  struct sidl_char__array* array = sidl_Character__getptr(env, obj);
  struct sidl_char__array* ret_ptr = NULL;
  jobject ret_array = NULL;
  jint* cnumElem = NULL;
  jint* csrcStart = NULL;
  jint* csrcStride = NULL;
  jint* cnewStart = NULL; 
  int i = 0; 

  if(numElem == NULL) {
    return NULL;  /*If numElem is NULL, we need to return Null, that's bad.*/
  } else {
    if((*env)->GetArrayLength(env, numElem) > 7)
      return NULL;  
    cnumElem = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      cnumElem[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, numElem, 0,
			      (*env)->GetArrayLength(env, numElem), cnumElem); 
    
  }

  if(srcStart != NULL) {
    if((*env)->GetArrayLength(env, srcStart) > 7)
      return NULL; 
    csrcStart = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      csrcStart[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, srcStart, 0, 
			    (*env)->GetArrayLength(env, srcStart), csrcStart);   
  } 

  if(srcStride != NULL) {
    if((*env)->GetArrayLength(env, srcStride) > 7)
      return NULL; 
    csrcStride = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      csrcStride[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, srcStride, 0, 
			      (*env)->GetArrayLength(env, srcStride), csrcStride);
  } 

  if(newStart != NULL) {
    if((*env)->GetArrayLength(env, newStart) > 7)
      return NULL; 
    cnewStart = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      cnewStart[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, newStart, 0, 
			      (*env)->GetArrayLength(env, newStart), cnewStart);    
  } 

  if (array != NULL) {
    /* jint should be equivalent to int32_t; Java int is 32 bits */
    ret_ptr = sidl_char__array_slice(array, dimen, cnumElem, csrcStart, csrcStride, cnewStart);
  }

  if(ret_ptr != NULL) {  
    ret_array = sidl_Java_I2J_new_array(env,ret_ptr, "sidl.Character$Array");
  }

   return ret_array;
}

/*
 * Native routine to increment the current array reference count.
 */
static void sidl_Character__addRef(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_char__array* array = sidl_Character__getptr(env, obj);
  if (array != NULL) {
    sidl_char__array_addRef(array);
  }
}

/*
 * Native routine to decrement the current array reference count.
 */
static void sidl_Character__deallocate(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_char__array* array = sidl_Character__getptr(env, obj);
  if (array != NULL) {
    sidl_char__array_deleteRef(array);
  }
  /*sidl_Character__setptr(env, obj, NULL);*/
}

/*
 * Native routine to destroy (deallocate) the current array data.
 */
static void sidl_Character__destroy(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_char__array* array = sidl_Character__getptr(env, obj);
  if (array != NULL) {
    sidl_char__array_deleteRef(array);
  }
  sidl_Character__setptr(env, obj, NULL);
}

/*
 * Native routine to reallocate data in the array.  The specified array
 * dimension and indices must match and be within valid ranges (e.g., the
 * upper bounds must be greater than or equal to lowe rbounds.  Invalid
 * indices will have unpredictable (but almost certainly bad) results.
 * This routine will deallocate the existing array data if it is not null.
 * if argument "isRow" is true, the array will be in RowOrder, other wise
 * it will be in ColumnOrder   
 */
static void sidl_Character__reallocate(
  JNIEnv* env,
  jobject obj,
  jint dim,
  jarray lower,
  jarray upper,
  jboolean isRow)
{
  jint* l = NULL;
  jint* u = NULL;
  struct sidl_char__array* array = NULL;

  sidl_Character__destroy(env, obj);

  l = (*env)->GetIntArrayElements(env, lower, NULL);
  u = (*env)->GetIntArrayElements(env, upper, NULL);
  if(isRow)  
    array = sidl_char__array_createRow((int32_t) dim, (int32_t*) l,
                                          (int32_t*) u);
  else
    array = sidl_char__array_createCol((int32_t) dim, (int32_t*) l,
                                          (int32_t*) u);
  (*env)->ReleaseIntArrayElements(env, lower, l, JNI_ABORT);
  (*env)->ReleaseIntArrayElements(env, upper, u, JNI_ABORT);

  sidl_Character__setptr(env, obj, array);
}

/*
 * Register JNI array methods with the Java JVM.
 */
void sidl_Character__register(JNIEnv* env)
{
  JNINativeMethod methods[15];
  jclass cls;

  methods[0].name      = "_dim";
  methods[0].signature = "()I";
  methods[0].fnPtr     = sidl_Character__dim;
  methods[1].name      = "_lower";
  methods[1].signature = "(I)I";
  methods[1].fnPtr     = sidl_Character__lower;
  methods[2].name      = "_upper";
  methods[2].signature = "(I)I";
  methods[2].fnPtr     = sidl_Character__upper;
  methods[3].name      = "_get";
  methods[3].signature = "(IIIIIII)C";
  methods[3].fnPtr     = sidl_Character__get;
  methods[4].name      = "_set";
  methods[4].signature = "(IIIIIIIC)V";
  methods[4].fnPtr     = sidl_Character__set;
  methods[5].name      = "_destroy";
  methods[5].signature = "()V";
  methods[5].fnPtr     = sidl_Character__destroy;
  methods[6].name      = "_reallocate";
  methods[6].signature = "(I[I[IZ)V";
  methods[6].fnPtr     = sidl_Character__reallocate;
  methods[7].name      = "_stride";
  methods[7].signature = "(I)I";
  methods[7].fnPtr     = sidl_Character__stride;  
  methods[8].name      = "_isColumnOrder";
  methods[8].signature = "()Z";
  methods[8].fnPtr     = sidl_Character__isColumnOrder;       
  methods[9].name      = "_isRowOrder";
  methods[9].signature = "()Z";
  methods[9].fnPtr     = sidl_Character__isRowOrder;
  methods[10].name      = "_deallocate";
  methods[10].signature = "()V";
  methods[10].fnPtr     = sidl_Character__deallocate;
  methods[11].name      = "_copy";
  methods[11].signature = "(Lsidl/Character$Array;)V";
  methods[11].fnPtr     = sidl_Character__copy;  
  methods[12].name      = "_smartCopy";
  methods[12].signature = "()Lsidl/Character$Array;";
  methods[12].fnPtr     = sidl_Character__smartCopy;    
  methods[13].name      = "_slice";
  methods[13].signature = "(I[I[I[I[I)Lsidl/Character$Array;";
  methods[13].fnPtr     = sidl_Character__slice;    
  methods[14].name      = "_addRef";
  methods[14].signature = "()V";
  methods[14].fnPtr     = sidl_Character__addRef;

  cls = (*env)->FindClass(env, "sidl/Character$Array");
  if (cls) {
    (*env)->RegisterNatives(env, cls, methods, 15);
    (*env)->DeleteLocalRef(env, cls);
  }
}

/* 
 * Local utility function to extract the array pointer from the Java object.
 * Extract the d_array long data member and convert it to a pointer.
 */
static struct sidl_dcomplex__array* sidl_DoubleComplex__getptr(
  JNIEnv* env,
  jobject obj)
{
  void* ptr = NULL;
  static jfieldID s_array_field = NULL;

  if (s_array_field == NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    s_array_field = (*env)->GetFieldID(env, cls, "d_array", "J");
    (*env)->DeleteLocalRef(env, cls);
  }

  ptr = JLONG_TO_POINTER((*env)->GetLongField(env, obj, s_array_field));
  return (struct sidl_dcomplex__array*) ptr;
}

/*
 * Local utility function to set the array pointer on the Java object.
 * Convert the pointer to a long value and set the d_array data member.
 */
static void sidl_DoubleComplex__setptr(
  JNIEnv* env,
  jobject obj,
  struct sidl_dcomplex__array* array)
{
  static jfieldID s_array_field = NULL;

  if (s_array_field == NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    s_array_field = (*env)->GetFieldID(env, cls, "d_array", "J");
    (*env)->DeleteLocalRef(env, cls);
  }

  (*env)->SetLongField(env, obj, s_array_field, POINTER_TO_JLONG(array));
}

/*
 * Native routine to get the dimension of the current array.  This
 * routine assumes that the array has already been initialized.  If
 * the array has not been initialized, then horrible things may happen.
 */
static jint sidl_DoubleComplex__dim(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_dcomplex__array* array = sidl_DoubleComplex__getptr(env, obj);
  return (jint) sidl_dcomplex__array_dimen(array);
}

/*
 * Native routine to fetch the specified lower bound of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_DoubleComplex__lower(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_dcomplex__array* array = sidl_DoubleComplex__getptr(env, obj);
  return (jint) sidl_dcomplex__array_lower(array, dim);
}

/*
 * Native routine to fetch the specified upper bound of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_DoubleComplex__upper(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_dcomplex__array* array = sidl_DoubleComplex__getptr(env, obj);
  return (jint) sidl_dcomplex__array_upper(array, dim);
}

/*
 * Native routine to fetch the specified stride of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_DoubleComplex__stride(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_dcomplex__array* array = sidl_DoubleComplex__getptr(env, obj);
  return (jint) sidl_dcomplex__array_stride(array, dim);
}

/*
 * Native routine to returns true if the array is Column order.  
 */
static jboolean sidl_DoubleComplex__isColumnOrder(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_dcomplex__array* array = sidl_DoubleComplex__getptr(env, obj);
#if (JNI_TRUE == TRUE) && (JNI_FALSE == FALSE)
  return (jboolean) sidl_dcomplex__array_isColumnOrder(array);
#else
  return sidl_dcomplex__array_isColumnOrder(array) ? JNI_TRUE : JNI_FALSE;
#endif
}
/*
 * Native routine returns the if array is RowOrder.  
 */
static jint sidl_DoubleComplex__isRowOrder(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_dcomplex__array* array = sidl_DoubleComplex__getptr(env, obj);
#if (JNI_TRUE == TRUE) && (JNI_FALSE == FALSE)
  return (jboolean) sidl_dcomplex__array_isRowOrder(array);
#else
  return sidl_dcomplex__array_isRowOrder(array) ? JNI_TRUE : JNI_FALSE;
#endif
}

/*
 * Native routine to fetch the specified value from the array.  The
 * specified array index/indices must be lie between the array lower
 * upper bounds (inclusive).  Invalid indices will have unpredictable
 * (but almost certainly bad) results.
 */
static jobject sidl_DoubleComplex__get(
  JNIEnv* env,
  jobject obj,
  jint i,
  jint j,
  jint k,
  jint l,
  jint m,
  jint n,
  jint o)
{
  struct sidl_dcomplex__array* array = sidl_DoubleComplex__getptr(env, obj);
  int32_t a[7];
  struct sidl_dcomplex value;

  a[0] = i; a[1] = j; a[2] = k; a[3] = l; a[4] = m; a[5] = n; a[6] = o;
  value = sidl_dcomplex__array_get(array, a);
  return sidl_Java_I2J_dcomplex(env, &value);
}

/**
 * Native routine to set the specified value in the array.  The
 * specified array index/indices must be lie between the array lower
 * upper bounds (inclusive).  Invalid indices will have unpredictable
 * (but almost certainly bad) results.
 */
static void sidl_DoubleComplex__set(
  JNIEnv* env,
  jobject obj,
  jint i,
  jint j,
  jint k,
  jint l,
  jint m, 
  jint n, 
  jint o,
  jobject value)
{
  struct sidl_dcomplex__array* array = sidl_DoubleComplex__getptr(env, obj);
  int32_t a[7];
  struct sidl_dcomplex elem;

  a[0] = i; a[1] = j; a[2] = k; a[3] = l; a[4] = m; a[5] = n; a[6] = o;
  elem = sidl_Java_J2I_dcomplex(env, value);
  sidl_dcomplex__array_set(array, a, elem);
}


/*
 * Native function copies borrowed arrays, or increments the reference count
 * of non-borrowed arrays.  Good if you want to keep a copy of a passed
 * in array.  Returns an array, new if borrowed.
 */
static jobject sidl_DoubleComplex__smartCopy(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_dcomplex__array* array = sidl_DoubleComplex__getptr(env, obj);
  struct sidl_dcomplex__array* ret_ptr = NULL;
  jobject ret_array = NULL;
  
  if (array != NULL) {
    ret_ptr = sidl_dcomplex__array_smartCopy(array);
  }

  /* Assuming our pointer comes back good, crate a new java array with the correct pointer,
     and owner and pass that back */
  if(ret_ptr != NULL) {  
    ret_array = sidl_Java_I2J_new_array(env,ret_ptr, "sidl.DoubleComplex$Array");
  }
    return ret_array;
}


/*
 * Native function copies borrowed arrays, or increments the reference count
 * of non-borrowed arrays.  Good if you want to keep a copy of a passed
 * in array.  Returns an array, new if borrowed.
 */
static void sidl_DoubleComplex__copy(
  JNIEnv* env,
  jobject obj,
  jobject dest)
{
  struct sidl_dcomplex__array* csrc = sidl_DoubleComplex__getptr(env, obj);
  struct sidl_dcomplex__array* cdest = sidl_DoubleComplex__getptr(env, dest);
      
  if(csrc && cdest) {
    sidl_dcomplex__array_copy(csrc,cdest);
  } 

}

/*
 * Native function slices arrays in various ways
 */
static jobject sidl_DoubleComplex__slice(
  JNIEnv* env,
  jobject obj, 
  jint dimen, 
  jintArray numElem, 
  jintArray srcStart, 
  jintArray srcStride,
  jintArray newStart)
{
  struct sidl_dcomplex__array* array = sidl_DoubleComplex__getptr(env, obj);
  struct sidl_dcomplex__array* ret_ptr = NULL;
  jobject ret_array = NULL;
  jint* cnumElem = NULL;
  jint* csrcStart = NULL;
  jint* csrcStride = NULL;
  jint* cnewStart = NULL; 
  int i = 0; 

  if(numElem == NULL) {
    return NULL;  /*If numElem is NULL, we need to return Null, that's bad.*/
  } else {
    if((*env)->GetArrayLength(env, numElem) > 7)
      return NULL;  
    cnumElem = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      cnumElem[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, numElem, 0,
			      (*env)->GetArrayLength(env, numElem), cnumElem); 
    
  }

  if(srcStart != NULL) {
    if((*env)->GetArrayLength(env, srcStart) > 7)
      return NULL; 
    csrcStart = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      csrcStart[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, srcStart, 0, 
			    (*env)->GetArrayLength(env, srcStart), csrcStart);   
  } 

  if(srcStride != NULL) {
    if((*env)->GetArrayLength(env, srcStride) > 7)
      return NULL; 
    csrcStride = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      csrcStride[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, srcStride, 0, 
			      (*env)->GetArrayLength(env, srcStride), csrcStride);
  } 

  if(newStart != NULL) {
    if((*env)->GetArrayLength(env, newStart) > 7)
      return NULL; 
    cnewStart = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      cnewStart[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, newStart, 0, 
			      (*env)->GetArrayLength(env, newStart), cnewStart);    
  } 

  if (array != NULL) {
    /* jint should be equivalent to int32_t; Java int is 32 bits */
    ret_ptr = sidl_dcomplex__array_slice(array, dimen, cnumElem, csrcStart, csrcStride, cnewStart);
  }

  if(ret_ptr != NULL) {  
    ret_array = sidl_Java_I2J_new_array(env,ret_ptr, "sidl.DoubleComplex$Array");
  }

   return ret_array;
}

/*
 * Native routine to increment the current array reference count.
 */
static void sidl_DoubleComplex__addRef(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_dcomplex__array* array = sidl_DoubleComplex__getptr(env, obj);
  if (array != NULL) {
    sidl_dcomplex__array_addRef(array);
  }
}

/*
 * Native routine to decrement the current array reference count.
 */
static void sidl_DoubleComplex__deallocate(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_dcomplex__array* array = sidl_DoubleComplex__getptr(env, obj);
  if (array != NULL) {
    sidl_dcomplex__array_deleteRef(array);
  }
  /*sidl_DoubleComplex__setptr(env, obj, NULL);*/
}

/*
 * Native routine to destroy (deallocate) the current array data.
 */
static void sidl_DoubleComplex__destroy(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_dcomplex__array* array = sidl_DoubleComplex__getptr(env, obj);
  if (array != NULL) {
    sidl_dcomplex__array_deleteRef(array);
  }
  sidl_DoubleComplex__setptr(env, obj, NULL);
}

/*
 * Native routine to reallocate data in the array.  The specified array
 * dimension and indices must match and be within valid ranges (e.g., the
 * upper bounds must be greater than or equal to lowe rbounds.  Invalid
 * indices will have unpredictable (but almost certainly bad) results.
 * This routine will deallocate the existing array data if it is not null.
 * if argument "isRow" is true, the array will be in RowOrder, other wise
 * it will be in ColumnOrder   
 */
static void sidl_DoubleComplex__reallocate(
  JNIEnv* env,
  jobject obj,
  jint dim,
  jarray lower,
  jarray upper,
  jboolean isRow)
{
  jint* l = NULL;
  jint* u = NULL;
  struct sidl_dcomplex__array* array = NULL;

  sidl_DoubleComplex__destroy(env, obj);

  l = (*env)->GetIntArrayElements(env, lower, NULL);
  u = (*env)->GetIntArrayElements(env, upper, NULL);
  if(isRow)  
    array = sidl_dcomplex__array_createRow((int32_t) dim, (int32_t*) l,
                                          (int32_t*) u);
  else
    array = sidl_dcomplex__array_createCol((int32_t) dim, (int32_t*) l,
                                          (int32_t*) u);
  (*env)->ReleaseIntArrayElements(env, lower, l, JNI_ABORT);
  (*env)->ReleaseIntArrayElements(env, upper, u, JNI_ABORT);

  sidl_DoubleComplex__setptr(env, obj, array);
}

/*
 * Register JNI array methods with the Java JVM.
 */
void sidl_DoubleComplex__register(JNIEnv* env)
{
  JNINativeMethod methods[15];
  jclass cls;

  methods[0].name      = "_dim";
  methods[0].signature = "()I";
  methods[0].fnPtr     = sidl_DoubleComplex__dim;
  methods[1].name      = "_lower";
  methods[1].signature = "(I)I";
  methods[1].fnPtr     = sidl_DoubleComplex__lower;
  methods[2].name      = "_upper";
  methods[2].signature = "(I)I";
  methods[2].fnPtr     = sidl_DoubleComplex__upper;
  methods[3].name      = "_get";
  methods[3].signature = "(IIIIIII)Lsidl/DoubleComplex;";
  methods[3].fnPtr     = sidl_DoubleComplex__get;
  methods[4].name      = "_set";
  methods[4].signature = "(IIIIIIILsidl/DoubleComplex;)V";
  methods[4].fnPtr     = sidl_DoubleComplex__set;
  methods[5].name      = "_destroy";
  methods[5].signature = "()V";
  methods[5].fnPtr     = sidl_DoubleComplex__destroy;
  methods[6].name      = "_reallocate";
  methods[6].signature = "(I[I[IZ)V";
  methods[6].fnPtr     = sidl_DoubleComplex__reallocate;
  methods[7].name      = "_stride";
  methods[7].signature = "(I)I";
  methods[7].fnPtr     = sidl_DoubleComplex__stride;  
  methods[8].name      = "_isColumnOrder";
  methods[8].signature = "()Z";
  methods[8].fnPtr     = sidl_DoubleComplex__isColumnOrder;       
  methods[9].name      = "_isRowOrder";
  methods[9].signature = "()Z";
  methods[9].fnPtr     = sidl_DoubleComplex__isRowOrder;
  methods[10].name      = "_deallocate";
  methods[10].signature = "()V";
  methods[10].fnPtr     = sidl_DoubleComplex__deallocate;
  methods[11].name      = "_copy";
  methods[11].signature = "(Lsidl/DoubleComplex$Array;)V";
  methods[11].fnPtr     = sidl_DoubleComplex__copy;  
  methods[12].name      = "_smartCopy";
  methods[12].signature = "()Lsidl/DoubleComplex$Array;";
  methods[12].fnPtr     = sidl_DoubleComplex__smartCopy;    
  methods[13].name      = "_slice";
  methods[13].signature = "(I[I[I[I[I)Lsidl/DoubleComplex$Array;";
  methods[13].fnPtr     = sidl_DoubleComplex__slice;    
  methods[14].name      = "_addRef";
  methods[14].signature = "()V";
  methods[14].fnPtr     = sidl_DoubleComplex__addRef;

  cls = (*env)->FindClass(env, "sidl/DoubleComplex$Array");
  if (cls) {
    (*env)->RegisterNatives(env, cls, methods, 15);
    (*env)->DeleteLocalRef(env, cls);
  }
}

/* 
 * Local utility function to extract the array pointer from the Java object.
 * Extract the d_array long data member and convert it to a pointer.
 */
static struct sidl_double__array* sidl_Double__getptr(
  JNIEnv* env,
  jobject obj)
{
  void* ptr = NULL;
  static jfieldID s_array_field = NULL;

  if (s_array_field == NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    s_array_field = (*env)->GetFieldID(env, cls, "d_array", "J");
    (*env)->DeleteLocalRef(env, cls);
  }

  ptr = JLONG_TO_POINTER((*env)->GetLongField(env, obj, s_array_field));
  return (struct sidl_double__array*) ptr;
}

/*
 * Local utility function to set the array pointer on the Java object.
 * Convert the pointer to a long value and set the d_array data member.
 */
static void sidl_Double__setptr(
  JNIEnv* env,
  jobject obj,
  struct sidl_double__array* array)
{
  static jfieldID s_array_field = NULL;

  if (s_array_field == NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    s_array_field = (*env)->GetFieldID(env, cls, "d_array", "J");
    (*env)->DeleteLocalRef(env, cls);
  }

  (*env)->SetLongField(env, obj, s_array_field, POINTER_TO_JLONG(array));
}

/*
 * Native routine to get the dimension of the current array.  This
 * routine assumes that the array has already been initialized.  If
 * the array has not been initialized, then horrible things may happen.
 */
static jint sidl_Double__dim(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_double__array* array = sidl_Double__getptr(env, obj);
  return (jint) sidl_double__array_dimen(array);
}

/*
 * Native routine to fetch the specified lower bound of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_Double__lower(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_double__array* array = sidl_Double__getptr(env, obj);
  return (jint) sidl_double__array_lower(array, dim);
}

/*
 * Native routine to fetch the specified upper bound of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_Double__upper(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_double__array* array = sidl_Double__getptr(env, obj);
  return (jint) sidl_double__array_upper(array, dim);
}

/*
 * Native routine to fetch the specified stride of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_Double__stride(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_double__array* array = sidl_Double__getptr(env, obj);
  return (jint) sidl_double__array_stride(array, dim);
}

/*
 * Native routine to returns true if the array is Column order.  
 */
static jboolean sidl_Double__isColumnOrder(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_double__array* array = sidl_Double__getptr(env, obj);
#if (JNI_TRUE == TRUE) && (JNI_FALSE == FALSE)
  return (jboolean) sidl_double__array_isColumnOrder(array);
#else
  return sidl_double__array_isColumnOrder(array) ? JNI_TRUE : JNI_FALSE;
#endif
}
/*
 * Native routine returns the if array is RowOrder.  
 */
static jint sidl_Double__isRowOrder(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_double__array* array = sidl_Double__getptr(env, obj);
#if (JNI_TRUE == TRUE) && (JNI_FALSE == FALSE)
  return (jboolean) sidl_double__array_isRowOrder(array);
#else
  return sidl_double__array_isRowOrder(array) ? JNI_TRUE : JNI_FALSE;
#endif
}

/*
 * Native routine to fetch the specified value from the array.  The
 * specified array index/indices must be lie between the array lower
 * upper bounds (inclusive).  Invalid indices will have unpredictable
 * (but almost certainly bad) results.
 */
static jdouble sidl_Double__get(
  JNIEnv* env,
  jobject obj,
  jint i,
  jint j,
  jint k,
  jint l,
  jint m,
  jint n,
  jint o)
{
  struct sidl_double__array* array = sidl_Double__getptr(env, obj);
  int32_t a[7];

  a[0] = i; a[1] = j; a[2] = k; a[3] = l; a[4] = m; a[5] = n; a[6] = o;
  return (jdouble) sidl_double__array_get(array, a);
}

/**
 * Native routine to set the specified value in the array.  The
 * specified array index/indices must be lie between the array lower
 * upper bounds (inclusive).  Invalid indices will have unpredictable
 * (but almost certainly bad) results.
 */
static void sidl_Double__set(
  JNIEnv* env,
  jobject obj,
  jint i,
  jint j,
  jint k,
  jint l,
  jint m, 
  jint n, 
  jint o,
  jdouble value)
{
  struct sidl_double__array* array = sidl_Double__getptr(env, obj);
  int32_t a[7];

  a[0] = i; a[1] = j; a[2] = k; a[3] = l; a[4] = m; a[5] = n; a[6] = o;
  sidl_double__array_set(array, a, (double) value);
}


/*
 * Native function copies borrowed arrays, or increments the reference count
 * of non-borrowed arrays.  Good if you want to keep a copy of a passed
 * in array.  Returns an array, new if borrowed.
 */
static jobject sidl_Double__smartCopy(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_double__array* array = sidl_Double__getptr(env, obj);
  struct sidl_double__array* ret_ptr = NULL;
  jobject ret_array = NULL;
  
  if (array != NULL) {
    ret_ptr = sidl_double__array_smartCopy(array);
  }

  /* Assuming our pointer comes back good, crate a new java array with the correct pointer,
     and owner and pass that back */
  if(ret_ptr != NULL) {  
    ret_array = sidl_Java_I2J_new_array(env,ret_ptr, "sidl.Double$Array");
  }
    return ret_array;
}


/*
 * Native function copies borrowed arrays, or increments the reference count
 * of non-borrowed arrays.  Good if you want to keep a copy of a passed
 * in array.  Returns an array, new if borrowed.
 */
static void sidl_Double__copy(
  JNIEnv* env,
  jobject obj,
  jobject dest)
{
  struct sidl_double__array* csrc = sidl_Double__getptr(env, obj);
  struct sidl_double__array* cdest = sidl_Double__getptr(env, dest);
      
  if(csrc && cdest) {
    sidl_double__array_copy(csrc,cdest);
  } 

}

/*
 * Native function slices arrays in various ways
 */
static jobject sidl_Double__slice(
  JNIEnv* env,
  jobject obj, 
  jint dimen, 
  jintArray numElem, 
  jintArray srcStart, 
  jintArray srcStride,
  jintArray newStart)
{
  struct sidl_double__array* array = sidl_Double__getptr(env, obj);
  struct sidl_double__array* ret_ptr = NULL;
  jobject ret_array = NULL;
  jint* cnumElem = NULL;
  jint* csrcStart = NULL;
  jint* csrcStride = NULL;
  jint* cnewStart = NULL; 
  int i = 0; 

  if(numElem == NULL) {
    return NULL;  /*If numElem is NULL, we need to return Null, that's bad.*/
  } else {
    if((*env)->GetArrayLength(env, numElem) > 7)
      return NULL;  
    cnumElem = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      cnumElem[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, numElem, 0,
			      (*env)->GetArrayLength(env, numElem), cnumElem); 
    
  }

  if(srcStart != NULL) {
    if((*env)->GetArrayLength(env, srcStart) > 7)
      return NULL; 
    csrcStart = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      csrcStart[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, srcStart, 0, 
			    (*env)->GetArrayLength(env, srcStart), csrcStart);   
  } 

  if(srcStride != NULL) {
    if((*env)->GetArrayLength(env, srcStride) > 7)
      return NULL; 
    csrcStride = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      csrcStride[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, srcStride, 0, 
			      (*env)->GetArrayLength(env, srcStride), csrcStride);
  } 

  if(newStart != NULL) {
    if((*env)->GetArrayLength(env, newStart) > 7)
      return NULL; 
    cnewStart = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      cnewStart[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, newStart, 0, 
			      (*env)->GetArrayLength(env, newStart), cnewStart);    
  } 

  if (array != NULL) {
    /* jint should be equivalent to int32_t; Java int is 32 bits */
    ret_ptr = sidl_double__array_slice(array, dimen, cnumElem, csrcStart, csrcStride, cnewStart);
  }

  if(ret_ptr != NULL) {  
    ret_array = sidl_Java_I2J_new_array(env,ret_ptr, "sidl.Double$Array");
  }

   return ret_array;
}

/*
 * Native routine to increment the current array reference count.
 */
static void sidl_Double__addRef(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_double__array* array = sidl_Double__getptr(env, obj);
  if (array != NULL) {
    sidl_double__array_addRef(array);
  }
}

/*
 * Native routine to decrement the current array reference count.
 */
static void sidl_Double__deallocate(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_double__array* array = sidl_Double__getptr(env, obj);
  if (array != NULL) {
    sidl_double__array_deleteRef(array);
  }
  /*sidl_Double__setptr(env, obj, NULL);*/
}

/*
 * Native routine to destroy (deallocate) the current array data.
 */
static void sidl_Double__destroy(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_double__array* array = sidl_Double__getptr(env, obj);
  if (array != NULL) {
    sidl_double__array_deleteRef(array);
  }
  sidl_Double__setptr(env, obj, NULL);
}

/*
 * Native routine to reallocate data in the array.  The specified array
 * dimension and indices must match and be within valid ranges (e.g., the
 * upper bounds must be greater than or equal to lowe rbounds.  Invalid
 * indices will have unpredictable (but almost certainly bad) results.
 * This routine will deallocate the existing array data if it is not null.
 * if argument "isRow" is true, the array will be in RowOrder, other wise
 * it will be in ColumnOrder   
 */
static void sidl_Double__reallocate(
  JNIEnv* env,
  jobject obj,
  jint dim,
  jarray lower,
  jarray upper,
  jboolean isRow)
{
  jint* l = NULL;
  jint* u = NULL;
  struct sidl_double__array* array = NULL;

  sidl_Double__destroy(env, obj);

  l = (*env)->GetIntArrayElements(env, lower, NULL);
  u = (*env)->GetIntArrayElements(env, upper, NULL);
  if(isRow)  
    array = sidl_double__array_createRow((int32_t) dim, (int32_t*) l,
                                          (int32_t*) u);
  else
    array = sidl_double__array_createCol((int32_t) dim, (int32_t*) l,
                                          (int32_t*) u);
  (*env)->ReleaseIntArrayElements(env, lower, l, JNI_ABORT);
  (*env)->ReleaseIntArrayElements(env, upper, u, JNI_ABORT);

  sidl_Double__setptr(env, obj, array);
}

/*
 * Register JNI array methods with the Java JVM.
 */
void sidl_Double__register(JNIEnv* env)
{
  JNINativeMethod methods[15];
  jclass cls;

  methods[0].name      = "_dim";
  methods[0].signature = "()I";
  methods[0].fnPtr     = sidl_Double__dim;
  methods[1].name      = "_lower";
  methods[1].signature = "(I)I";
  methods[1].fnPtr     = sidl_Double__lower;
  methods[2].name      = "_upper";
  methods[2].signature = "(I)I";
  methods[2].fnPtr     = sidl_Double__upper;
  methods[3].name      = "_get";
  methods[3].signature = "(IIIIIII)D";
  methods[3].fnPtr     = sidl_Double__get;
  methods[4].name      = "_set";
  methods[4].signature = "(IIIIIIID)V";
  methods[4].fnPtr     = sidl_Double__set;
  methods[5].name      = "_destroy";
  methods[5].signature = "()V";
  methods[5].fnPtr     = sidl_Double__destroy;
  methods[6].name      = "_reallocate";
  methods[6].signature = "(I[I[IZ)V";
  methods[6].fnPtr     = sidl_Double__reallocate;
  methods[7].name      = "_stride";
  methods[7].signature = "(I)I";
  methods[7].fnPtr     = sidl_Double__stride;  
  methods[8].name      = "_isColumnOrder";
  methods[8].signature = "()Z";
  methods[8].fnPtr     = sidl_Double__isColumnOrder;       
  methods[9].name      = "_isRowOrder";
  methods[9].signature = "()Z";
  methods[9].fnPtr     = sidl_Double__isRowOrder;
  methods[10].name      = "_deallocate";
  methods[10].signature = "()V";
  methods[10].fnPtr     = sidl_Double__deallocate;
  methods[11].name      = "_copy";
  methods[11].signature = "(Lsidl/Double$Array;)V";
  methods[11].fnPtr     = sidl_Double__copy;  
  methods[12].name      = "_smartCopy";
  methods[12].signature = "()Lsidl/Double$Array;";
  methods[12].fnPtr     = sidl_Double__smartCopy;    
  methods[13].name      = "_slice";
  methods[13].signature = "(I[I[I[I[I)Lsidl/Double$Array;";
  methods[13].fnPtr     = sidl_Double__slice;    
  methods[14].name      = "_addRef";
  methods[14].signature = "()V";
  methods[14].fnPtr     = sidl_Double__addRef;

  cls = (*env)->FindClass(env, "sidl/Double$Array");
  if (cls) {
    (*env)->RegisterNatives(env, cls, methods, 15);
    (*env)->DeleteLocalRef(env, cls);
  }
}

/* 
 * Local utility function to extract the array pointer from the Java object.
 * Extract the d_array long data member and convert it to a pointer.
 */
static struct sidl_fcomplex__array* sidl_FloatComplex__getptr(
  JNIEnv* env,
  jobject obj)
{
  void* ptr = NULL;
  static jfieldID s_array_field = NULL;

  if (s_array_field == NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    s_array_field = (*env)->GetFieldID(env, cls, "d_array", "J");
    (*env)->DeleteLocalRef(env, cls);
  }

  ptr = JLONG_TO_POINTER((*env)->GetLongField(env, obj, s_array_field));
  return (struct sidl_fcomplex__array*) ptr;
}

/*
 * Local utility function to set the array pointer on the Java object.
 * Convert the pointer to a long value and set the d_array data member.
 */
static void sidl_FloatComplex__setptr(
  JNIEnv* env,
  jobject obj,
  struct sidl_fcomplex__array* array)
{
  static jfieldID s_array_field = NULL;

  if (s_array_field == NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    s_array_field = (*env)->GetFieldID(env, cls, "d_array", "J");
    (*env)->DeleteLocalRef(env, cls);
  }

  (*env)->SetLongField(env, obj, s_array_field, POINTER_TO_JLONG(array));
}

/*
 * Native routine to get the dimension of the current array.  This
 * routine assumes that the array has already been initialized.  If
 * the array has not been initialized, then horrible things may happen.
 */
static jint sidl_FloatComplex__dim(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_fcomplex__array* array = sidl_FloatComplex__getptr(env, obj);
  return (jint) sidl_fcomplex__array_dimen(array);
}

/*
 * Native routine to fetch the specified lower bound of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_FloatComplex__lower(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_fcomplex__array* array = sidl_FloatComplex__getptr(env, obj);
  return (jint) sidl_fcomplex__array_lower(array, dim);
}

/*
 * Native routine to fetch the specified upper bound of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_FloatComplex__upper(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_fcomplex__array* array = sidl_FloatComplex__getptr(env, obj);
  return (jint) sidl_fcomplex__array_upper(array, dim);
}

/*
 * Native routine to fetch the specified stride of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_FloatComplex__stride(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_fcomplex__array* array = sidl_FloatComplex__getptr(env, obj);
  return (jint) sidl_fcomplex__array_stride(array, dim);
}

/*
 * Native routine to returns true if the array is Column order.  
 */
static jboolean sidl_FloatComplex__isColumnOrder(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_fcomplex__array* array = sidl_FloatComplex__getptr(env, obj);
#if (JNI_TRUE == TRUE) && (JNI_FALSE == FALSE)
  return (jboolean) sidl_fcomplex__array_isColumnOrder(array);
#else
  return sidl_fcomplex__array_isColumnOrder(array) ? JNI_TRUE : JNI_FALSE;
#endif
}
/*
 * Native routine returns the if array is RowOrder.  
 */
static jint sidl_FloatComplex__isRowOrder(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_fcomplex__array* array = sidl_FloatComplex__getptr(env, obj);
#if (JNI_TRUE == TRUE) && (JNI_FALSE == FALSE)
  return (jboolean) sidl_fcomplex__array_isRowOrder(array);
#else
  return sidl_fcomplex__array_isRowOrder(array) ? JNI_TRUE : JNI_FALSE;
#endif
}

/*
 * Native routine to fetch the specified value from the array.  The
 * specified array index/indices must be lie between the array lower
 * upper bounds (inclusive).  Invalid indices will have unpredictable
 * (but almost certainly bad) results.
 */
static jobject sidl_FloatComplex__get(
  JNIEnv* env,
  jobject obj,
  jint i,
  jint j,
  jint k,
  jint l,
  jint m,
  jint n,
  jint o)
{
  struct sidl_fcomplex__array* array = sidl_FloatComplex__getptr(env, obj);
  int32_t a[7];
  struct sidl_fcomplex value;

  a[0] = i; a[1] = j; a[2] = k; a[3] = l; a[4] = m; a[5] = n; a[6] = o;
  value = sidl_fcomplex__array_get(array, a);
  return sidl_Java_I2J_fcomplex(env, &value);
}

/**
 * Native routine to set the specified value in the array.  The
 * specified array index/indices must be lie between the array lower
 * upper bounds (inclusive).  Invalid indices will have unpredictable
 * (but almost certainly bad) results.
 */
static void sidl_FloatComplex__set(
  JNIEnv* env,
  jobject obj,
  jint i,
  jint j,
  jint k,
  jint l,
  jint m, 
  jint n, 
  jint o,
  jobject value)
{
  struct sidl_fcomplex__array* array = sidl_FloatComplex__getptr(env, obj);
  int32_t a[7];
  struct sidl_fcomplex elem;

  a[0] = i; a[1] = j; a[2] = k; a[3] = l; a[4] = m; a[5] = n; a[6] = o;
  elem = sidl_Java_J2I_fcomplex(env, value);
  sidl_fcomplex__array_set(array, a, elem);
}


/*
 * Native function copies borrowed arrays, or increments the reference count
 * of non-borrowed arrays.  Good if you want to keep a copy of a passed
 * in array.  Returns an array, new if borrowed.
 */
static jobject sidl_FloatComplex__smartCopy(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_fcomplex__array* array = sidl_FloatComplex__getptr(env, obj);
  struct sidl_fcomplex__array* ret_ptr = NULL;
  jobject ret_array = NULL;
  
  if (array != NULL) {
    ret_ptr = sidl_fcomplex__array_smartCopy(array);
  }

  /* Assuming our pointer comes back good, crate a new java array with the correct pointer,
     and owner and pass that back */
  if(ret_ptr != NULL) {  
    ret_array = sidl_Java_I2J_new_array(env,ret_ptr, "sidl.FloatComplex$Array");
  }
    return ret_array;
}


/*
 * Native function copies borrowed arrays, or increments the reference count
 * of non-borrowed arrays.  Good if you want to keep a copy of a passed
 * in array.  Returns an array, new if borrowed.
 */
static void sidl_FloatComplex__copy(
  JNIEnv* env,
  jobject obj,
  jobject dest)
{
  struct sidl_fcomplex__array* csrc = sidl_FloatComplex__getptr(env, obj);
  struct sidl_fcomplex__array* cdest = sidl_FloatComplex__getptr(env, dest);
      
  if(csrc && cdest) {
    sidl_fcomplex__array_copy(csrc,cdest);
  } 

}

/*
 * Native function slices arrays in various ways
 */
static jobject sidl_FloatComplex__slice(
  JNIEnv* env,
  jobject obj, 
  jint dimen, 
  jintArray numElem, 
  jintArray srcStart, 
  jintArray srcStride,
  jintArray newStart)
{
  struct sidl_fcomplex__array* array = sidl_FloatComplex__getptr(env, obj);
  struct sidl_fcomplex__array* ret_ptr = NULL;
  jobject ret_array = NULL;
  jint* cnumElem = NULL;
  jint* csrcStart = NULL;
  jint* csrcStride = NULL;
  jint* cnewStart = NULL; 
  int i = 0; 

  if(numElem == NULL) {
    return NULL;  /*If numElem is NULL, we need to return Null, that's bad.*/
  } else {
    if((*env)->GetArrayLength(env, numElem) > 7)
      return NULL;  
    cnumElem = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      cnumElem[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, numElem, 0,
			      (*env)->GetArrayLength(env, numElem), cnumElem); 
    
  }

  if(srcStart != NULL) {
    if((*env)->GetArrayLength(env, srcStart) > 7)
      return NULL; 
    csrcStart = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      csrcStart[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, srcStart, 0, 
			    (*env)->GetArrayLength(env, srcStart), csrcStart);   
  } 

  if(srcStride != NULL) {
    if((*env)->GetArrayLength(env, srcStride) > 7)
      return NULL; 
    csrcStride = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      csrcStride[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, srcStride, 0, 
			      (*env)->GetArrayLength(env, srcStride), csrcStride);
  } 

  if(newStart != NULL) {
    if((*env)->GetArrayLength(env, newStart) > 7)
      return NULL; 
    cnewStart = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      cnewStart[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, newStart, 0, 
			      (*env)->GetArrayLength(env, newStart), cnewStart);    
  } 

  if (array != NULL) {
    /* jint should be equivalent to int32_t; Java int is 32 bits */
    ret_ptr = sidl_fcomplex__array_slice(array, dimen, cnumElem, csrcStart, csrcStride, cnewStart);
  }

  if(ret_ptr != NULL) {  
    ret_array = sidl_Java_I2J_new_array(env,ret_ptr, "sidl.FloatComplex$Array");
  }

   return ret_array;
}

/*
 * Native routine to increment the current array reference count.
 */
static void sidl_FloatComplex__addRef(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_fcomplex__array* array = sidl_FloatComplex__getptr(env, obj);
  if (array != NULL) {
    sidl_fcomplex__array_addRef(array);
  }
}

/*
 * Native routine to decrement the current array reference count.
 */
static void sidl_FloatComplex__deallocate(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_fcomplex__array* array = sidl_FloatComplex__getptr(env, obj);
  if (array != NULL) {
    sidl_fcomplex__array_deleteRef(array);
  }
  /*sidl_FloatComplex__setptr(env, obj, NULL);*/
}

/*
 * Native routine to destroy (deallocate) the current array data.
 */
static void sidl_FloatComplex__destroy(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_fcomplex__array* array = sidl_FloatComplex__getptr(env, obj);
  if (array != NULL) {
    sidl_fcomplex__array_deleteRef(array);
  }
  sidl_FloatComplex__setptr(env, obj, NULL);
}

/*
 * Native routine to reallocate data in the array.  The specified array
 * dimension and indices must match and be within valid ranges (e.g., the
 * upper bounds must be greater than or equal to lowe rbounds.  Invalid
 * indices will have unpredictable (but almost certainly bad) results.
 * This routine will deallocate the existing array data if it is not null.
 * if argument "isRow" is true, the array will be in RowOrder, other wise
 * it will be in ColumnOrder   
 */
static void sidl_FloatComplex__reallocate(
  JNIEnv* env,
  jobject obj,
  jint dim,
  jarray lower,
  jarray upper,
  jboolean isRow)
{
  jint* l = NULL;
  jint* u = NULL;
  struct sidl_fcomplex__array* array = NULL;

  sidl_FloatComplex__destroy(env, obj);

  l = (*env)->GetIntArrayElements(env, lower, NULL);
  u = (*env)->GetIntArrayElements(env, upper, NULL);
  if(isRow)  
    array = sidl_fcomplex__array_createRow((int32_t) dim, (int32_t*) l,
                                          (int32_t*) u);
  else
    array = sidl_fcomplex__array_createCol((int32_t) dim, (int32_t*) l,
                                          (int32_t*) u);
  (*env)->ReleaseIntArrayElements(env, lower, l, JNI_ABORT);
  (*env)->ReleaseIntArrayElements(env, upper, u, JNI_ABORT);

  sidl_FloatComplex__setptr(env, obj, array);
}

/*
 * Register JNI array methods with the Java JVM.
 */
void sidl_FloatComplex__register(JNIEnv* env)
{
  JNINativeMethod methods[15];
  jclass cls;

  methods[0].name      = "_dim";
  methods[0].signature = "()I";
  methods[0].fnPtr     = sidl_FloatComplex__dim;
  methods[1].name      = "_lower";
  methods[1].signature = "(I)I";
  methods[1].fnPtr     = sidl_FloatComplex__lower;
  methods[2].name      = "_upper";
  methods[2].signature = "(I)I";
  methods[2].fnPtr     = sidl_FloatComplex__upper;
  methods[3].name      = "_get";
  methods[3].signature = "(IIIIIII)Lsidl/FloatComplex;";
  methods[3].fnPtr     = sidl_FloatComplex__get;
  methods[4].name      = "_set";
  methods[4].signature = "(IIIIIIILsidl/FloatComplex;)V";
  methods[4].fnPtr     = sidl_FloatComplex__set;
  methods[5].name      = "_destroy";
  methods[5].signature = "()V";
  methods[5].fnPtr     = sidl_FloatComplex__destroy;
  methods[6].name      = "_reallocate";
  methods[6].signature = "(I[I[IZ)V";
  methods[6].fnPtr     = sidl_FloatComplex__reallocate;
  methods[7].name      = "_stride";
  methods[7].signature = "(I)I";
  methods[7].fnPtr     = sidl_FloatComplex__stride;  
  methods[8].name      = "_isColumnOrder";
  methods[8].signature = "()Z";
  methods[8].fnPtr     = sidl_FloatComplex__isColumnOrder;       
  methods[9].name      = "_isRowOrder";
  methods[9].signature = "()Z";
  methods[9].fnPtr     = sidl_FloatComplex__isRowOrder;
  methods[10].name      = "_deallocate";
  methods[10].signature = "()V";
  methods[10].fnPtr     = sidl_FloatComplex__deallocate;
  methods[11].name      = "_copy";
  methods[11].signature = "(Lsidl/FloatComplex$Array;)V";
  methods[11].fnPtr     = sidl_FloatComplex__copy;  
  methods[12].name      = "_smartCopy";
  methods[12].signature = "()Lsidl/FloatComplex$Array;";
  methods[12].fnPtr     = sidl_FloatComplex__smartCopy;    
  methods[13].name      = "_slice";
  methods[13].signature = "(I[I[I[I[I)Lsidl/FloatComplex$Array;";
  methods[13].fnPtr     = sidl_FloatComplex__slice;    
  methods[14].name      = "_addRef";
  methods[14].signature = "()V";
  methods[14].fnPtr     = sidl_FloatComplex__addRef;

  cls = (*env)->FindClass(env, "sidl/FloatComplex$Array");
  if (cls) {
    (*env)->RegisterNatives(env, cls, methods, 15);
    (*env)->DeleteLocalRef(env, cls);
  }
}

/* 
 * Local utility function to extract the array pointer from the Java object.
 * Extract the d_array long data member and convert it to a pointer.
 */
static struct sidl_float__array* sidl_Float__getptr(
  JNIEnv* env,
  jobject obj)
{
  void* ptr = NULL;
  static jfieldID s_array_field = NULL;

  if (s_array_field == NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    s_array_field = (*env)->GetFieldID(env, cls, "d_array", "J");
    (*env)->DeleteLocalRef(env, cls);
  }

  ptr = JLONG_TO_POINTER((*env)->GetLongField(env, obj, s_array_field));
  return (struct sidl_float__array*) ptr;
}

/*
 * Local utility function to set the array pointer on the Java object.
 * Convert the pointer to a long value and set the d_array data member.
 */
static void sidl_Float__setptr(
  JNIEnv* env,
  jobject obj,
  struct sidl_float__array* array)
{
  static jfieldID s_array_field = NULL;

  if (s_array_field == NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    s_array_field = (*env)->GetFieldID(env, cls, "d_array", "J");
    (*env)->DeleteLocalRef(env, cls);
  }

  (*env)->SetLongField(env, obj, s_array_field, POINTER_TO_JLONG(array));
}

/*
 * Native routine to get the dimension of the current array.  This
 * routine assumes that the array has already been initialized.  If
 * the array has not been initialized, then horrible things may happen.
 */
static jint sidl_Float__dim(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_float__array* array = sidl_Float__getptr(env, obj);
  return (jint) sidl_float__array_dimen(array);
}

/*
 * Native routine to fetch the specified lower bound of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_Float__lower(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_float__array* array = sidl_Float__getptr(env, obj);
  return (jint) sidl_float__array_lower(array, dim);
}

/*
 * Native routine to fetch the specified upper bound of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_Float__upper(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_float__array* array = sidl_Float__getptr(env, obj);
  return (jint) sidl_float__array_upper(array, dim);
}

/*
 * Native routine to fetch the specified stride of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_Float__stride(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_float__array* array = sidl_Float__getptr(env, obj);
  return (jint) sidl_float__array_stride(array, dim);
}

/*
 * Native routine to returns true if the array is Column order.  
 */
static jboolean sidl_Float__isColumnOrder(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_float__array* array = sidl_Float__getptr(env, obj);
#if (JNI_TRUE == TRUE) && (JNI_FALSE == FALSE)
  return (jboolean) sidl_float__array_isColumnOrder(array);
#else
  return sidl_float__array_isColumnOrder(array) ? JNI_TRUE : JNI_FALSE;
#endif
}
/*
 * Native routine returns the if array is RowOrder.  
 */
static jint sidl_Float__isRowOrder(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_float__array* array = sidl_Float__getptr(env, obj);
#if (JNI_TRUE == TRUE) && (JNI_FALSE == FALSE)
  return (jboolean) sidl_float__array_isRowOrder(array);
#else
  return sidl_float__array_isRowOrder(array) ? JNI_TRUE : JNI_FALSE;
#endif
}

/*
 * Native routine to fetch the specified value from the array.  The
 * specified array index/indices must be lie between the array lower
 * upper bounds (inclusive).  Invalid indices will have unpredictable
 * (but almost certainly bad) results.
 */
static jfloat sidl_Float__get(
  JNIEnv* env,
  jobject obj,
  jint i,
  jint j,
  jint k,
  jint l,
  jint m,
  jint n,
  jint o)
{
  struct sidl_float__array* array = sidl_Float__getptr(env, obj);
  int32_t a[7];

  a[0] = i; a[1] = j; a[2] = k; a[3] = l; a[4] = m; a[5] = n; a[6] = o;
  return (jfloat) sidl_float__array_get(array, a);
}

/**
 * Native routine to set the specified value in the array.  The
 * specified array index/indices must be lie between the array lower
 * upper bounds (inclusive).  Invalid indices will have unpredictable
 * (but almost certainly bad) results.
 */
static void sidl_Float__set(
  JNIEnv* env,
  jobject obj,
  jint i,
  jint j,
  jint k,
  jint l,
  jint m, 
  jint n, 
  jint o,
  jfloat value)
{
  struct sidl_float__array* array = sidl_Float__getptr(env, obj);
  int32_t a[7];

  a[0] = i; a[1] = j; a[2] = k; a[3] = l; a[4] = m; a[5] = n; a[6] = o;
  sidl_float__array_set(array, a, (float) value);
}


/*
 * Native function copies borrowed arrays, or increments the reference count
 * of non-borrowed arrays.  Good if you want to keep a copy of a passed
 * in array.  Returns an array, new if borrowed.
 */
static jobject sidl_Float__smartCopy(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_float__array* array = sidl_Float__getptr(env, obj);
  struct sidl_float__array* ret_ptr = NULL;
  jobject ret_array = NULL;
  
  if (array != NULL) {
    ret_ptr = sidl_float__array_smartCopy(array);
  }

  /* Assuming our pointer comes back good, crate a new java array with the correct pointer,
     and owner and pass that back */
  if(ret_ptr != NULL) {  
    ret_array = sidl_Java_I2J_new_array(env,ret_ptr, "sidl.Float$Array");
  }
    return ret_array;
}


/*
 * Native function copies borrowed arrays, or increments the reference count
 * of non-borrowed arrays.  Good if you want to keep a copy of a passed
 * in array.  Returns an array, new if borrowed.
 */
static void sidl_Float__copy(
  JNIEnv* env,
  jobject obj,
  jobject dest)
{
  struct sidl_float__array* csrc = sidl_Float__getptr(env, obj);
  struct sidl_float__array* cdest = sidl_Float__getptr(env, dest);
      
  if(csrc && cdest) {
    sidl_float__array_copy(csrc,cdest);
  } 

}

/*
 * Native function slices arrays in various ways
 */
static jobject sidl_Float__slice(
  JNIEnv* env,
  jobject obj, 
  jint dimen, 
  jintArray numElem, 
  jintArray srcStart, 
  jintArray srcStride,
  jintArray newStart)
{
  struct sidl_float__array* array = sidl_Float__getptr(env, obj);
  struct sidl_float__array* ret_ptr = NULL;
  jobject ret_array = NULL;
  jint* cnumElem = NULL;
  jint* csrcStart = NULL;
  jint* csrcStride = NULL;
  jint* cnewStart = NULL; 
  int i = 0; 

  if(numElem == NULL) {
    return NULL;  /*If numElem is NULL, we need to return Null, that's bad.*/
  } else {
    if((*env)->GetArrayLength(env, numElem) > 7)
      return NULL;  
    cnumElem = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      cnumElem[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, numElem, 0,
			      (*env)->GetArrayLength(env, numElem), cnumElem); 
    
  }

  if(srcStart != NULL) {
    if((*env)->GetArrayLength(env, srcStart) > 7)
      return NULL; 
    csrcStart = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      csrcStart[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, srcStart, 0, 
			    (*env)->GetArrayLength(env, srcStart), csrcStart);   
  } 

  if(srcStride != NULL) {
    if((*env)->GetArrayLength(env, srcStride) > 7)
      return NULL; 
    csrcStride = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      csrcStride[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, srcStride, 0, 
			      (*env)->GetArrayLength(env, srcStride), csrcStride);
  } 

  if(newStart != NULL) {
    if((*env)->GetArrayLength(env, newStart) > 7)
      return NULL; 
    cnewStart = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      cnewStart[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, newStart, 0, 
			      (*env)->GetArrayLength(env, newStart), cnewStart);    
  } 

  if (array != NULL) {
    /* jint should be equivalent to int32_t; Java int is 32 bits */
    ret_ptr = sidl_float__array_slice(array, dimen, cnumElem, csrcStart, csrcStride, cnewStart);
  }

  if(ret_ptr != NULL) {  
    ret_array = sidl_Java_I2J_new_array(env,ret_ptr, "sidl.Float$Array");
  }

   return ret_array;
}

/*
 * Native routine to increment the current array reference count.
 */
static void sidl_Float__addRef(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_float__array* array = sidl_Float__getptr(env, obj);
  if (array != NULL) {
    sidl_float__array_addRef(array);
  }
}

/*
 * Native routine to decrement the current array reference count.
 */
static void sidl_Float__deallocate(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_float__array* array = sidl_Float__getptr(env, obj);
  if (array != NULL) {
    sidl_float__array_deleteRef(array);
  }
  /*sidl_Float__setptr(env, obj, NULL);*/
}

/*
 * Native routine to destroy (deallocate) the current array data.
 */
static void sidl_Float__destroy(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_float__array* array = sidl_Float__getptr(env, obj);
  if (array != NULL) {
    sidl_float__array_deleteRef(array);
  }
  sidl_Float__setptr(env, obj, NULL);
}

/*
 * Native routine to reallocate data in the array.  The specified array
 * dimension and indices must match and be within valid ranges (e.g., the
 * upper bounds must be greater than or equal to lowe rbounds.  Invalid
 * indices will have unpredictable (but almost certainly bad) results.
 * This routine will deallocate the existing array data if it is not null.
 * if argument "isRow" is true, the array will be in RowOrder, other wise
 * it will be in ColumnOrder   
 */
static void sidl_Float__reallocate(
  JNIEnv* env,
  jobject obj,
  jint dim,
  jarray lower,
  jarray upper,
  jboolean isRow)
{
  jint* l = NULL;
  jint* u = NULL;
  struct sidl_float__array* array = NULL;

  sidl_Float__destroy(env, obj);

  l = (*env)->GetIntArrayElements(env, lower, NULL);
  u = (*env)->GetIntArrayElements(env, upper, NULL);
  if(isRow)  
    array = sidl_float__array_createRow((int32_t) dim, (int32_t*) l,
                                          (int32_t*) u);
  else
    array = sidl_float__array_createCol((int32_t) dim, (int32_t*) l,
                                          (int32_t*) u);
  (*env)->ReleaseIntArrayElements(env, lower, l, JNI_ABORT);
  (*env)->ReleaseIntArrayElements(env, upper, u, JNI_ABORT);

  sidl_Float__setptr(env, obj, array);
}

/*
 * Register JNI array methods with the Java JVM.
 */
void sidl_Float__register(JNIEnv* env)
{
  JNINativeMethod methods[15];
  jclass cls;

  methods[0].name      = "_dim";
  methods[0].signature = "()I";
  methods[0].fnPtr     = sidl_Float__dim;
  methods[1].name      = "_lower";
  methods[1].signature = "(I)I";
  methods[1].fnPtr     = sidl_Float__lower;
  methods[2].name      = "_upper";
  methods[2].signature = "(I)I";
  methods[2].fnPtr     = sidl_Float__upper;
  methods[3].name      = "_get";
  methods[3].signature = "(IIIIIII)F";
  methods[3].fnPtr     = sidl_Float__get;
  methods[4].name      = "_set";
  methods[4].signature = "(IIIIIIIF)V";
  methods[4].fnPtr     = sidl_Float__set;
  methods[5].name      = "_destroy";
  methods[5].signature = "()V";
  methods[5].fnPtr     = sidl_Float__destroy;
  methods[6].name      = "_reallocate";
  methods[6].signature = "(I[I[IZ)V";
  methods[6].fnPtr     = sidl_Float__reallocate;
  methods[7].name      = "_stride";
  methods[7].signature = "(I)I";
  methods[7].fnPtr     = sidl_Float__stride;  
  methods[8].name      = "_isColumnOrder";
  methods[8].signature = "()Z";
  methods[8].fnPtr     = sidl_Float__isColumnOrder;       
  methods[9].name      = "_isRowOrder";
  methods[9].signature = "()Z";
  methods[9].fnPtr     = sidl_Float__isRowOrder;
  methods[10].name      = "_deallocate";
  methods[10].signature = "()V";
  methods[10].fnPtr     = sidl_Float__deallocate;
  methods[11].name      = "_copy";
  methods[11].signature = "(Lsidl/Float$Array;)V";
  methods[11].fnPtr     = sidl_Float__copy;  
  methods[12].name      = "_smartCopy";
  methods[12].signature = "()Lsidl/Float$Array;";
  methods[12].fnPtr     = sidl_Float__smartCopy;    
  methods[13].name      = "_slice";
  methods[13].signature = "(I[I[I[I[I)Lsidl/Float$Array;";
  methods[13].fnPtr     = sidl_Float__slice;    
  methods[14].name      = "_addRef";
  methods[14].signature = "()V";
  methods[14].fnPtr     = sidl_Float__addRef;

  cls = (*env)->FindClass(env, "sidl/Float$Array");
  if (cls) {
    (*env)->RegisterNatives(env, cls, methods, 15);
    (*env)->DeleteLocalRef(env, cls);
  }
}

/* 
 * Local utility function to extract the array pointer from the Java object.
 * Extract the d_array long data member and convert it to a pointer.
 */
static struct sidl_int__array* sidl_Integer__getptr(
  JNIEnv* env,
  jobject obj)
{
  void* ptr = NULL;
  static jfieldID s_array_field = NULL;

  if (s_array_field == NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    s_array_field = (*env)->GetFieldID(env, cls, "d_array", "J");
    (*env)->DeleteLocalRef(env, cls);
  }

  ptr = JLONG_TO_POINTER((*env)->GetLongField(env, obj, s_array_field));
  return (struct sidl_int__array*) ptr;
}

/*
 * Local utility function to set the array pointer on the Java object.
 * Convert the pointer to a long value and set the d_array data member.
 */
static void sidl_Integer__setptr(
  JNIEnv* env,
  jobject obj,
  struct sidl_int__array* array)
{
  static jfieldID s_array_field = NULL;

  if (s_array_field == NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    s_array_field = (*env)->GetFieldID(env, cls, "d_array", "J");
    (*env)->DeleteLocalRef(env, cls);
  }

  (*env)->SetLongField(env, obj, s_array_field, POINTER_TO_JLONG(array));
}

/*
 * Native routine to get the dimension of the current array.  This
 * routine assumes that the array has already been initialized.  If
 * the array has not been initialized, then horrible things may happen.
 */
static jint sidl_Integer__dim(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_int__array* array = sidl_Integer__getptr(env, obj);
  return (jint) sidl_int__array_dimen(array);
}

/*
 * Native routine to fetch the specified lower bound of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_Integer__lower(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_int__array* array = sidl_Integer__getptr(env, obj);
  return (jint) sidl_int__array_lower(array, dim);
}

/*
 * Native routine to fetch the specified upper bound of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_Integer__upper(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_int__array* array = sidl_Integer__getptr(env, obj);
  return (jint) sidl_int__array_upper(array, dim);
}

/*
 * Native routine to fetch the specified stride of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_Integer__stride(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_int__array* array = sidl_Integer__getptr(env, obj);
  return (jint) sidl_int__array_stride(array, dim);
}

/*
 * Native routine to returns true if the array is Column order.  
 */
static jboolean sidl_Integer__isColumnOrder(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_int__array* array = sidl_Integer__getptr(env, obj);
#if (JNI_TRUE == TRUE) && (JNI_FALSE == FALSE)
  return (jboolean) sidl_int__array_isColumnOrder(array);
#else
  return sidl_int__array_isColumnOrder(array) ? JNI_TRUE : JNI_FALSE;
#endif
}
/*
 * Native routine returns the if array is RowOrder.  
 */
static jint sidl_Integer__isRowOrder(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_int__array* array = sidl_Integer__getptr(env, obj);
#if (JNI_TRUE == TRUE) && (JNI_FALSE == FALSE)
  return (jboolean) sidl_int__array_isRowOrder(array);
#else
  return sidl_int__array_isRowOrder(array) ? JNI_TRUE : JNI_FALSE;
#endif
}

/*
 * Native routine to fetch the specified value from the array.  The
 * specified array index/indices must be lie between the array lower
 * upper bounds (inclusive).  Invalid indices will have unpredictable
 * (but almost certainly bad) results.
 */
static jint sidl_Integer__get(
  JNIEnv* env,
  jobject obj,
  jint i,
  jint j,
  jint k,
  jint l,
  jint m,
  jint n,
  jint o)
{
  struct sidl_int__array* array = sidl_Integer__getptr(env, obj);
  int32_t a[7];

  a[0] = i; a[1] = j; a[2] = k; a[3] = l; a[4] = m; a[5] = n; a[6] = o;
  return (jint) sidl_int__array_get(array, a);
}

/**
 * Native routine to set the specified value in the array.  The
 * specified array index/indices must be lie between the array lower
 * upper bounds (inclusive).  Invalid indices will have unpredictable
 * (but almost certainly bad) results.
 */
static void sidl_Integer__set(
  JNIEnv* env,
  jobject obj,
  jint i,
  jint j,
  jint k,
  jint l,
  jint m, 
  jint n, 
  jint o,
  jint value)
{
  struct sidl_int__array* array = sidl_Integer__getptr(env, obj);
  int32_t a[7];

  a[0] = i; a[1] = j; a[2] = k; a[3] = l; a[4] = m; a[5] = n; a[6] = o;
  sidl_int__array_set(array, a, (int32_t) value);
}


/*
 * Native function copies borrowed arrays, or increments the reference count
 * of non-borrowed arrays.  Good if you want to keep a copy of a passed
 * in array.  Returns an array, new if borrowed.
 */
static jobject sidl_Integer__smartCopy(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_int__array* array = sidl_Integer__getptr(env, obj);
  struct sidl_int__array* ret_ptr = NULL;
  jobject ret_array = NULL;
  
  if (array != NULL) {
    ret_ptr = sidl_int__array_smartCopy(array);
  }

  /* Assuming our pointer comes back good, crate a new java array with the correct pointer,
     and owner and pass that back */
  if(ret_ptr != NULL) {  
    ret_array = sidl_Java_I2J_new_array(env,ret_ptr, "sidl.Integer$Array");
  }
    return ret_array;
}


/*
 * Native function copies borrowed arrays, or increments the reference count
 * of non-borrowed arrays.  Good if you want to keep a copy of a passed
 * in array.  Returns an array, new if borrowed.
 */
static void sidl_Integer__copy(
  JNIEnv* env,
  jobject obj,
  jobject dest)
{
  struct sidl_int__array* csrc = sidl_Integer__getptr(env, obj);
  struct sidl_int__array* cdest = sidl_Integer__getptr(env, dest);
      
  if(csrc && cdest) {
    sidl_int__array_copy(csrc,cdest);
  } 

}

/*
 * Native function slices arrays in various ways
 */
static jobject sidl_Integer__slice(
  JNIEnv* env,
  jobject obj, 
  jint dimen, 
  jintArray numElem, 
  jintArray srcStart, 
  jintArray srcStride,
  jintArray newStart)
{
  struct sidl_int__array* array = sidl_Integer__getptr(env, obj);
  struct sidl_int__array* ret_ptr = NULL;
  jobject ret_array = NULL;
  jint* cnumElem = NULL;
  jint* csrcStart = NULL;
  jint* csrcStride = NULL;
  jint* cnewStart = NULL; 
  int i = 0; 

  if(numElem == NULL) {
    return NULL;  /*If numElem is NULL, we need to return Null, that's bad.*/
  } else {
    if((*env)->GetArrayLength(env, numElem) > 7)
      return NULL;  
    cnumElem = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      cnumElem[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, numElem, 0,
			      (*env)->GetArrayLength(env, numElem), cnumElem); 
    
  }

  if(srcStart != NULL) {
    if((*env)->GetArrayLength(env, srcStart) > 7)
      return NULL; 
    csrcStart = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      csrcStart[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, srcStart, 0, 
			    (*env)->GetArrayLength(env, srcStart), csrcStart);   
  } 

  if(srcStride != NULL) {
    if((*env)->GetArrayLength(env, srcStride) > 7)
      return NULL; 
    csrcStride = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      csrcStride[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, srcStride, 0, 
			      (*env)->GetArrayLength(env, srcStride), csrcStride);
  } 

  if(newStart != NULL) {
    if((*env)->GetArrayLength(env, newStart) > 7)
      return NULL; 
    cnewStart = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      cnewStart[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, newStart, 0, 
			      (*env)->GetArrayLength(env, newStart), cnewStart);    
  } 

  if (array != NULL) {
    /* jint should be equivalent to int32_t; Java int is 32 bits */
    ret_ptr = sidl_int__array_slice(array, dimen, cnumElem, csrcStart, csrcStride, cnewStart);
  }

  if(ret_ptr != NULL) {  
    ret_array = sidl_Java_I2J_new_array(env,ret_ptr, "sidl.Integer$Array");
  }

   return ret_array;
}

/*
 * Native routine to increment the current array reference count.
 */
static void sidl_Integer__addRef(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_int__array* array = sidl_Integer__getptr(env, obj);
  if (array != NULL) {
    sidl_int__array_addRef(array);
  }
}

/*
 * Native routine to decrement the current array reference count.
 */
static void sidl_Integer__deallocate(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_int__array* array = sidl_Integer__getptr(env, obj);
  if (array != NULL) {
    sidl_int__array_deleteRef(array);
  }
  /*sidl_Integer__setptr(env, obj, NULL);*/
}

/*
 * Native routine to destroy (deallocate) the current array data.
 */
static void sidl_Integer__destroy(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_int__array* array = sidl_Integer__getptr(env, obj);
  if (array != NULL) {
    sidl_int__array_deleteRef(array);
  }
  sidl_Integer__setptr(env, obj, NULL);
}

/*
 * Native routine to reallocate data in the array.  The specified array
 * dimension and indices must match and be within valid ranges (e.g., the
 * upper bounds must be greater than or equal to lowe rbounds.  Invalid
 * indices will have unpredictable (but almost certainly bad) results.
 * This routine will deallocate the existing array data if it is not null.
 * if argument "isRow" is true, the array will be in RowOrder, other wise
 * it will be in ColumnOrder   
 */
static void sidl_Integer__reallocate(
  JNIEnv* env,
  jobject obj,
  jint dim,
  jarray lower,
  jarray upper,
  jboolean isRow)
{
  jint* l = NULL;
  jint* u = NULL;
  struct sidl_int__array* array = NULL;

  sidl_Integer__destroy(env, obj);

  l = (*env)->GetIntArrayElements(env, lower, NULL);
  u = (*env)->GetIntArrayElements(env, upper, NULL);
  if(isRow)  
    array = sidl_int__array_createRow((int32_t) dim, (int32_t*) l,
                                          (int32_t*) u);
  else
    array = sidl_int__array_createCol((int32_t) dim, (int32_t*) l,
                                          (int32_t*) u);
  (*env)->ReleaseIntArrayElements(env, lower, l, JNI_ABORT);
  (*env)->ReleaseIntArrayElements(env, upper, u, JNI_ABORT);

  sidl_Integer__setptr(env, obj, array);
}

/*
 * Register JNI array methods with the Java JVM.
 */
void sidl_Integer__register(JNIEnv* env)
{
  JNINativeMethod methods[15];
  jclass cls;

  methods[0].name      = "_dim";
  methods[0].signature = "()I";
  methods[0].fnPtr     = sidl_Integer__dim;
  methods[1].name      = "_lower";
  methods[1].signature = "(I)I";
  methods[1].fnPtr     = sidl_Integer__lower;
  methods[2].name      = "_upper";
  methods[2].signature = "(I)I";
  methods[2].fnPtr     = sidl_Integer__upper;
  methods[3].name      = "_get";
  methods[3].signature = "(IIIIIII)I";
  methods[3].fnPtr     = sidl_Integer__get;
  methods[4].name      = "_set";
  methods[4].signature = "(IIIIIIII)V";
  methods[4].fnPtr     = sidl_Integer__set;
  methods[5].name      = "_destroy";
  methods[5].signature = "()V";
  methods[5].fnPtr     = sidl_Integer__destroy;
  methods[6].name      = "_reallocate";
  methods[6].signature = "(I[I[IZ)V";
  methods[6].fnPtr     = sidl_Integer__reallocate;
  methods[7].name      = "_stride";
  methods[7].signature = "(I)I";
  methods[7].fnPtr     = sidl_Integer__stride;  
  methods[8].name      = "_isColumnOrder";
  methods[8].signature = "()Z";
  methods[8].fnPtr     = sidl_Integer__isColumnOrder;       
  methods[9].name      = "_isRowOrder";
  methods[9].signature = "()Z";
  methods[9].fnPtr     = sidl_Integer__isRowOrder;
  methods[10].name      = "_deallocate";
  methods[10].signature = "()V";
  methods[10].fnPtr     = sidl_Integer__deallocate;
  methods[11].name      = "_copy";
  methods[11].signature = "(Lsidl/Integer$Array;)V";
  methods[11].fnPtr     = sidl_Integer__copy;  
  methods[12].name      = "_smartCopy";
  methods[12].signature = "()Lsidl/Integer$Array;";
  methods[12].fnPtr     = sidl_Integer__smartCopy;    
  methods[13].name      = "_slice";
  methods[13].signature = "(I[I[I[I[I)Lsidl/Integer$Array;";
  methods[13].fnPtr     = sidl_Integer__slice;    
  methods[14].name      = "_addRef";
  methods[14].signature = "()V";
  methods[14].fnPtr     = sidl_Integer__addRef;

  cls = (*env)->FindClass(env, "sidl/Integer$Array");
  if (cls) {
    (*env)->RegisterNatives(env, cls, methods, 15);
    (*env)->DeleteLocalRef(env, cls);
  }
}

/* 
 * Local utility function to extract the array pointer from the Java object.
 * Extract the d_array long data member and convert it to a pointer.
 */
static struct sidl_long__array* sidl_Long__getptr(
  JNIEnv* env,
  jobject obj)
{
  void* ptr = NULL;
  static jfieldID s_array_field = NULL;

  if (s_array_field == NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    s_array_field = (*env)->GetFieldID(env, cls, "d_array", "J");
    (*env)->DeleteLocalRef(env, cls);
  }

  ptr = JLONG_TO_POINTER((*env)->GetLongField(env, obj, s_array_field));
  return (struct sidl_long__array*) ptr;
}

/*
 * Local utility function to set the array pointer on the Java object.
 * Convert the pointer to a long value and set the d_array data member.
 */
static void sidl_Long__setptr(
  JNIEnv* env,
  jobject obj,
  struct sidl_long__array* array)
{
  static jfieldID s_array_field = NULL;

  if (s_array_field == NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    s_array_field = (*env)->GetFieldID(env, cls, "d_array", "J");
    (*env)->DeleteLocalRef(env, cls);
  }

  (*env)->SetLongField(env, obj, s_array_field, POINTER_TO_JLONG(array));
}

/*
 * Native routine to get the dimension of the current array.  This
 * routine assumes that the array has already been initialized.  If
 * the array has not been initialized, then horrible things may happen.
 */
static jint sidl_Long__dim(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_long__array* array = sidl_Long__getptr(env, obj);
  return (jint) sidl_long__array_dimen(array);
}

/*
 * Native routine to fetch the specified lower bound of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_Long__lower(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_long__array* array = sidl_Long__getptr(env, obj);
  return (jint) sidl_long__array_lower(array, dim);
}

/*
 * Native routine to fetch the specified upper bound of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_Long__upper(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_long__array* array = sidl_Long__getptr(env, obj);
  return (jint) sidl_long__array_upper(array, dim);
}

/*
 * Native routine to fetch the specified stride of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_Long__stride(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_long__array* array = sidl_Long__getptr(env, obj);
  return (jint) sidl_long__array_stride(array, dim);
}

/*
 * Native routine to returns true if the array is Column order.  
 */
static jboolean sidl_Long__isColumnOrder(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_long__array* array = sidl_Long__getptr(env, obj);
#if (JNI_TRUE == TRUE) && (JNI_FALSE == FALSE)
  return (jboolean) sidl_long__array_isColumnOrder(array);
#else
  return sidl_long__array_isColumnOrder(array) ? JNI_TRUE : JNI_FALSE;
#endif
}
/*
 * Native routine returns the if array is RowOrder.  
 */
static jint sidl_Long__isRowOrder(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_long__array* array = sidl_Long__getptr(env, obj);
#if (JNI_TRUE == TRUE) && (JNI_FALSE == FALSE)
  return (jboolean) sidl_long__array_isRowOrder(array);
#else
  return sidl_long__array_isRowOrder(array) ? JNI_TRUE : JNI_FALSE;
#endif
}

/*
 * Native routine to fetch the specified value from the array.  The
 * specified array index/indices must be lie between the array lower
 * upper bounds (inclusive).  Invalid indices will have unpredictable
 * (but almost certainly bad) results.
 */
static jlong sidl_Long__get(
  JNIEnv* env,
  jobject obj,
  jint i,
  jint j,
  jint k,
  jint l,
  jint m,
  jint n,
  jint o)
{
  struct sidl_long__array* array = sidl_Long__getptr(env, obj);
  int32_t a[7];

  a[0] = i; a[1] = j; a[2] = k; a[3] = l; a[4] = m; a[5] = n; a[6] = o;
  return (jlong) sidl_long__array_get(array, a);
}

/**
 * Native routine to set the specified value in the array.  The
 * specified array index/indices must be lie between the array lower
 * upper bounds (inclusive).  Invalid indices will have unpredictable
 * (but almost certainly bad) results.
 */
static void sidl_Long__set(
  JNIEnv* env,
  jobject obj,
  jint i,
  jint j,
  jint k,
  jint l,
  jint m, 
  jint n, 
  jint o,
  jlong value)
{
  struct sidl_long__array* array = sidl_Long__getptr(env, obj);
  int32_t a[7];

  a[0] = i; a[1] = j; a[2] = k; a[3] = l; a[4] = m; a[5] = n; a[6] = o;
  sidl_long__array_set(array, a, (int64_t) value);
}


/*
 * Native function copies borrowed arrays, or increments the reference count
 * of non-borrowed arrays.  Good if you want to keep a copy of a passed
 * in array.  Returns an array, new if borrowed.
 */
static jobject sidl_Long__smartCopy(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_long__array* array = sidl_Long__getptr(env, obj);
  struct sidl_long__array* ret_ptr = NULL;
  jobject ret_array = NULL;
  
  if (array != NULL) {
    ret_ptr = sidl_long__array_smartCopy(array);
  }

  /* Assuming our pointer comes back good, crate a new java array with the correct pointer,
     and owner and pass that back */
  if(ret_ptr != NULL) {  
    ret_array = sidl_Java_I2J_new_array(env,ret_ptr, "sidl.Long$Array");
  }
    return ret_array;
}


/*
 * Native function copies borrowed arrays, or increments the reference count
 * of non-borrowed arrays.  Good if you want to keep a copy of a passed
 * in array.  Returns an array, new if borrowed.
 */
static void sidl_Long__copy(
  JNIEnv* env,
  jobject obj,
  jobject dest)
{
  struct sidl_long__array* csrc = sidl_Long__getptr(env, obj);
  struct sidl_long__array* cdest = sidl_Long__getptr(env, dest);
      
  if(csrc && cdest) {
    sidl_long__array_copy(csrc,cdest);
  } 

}

/*
 * Native function slices arrays in various ways
 */
static jobject sidl_Long__slice(
  JNIEnv* env,
  jobject obj, 
  jint dimen, 
  jintArray numElem, 
  jintArray srcStart, 
  jintArray srcStride,
  jintArray newStart)
{
  struct sidl_long__array* array = sidl_Long__getptr(env, obj);
  struct sidl_long__array* ret_ptr = NULL;
  jobject ret_array = NULL;
  jint* cnumElem = NULL;
  jint* csrcStart = NULL;
  jint* csrcStride = NULL;
  jint* cnewStart = NULL; 
  int i = 0; 

  if(numElem == NULL) {
    return NULL;  /*If numElem is NULL, we need to return Null, that's bad.*/
  } else {
    if((*env)->GetArrayLength(env, numElem) > 7)
      return NULL;  
    cnumElem = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      cnumElem[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, numElem, 0,
			      (*env)->GetArrayLength(env, numElem), cnumElem); 
    
  }

  if(srcStart != NULL) {
    if((*env)->GetArrayLength(env, srcStart) > 7)
      return NULL; 
    csrcStart = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      csrcStart[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, srcStart, 0, 
			    (*env)->GetArrayLength(env, srcStart), csrcStart);   
  } 

  if(srcStride != NULL) {
    if((*env)->GetArrayLength(env, srcStride) > 7)
      return NULL; 
    csrcStride = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      csrcStride[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, srcStride, 0, 
			      (*env)->GetArrayLength(env, srcStride), csrcStride);
  } 

  if(newStart != NULL) {
    if((*env)->GetArrayLength(env, newStart) > 7)
      return NULL; 
    cnewStart = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      cnewStart[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, newStart, 0, 
			      (*env)->GetArrayLength(env, newStart), cnewStart);    
  } 

  if (array != NULL) {
    /* jint should be equivalent to int32_t; Java int is 32 bits */
    ret_ptr = sidl_long__array_slice(array, dimen, cnumElem, csrcStart, csrcStride, cnewStart);
  }

  if(ret_ptr != NULL) {  
    ret_array = sidl_Java_I2J_new_array(env,ret_ptr, "sidl.Long$Array");
  }

   return ret_array;
}

/*
 * Native routine to increment the current array reference count.
 */
static void sidl_Long__addRef(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_long__array* array = sidl_Long__getptr(env, obj);
  if (array != NULL) {
    sidl_long__array_addRef(array);
  }
}

/*
 * Native routine to decrement the current array reference count.
 */
static void sidl_Long__deallocate(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_long__array* array = sidl_Long__getptr(env, obj);
  if (array != NULL) {
    sidl_long__array_deleteRef(array);
  }
  /*sidl_Long__setptr(env, obj, NULL);*/
}

/*
 * Native routine to destroy (deallocate) the current array data.
 */
static void sidl_Long__destroy(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_long__array* array = sidl_Long__getptr(env, obj);
  if (array != NULL) {
    sidl_long__array_deleteRef(array);
  }
  sidl_Long__setptr(env, obj, NULL);
}

/*
 * Native routine to reallocate data in the array.  The specified array
 * dimension and indices must match and be within valid ranges (e.g., the
 * upper bounds must be greater than or equal to lowe rbounds.  Invalid
 * indices will have unpredictable (but almost certainly bad) results.
 * This routine will deallocate the existing array data if it is not null.
 * if argument "isRow" is true, the array will be in RowOrder, other wise
 * it will be in ColumnOrder   
 */
static void sidl_Long__reallocate(
  JNIEnv* env,
  jobject obj,
  jint dim,
  jarray lower,
  jarray upper,
  jboolean isRow)
{
  jint* l = NULL;
  jint* u = NULL;
  struct sidl_long__array* array = NULL;

  sidl_Long__destroy(env, obj);

  l = (*env)->GetIntArrayElements(env, lower, NULL);
  u = (*env)->GetIntArrayElements(env, upper, NULL);
  if(isRow)  
    array = sidl_long__array_createRow((int32_t) dim, (int32_t*) l,
                                          (int32_t*) u);
  else
    array = sidl_long__array_createCol((int32_t) dim, (int32_t*) l,
                                          (int32_t*) u);
  (*env)->ReleaseIntArrayElements(env, lower, l, JNI_ABORT);
  (*env)->ReleaseIntArrayElements(env, upper, u, JNI_ABORT);

  sidl_Long__setptr(env, obj, array);
}

/*
 * Register JNI array methods with the Java JVM.
 */
void sidl_Long__register(JNIEnv* env)
{
  JNINativeMethod methods[15];
  jclass cls;

  methods[0].name      = "_dim";
  methods[0].signature = "()I";
  methods[0].fnPtr     = sidl_Long__dim;
  methods[1].name      = "_lower";
  methods[1].signature = "(I)I";
  methods[1].fnPtr     = sidl_Long__lower;
  methods[2].name      = "_upper";
  methods[2].signature = "(I)I";
  methods[2].fnPtr     = sidl_Long__upper;
  methods[3].name      = "_get";
  methods[3].signature = "(IIIIIII)J";
  methods[3].fnPtr     = sidl_Long__get;
  methods[4].name      = "_set";
  methods[4].signature = "(IIIIIIIJ)V";
  methods[4].fnPtr     = sidl_Long__set;
  methods[5].name      = "_destroy";
  methods[5].signature = "()V";
  methods[5].fnPtr     = sidl_Long__destroy;
  methods[6].name      = "_reallocate";
  methods[6].signature = "(I[I[IZ)V";
  methods[6].fnPtr     = sidl_Long__reallocate;
  methods[7].name      = "_stride";
  methods[7].signature = "(I)I";
  methods[7].fnPtr     = sidl_Long__stride;  
  methods[8].name      = "_isColumnOrder";
  methods[8].signature = "()Z";
  methods[8].fnPtr     = sidl_Long__isColumnOrder;       
  methods[9].name      = "_isRowOrder";
  methods[9].signature = "()Z";
  methods[9].fnPtr     = sidl_Long__isRowOrder;
  methods[10].name      = "_deallocate";
  methods[10].signature = "()V";
  methods[10].fnPtr     = sidl_Long__deallocate;
  methods[11].name      = "_copy";
  methods[11].signature = "(Lsidl/Long$Array;)V";
  methods[11].fnPtr     = sidl_Long__copy;  
  methods[12].name      = "_smartCopy";
  methods[12].signature = "()Lsidl/Long$Array;";
  methods[12].fnPtr     = sidl_Long__smartCopy;    
  methods[13].name      = "_slice";
  methods[13].signature = "(I[I[I[I[I)Lsidl/Long$Array;";
  methods[13].fnPtr     = sidl_Long__slice;    
  methods[14].name      = "_addRef";
  methods[14].signature = "()V";
  methods[14].fnPtr     = sidl_Long__addRef;

  cls = (*env)->FindClass(env, "sidl/Long$Array");
  if (cls) {
    (*env)->RegisterNatives(env, cls, methods, 15);
    (*env)->DeleteLocalRef(env, cls);
  }
}

/* 
 * Local utility function to extract the array pointer from the Java object.
 * Extract the d_array long data member and convert it to a pointer.
 */
static struct sidl_opaque__array* sidl_Opaque__getptr(
  JNIEnv* env,
  jobject obj)
{
  void* ptr = NULL;
  static jfieldID s_array_field = NULL;

  if (s_array_field == NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    s_array_field = (*env)->GetFieldID(env, cls, "d_array", "J");
    (*env)->DeleteLocalRef(env, cls);
  }

  ptr = JLONG_TO_POINTER((*env)->GetLongField(env, obj, s_array_field));
  return (struct sidl_opaque__array*) ptr;
}

/*
 * Local utility function to set the array pointer on the Java object.
 * Convert the pointer to a long value and set the d_array data member.
 */
static void sidl_Opaque__setptr(
  JNIEnv* env,
  jobject obj,
  struct sidl_opaque__array* array)
{
  static jfieldID s_array_field = NULL;

  if (s_array_field == NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    s_array_field = (*env)->GetFieldID(env, cls, "d_array", "J");
    (*env)->DeleteLocalRef(env, cls);
  }

  (*env)->SetLongField(env, obj, s_array_field, POINTER_TO_JLONG(array));
}

/*
 * Native routine to get the dimension of the current array.  This
 * routine assumes that the array has already been initialized.  If
 * the array has not been initialized, then horrible things may happen.
 */
static jint sidl_Opaque__dim(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_opaque__array* array = sidl_Opaque__getptr(env, obj);
  return (jint) sidl_opaque__array_dimen(array);
}

/*
 * Native routine to fetch the specified lower bound of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_Opaque__lower(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_opaque__array* array = sidl_Opaque__getptr(env, obj);
  return (jint) sidl_opaque__array_lower(array, dim);
}

/*
 * Native routine to fetch the specified upper bound of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_Opaque__upper(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_opaque__array* array = sidl_Opaque__getptr(env, obj);
  return (jint) sidl_opaque__array_upper(array, dim);
}

/*
 * Native routine to fetch the specified stride of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_Opaque__stride(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_opaque__array* array = sidl_Opaque__getptr(env, obj);
  return (jint) sidl_opaque__array_stride(array, dim);
}

/*
 * Native routine to returns true if the array is Column order.  
 */
static jboolean sidl_Opaque__isColumnOrder(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_opaque__array* array = sidl_Opaque__getptr(env, obj);
#if (JNI_TRUE == TRUE) && (JNI_FALSE == FALSE)
  return (jboolean) sidl_opaque__array_isColumnOrder(array);
#else
  return sidl_opaque__array_isColumnOrder(array) ? JNI_TRUE : JNI_FALSE;
#endif
}
/*
 * Native routine returns the if array is RowOrder.  
 */
static jint sidl_Opaque__isRowOrder(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_opaque__array* array = sidl_Opaque__getptr(env, obj);
#if (JNI_TRUE == TRUE) && (JNI_FALSE == FALSE)
  return (jboolean) sidl_opaque__array_isRowOrder(array);
#else
  return sidl_opaque__array_isRowOrder(array) ? JNI_TRUE : JNI_FALSE;
#endif
}

/*
 * Native routine to fetch the specified value from the array.  The
 * specified array index/indices must be lie between the array lower
 * upper bounds (inclusive).  Invalid indices will have unpredictable
 * (but almost certainly bad) results.
 */
static jlong sidl_Opaque__get(
  JNIEnv* env,
  jobject obj,
  jint i,
  jint j,
  jint k,
  jint l,
  jint m,
  jint n,
  jint o)
{
  struct sidl_opaque__array* array = sidl_Opaque__getptr(env, obj);
  int32_t a[7];

  a[0] = i; a[1] = j; a[2] = k; a[3] = l; a[4] = m; a[5] = n; a[6] = o;
  return POINTER_TO_JLONG(sidl_opaque__array_get(array, a));
}

/**
 * Native routine to set the specified value in the array.  The
 * specified array index/indices must be lie between the array lower
 * upper bounds (inclusive).  Invalid indices will have unpredictable
 * (but almost certainly bad) results.
 */
static void sidl_Opaque__set(
  JNIEnv* env,
  jobject obj,
  jint i,
  jint j,
  jint k,
  jint l,
  jint m, 
  jint n, 
  jint o,
  jlong value)
{
  struct sidl_opaque__array* array = sidl_Opaque__getptr(env, obj);
  int32_t a[7];

  a[0] = i; a[1] = j; a[2] = k; a[3] = l; a[4] = m; a[5] = n; a[6] = o;
  sidl_opaque__array_set(array, a, JLONG_TO_POINTER(value));
}


/*
 * Native function copies borrowed arrays, or increments the reference count
 * of non-borrowed arrays.  Good if you want to keep a copy of a passed
 * in array.  Returns an array, new if borrowed.
 */
static jobject sidl_Opaque__smartCopy(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_opaque__array* array = sidl_Opaque__getptr(env, obj);
  struct sidl_opaque__array* ret_ptr = NULL;
  jobject ret_array = NULL;
  
  if (array != NULL) {
    ret_ptr = sidl_opaque__array_smartCopy(array);
  }

  /* Assuming our pointer comes back good, crate a new java array with the correct pointer,
     and owner and pass that back */
  if(ret_ptr != NULL) {  
    ret_array = sidl_Java_I2J_new_array(env,ret_ptr, "sidl.Opaque$Array");
  }
    return ret_array;
}


/*
 * Native function copies borrowed arrays, or increments the reference count
 * of non-borrowed arrays.  Good if you want to keep a copy of a passed
 * in array.  Returns an array, new if borrowed.
 */
static void sidl_Opaque__copy(
  JNIEnv* env,
  jobject obj,
  jobject dest)
{
  struct sidl_opaque__array* csrc = sidl_Opaque__getptr(env, obj);
  struct sidl_opaque__array* cdest = sidl_Opaque__getptr(env, dest);
      
  if(csrc && cdest) {
    sidl_opaque__array_copy(csrc,cdest);
  } 

}

/*
 * Native function slices arrays in various ways
 */
static jobject sidl_Opaque__slice(
  JNIEnv* env,
  jobject obj, 
  jint dimen, 
  jintArray numElem, 
  jintArray srcStart, 
  jintArray srcStride,
  jintArray newStart)
{
  struct sidl_opaque__array* array = sidl_Opaque__getptr(env, obj);
  struct sidl_opaque__array* ret_ptr = NULL;
  jobject ret_array = NULL;
  jint* cnumElem = NULL;
  jint* csrcStart = NULL;
  jint* csrcStride = NULL;
  jint* cnewStart = NULL; 
  int i = 0; 

  if(numElem == NULL) {
    return NULL;  /*If numElem is NULL, we need to return Null, that's bad.*/
  } else {
    if((*env)->GetArrayLength(env, numElem) > 7)
      return NULL;  
    cnumElem = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      cnumElem[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, numElem, 0,
			      (*env)->GetArrayLength(env, numElem), cnumElem); 
    
  }

  if(srcStart != NULL) {
    if((*env)->GetArrayLength(env, srcStart) > 7)
      return NULL; 
    csrcStart = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      csrcStart[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, srcStart, 0, 
			    (*env)->GetArrayLength(env, srcStart), csrcStart);   
  } 

  if(srcStride != NULL) {
    if((*env)->GetArrayLength(env, srcStride) > 7)
      return NULL; 
    csrcStride = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      csrcStride[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, srcStride, 0, 
			      (*env)->GetArrayLength(env, srcStride), csrcStride);
  } 

  if(newStart != NULL) {
    if((*env)->GetArrayLength(env, newStart) > 7)
      return NULL; 
    cnewStart = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      cnewStart[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, newStart, 0, 
			      (*env)->GetArrayLength(env, newStart), cnewStart);    
  } 

  if (array != NULL) {
    /* jint should be equivalent to int32_t; Java int is 32 bits */
    ret_ptr = sidl_opaque__array_slice(array, dimen, cnumElem, csrcStart, csrcStride, cnewStart);
  }

  if(ret_ptr != NULL) {  
    ret_array = sidl_Java_I2J_new_array(env,ret_ptr, "sidl.Opaque$Array");
  }

   return ret_array;
}

/*
 * Native routine to increment the current array reference count.
 */
static void sidl_Opaque__addRef(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_opaque__array* array = sidl_Opaque__getptr(env, obj);
  if (array != NULL) {
    sidl_opaque__array_addRef(array);
  }
}

/*
 * Native routine to decrement the current array reference count.
 */
static void sidl_Opaque__deallocate(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_opaque__array* array = sidl_Opaque__getptr(env, obj);
  if (array != NULL) {
    sidl_opaque__array_deleteRef(array);
  }
  /*sidl_Opaque__setptr(env, obj, NULL);*/
}

/*
 * Native routine to destroy (deallocate) the current array data.
 */
static void sidl_Opaque__destroy(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_opaque__array* array = sidl_Opaque__getptr(env, obj);
  if (array != NULL) {
    sidl_opaque__array_deleteRef(array);
  }
  sidl_Opaque__setptr(env, obj, NULL);
}

/*
 * Native routine to reallocate data in the array.  The specified array
 * dimension and indices must match and be within valid ranges (e.g., the
 * upper bounds must be greater than or equal to lowe rbounds.  Invalid
 * indices will have unpredictable (but almost certainly bad) results.
 * This routine will deallocate the existing array data if it is not null.
 * if argument "isRow" is true, the array will be in RowOrder, other wise
 * it will be in ColumnOrder   
 */
static void sidl_Opaque__reallocate(
  JNIEnv* env,
  jobject obj,
  jint dim,
  jarray lower,
  jarray upper,
  jboolean isRow)
{
  jint* l = NULL;
  jint* u = NULL;
  struct sidl_opaque__array* array = NULL;

  sidl_Opaque__destroy(env, obj);

  l = (*env)->GetIntArrayElements(env, lower, NULL);
  u = (*env)->GetIntArrayElements(env, upper, NULL);
  if(isRow)  
    array = sidl_opaque__array_createRow((int32_t) dim, (int32_t*) l,
                                          (int32_t*) u);
  else
    array = sidl_opaque__array_createCol((int32_t) dim, (int32_t*) l,
                                          (int32_t*) u);
  (*env)->ReleaseIntArrayElements(env, lower, l, JNI_ABORT);
  (*env)->ReleaseIntArrayElements(env, upper, u, JNI_ABORT);

  sidl_Opaque__setptr(env, obj, array);
}

/*
 * Register JNI array methods with the Java JVM.
 */
void sidl_Opaque__register(JNIEnv* env)
{
  JNINativeMethod methods[15];
  jclass cls;

  methods[0].name      = "_dim";
  methods[0].signature = "()I";
  methods[0].fnPtr     = sidl_Opaque__dim;
  methods[1].name      = "_lower";
  methods[1].signature = "(I)I";
  methods[1].fnPtr     = sidl_Opaque__lower;
  methods[2].name      = "_upper";
  methods[2].signature = "(I)I";
  methods[2].fnPtr     = sidl_Opaque__upper;
  methods[3].name      = "_get";
  methods[3].signature = "(IIIIIII)J";
  methods[3].fnPtr     = sidl_Opaque__get;
  methods[4].name      = "_set";
  methods[4].signature = "(IIIIIIIJ)V";
  methods[4].fnPtr     = sidl_Opaque__set;
  methods[5].name      = "_destroy";
  methods[5].signature = "()V";
  methods[5].fnPtr     = sidl_Opaque__destroy;
  methods[6].name      = "_reallocate";
  methods[6].signature = "(I[I[IZ)V";
  methods[6].fnPtr     = sidl_Opaque__reallocate;
  methods[7].name      = "_stride";
  methods[7].signature = "(I)I";
  methods[7].fnPtr     = sidl_Opaque__stride;  
  methods[8].name      = "_isColumnOrder";
  methods[8].signature = "()Z";
  methods[8].fnPtr     = sidl_Opaque__isColumnOrder;       
  methods[9].name      = "_isRowOrder";
  methods[9].signature = "()Z";
  methods[9].fnPtr     = sidl_Opaque__isRowOrder;
  methods[10].name      = "_deallocate";
  methods[10].signature = "()V";
  methods[10].fnPtr     = sidl_Opaque__deallocate;
  methods[11].name      = "_copy";
  methods[11].signature = "(Lsidl/Opaque$Array;)V";
  methods[11].fnPtr     = sidl_Opaque__copy;  
  methods[12].name      = "_smartCopy";
  methods[12].signature = "()Lsidl/Opaque$Array;";
  methods[12].fnPtr     = sidl_Opaque__smartCopy;    
  methods[13].name      = "_slice";
  methods[13].signature = "(I[I[I[I[I)Lsidl/Opaque$Array;";
  methods[13].fnPtr     = sidl_Opaque__slice;    
  methods[14].name      = "_addRef";
  methods[14].signature = "()V";
  methods[14].fnPtr     = sidl_Opaque__addRef;

  cls = (*env)->FindClass(env, "sidl/Opaque$Array");
  if (cls) {
    (*env)->RegisterNatives(env, cls, methods, 15);
    (*env)->DeleteLocalRef(env, cls);
  }
}

/* 
 * Local utility function to extract the array pointer from the Java object.
 * Extract the d_array long data member and convert it to a pointer.
 */
static struct sidl_string__array* sidl_String__getptr(
  JNIEnv* env,
  jobject obj)
{
  void* ptr = NULL;
  static jfieldID s_array_field = NULL;

  if (s_array_field == NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    s_array_field = (*env)->GetFieldID(env, cls, "d_array", "J");
    (*env)->DeleteLocalRef(env, cls);
  }

  ptr = JLONG_TO_POINTER((*env)->GetLongField(env, obj, s_array_field));
  return (struct sidl_string__array*) ptr;
}

/*
 * Local utility function to set the array pointer on the Java object.
 * Convert the pointer to a long value and set the d_array data member.
 */
static void sidl_String__setptr(
  JNIEnv* env,
  jobject obj,
  struct sidl_string__array* array)
{
  static jfieldID s_array_field = NULL;

  if (s_array_field == NULL) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    s_array_field = (*env)->GetFieldID(env, cls, "d_array", "J");
    (*env)->DeleteLocalRef(env, cls);
  }

  (*env)->SetLongField(env, obj, s_array_field, POINTER_TO_JLONG(array));
}

/*
 * Native routine to get the dimension of the current array.  This
 * routine assumes that the array has already been initialized.  If
 * the array has not been initialized, then horrible things may happen.
 */
static jint sidl_String__dim(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_string__array* array = sidl_String__getptr(env, obj);
  return (jint) sidl_string__array_dimen(array);
}

/*
 * Native routine to fetch the specified lower bound of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_String__lower(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_string__array* array = sidl_String__getptr(env, obj);
  return (jint) sidl_string__array_lower(array, dim);
}

/*
 * Native routine to fetch the specified upper bound of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_String__upper(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_string__array* array = sidl_String__getptr(env, obj);
  return (jint) sidl_string__array_upper(array, dim);
}

/*
 * Native routine to fetch the specified stride of the array.  The
 * specified array dimension must be between zero and the array dimension
 * minus one.  Invalid values will have unpredictable (but almost certainly
 * bad) results.
 */
static jint sidl_String__stride(
  JNIEnv* env,
  jobject obj,
  jint dim)
{
  struct sidl_string__array* array = sidl_String__getptr(env, obj);
  return (jint) sidl_string__array_stride(array, dim);
}

/*
 * Native routine to returns true if the array is Column order.  
 */
static jboolean sidl_String__isColumnOrder(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_string__array* array = sidl_String__getptr(env, obj);
#if (JNI_TRUE == TRUE) && (JNI_FALSE == FALSE)
  return (jboolean) sidl_string__array_isColumnOrder(array);
#else
  return sidl_string__array_isColumnOrder(array) ? JNI_TRUE : JNI_FALSE;
#endif
}
/*
 * Native routine returns the if array is RowOrder.  
 */
static jint sidl_String__isRowOrder(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_string__array* array = sidl_String__getptr(env, obj);
#if (JNI_TRUE == TRUE) && (JNI_FALSE == FALSE)
  return (jboolean) sidl_string__array_isRowOrder(array);
#else
  return sidl_string__array_isRowOrder(array) ? JNI_TRUE : JNI_FALSE;
#endif
}

/*
 * Native routine to fetch the specified value from the array.  The
 * specified array index/indices must be lie between the array lower
 * upper bounds (inclusive).  Invalid indices will have unpredictable
 * (but almost certainly bad) results.
 */
static jstring sidl_String__get(
  JNIEnv* env,
  jobject obj,
  jint i,
  jint j,
  jint k,
  jint l,
  jint m,
  jint n,
  jint o)
{
  struct sidl_string__array* array = sidl_String__getptr(env, obj);
  int32_t a[7];
  char* value;
  jstring jstr;

  a[0] = i; a[1] = j; a[2] = k; a[3] = l; a[4] = m; a[5] = n; a[6] = o;
  value = sidl_string__array_get(array, a);
  jstr = sidl_Java_I2J_string(env, value);
  sidl_String_free(value);
  return jstr;
}

/**
 * Native routine to set the specified value in the array.  The
 * specified array index/indices must be lie between the array lower
 * upper bounds (inclusive).  Invalid indices will have unpredictable
 * (but almost certainly bad) results.
 */
static void sidl_String__set(
  JNIEnv* env,
  jobject obj,
  jint i,
  jint j,
  jint k,
  jint l,
  jint m, 
  jint n, 
  jint o,
  jstring value)
{
  struct sidl_string__array* array = sidl_String__getptr(env, obj);
  int32_t a[7];
  char* elem;

  a[0] = i; a[1] = j; a[2] = k; a[3] = l; a[4] = m; a[5] = n; a[6] = o;
  elem = sidl_Java_J2I_string(env, value);
  sidl_string__array_set(array, a, elem);
  sidl_String_free(elem);
}


/*
 * Native function copies borrowed arrays, or increments the reference count
 * of non-borrowed arrays.  Good if you want to keep a copy of a passed
 * in array.  Returns an array, new if borrowed.
 */
static jobject sidl_String__smartCopy(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_string__array* array = sidl_String__getptr(env, obj);
  struct sidl_string__array* ret_ptr = NULL;
  jobject ret_array = NULL;
  
  if (array != NULL) {
    ret_ptr = sidl_string__array_smartCopy(array);
  }

  /* Assuming our pointer comes back good, crate a new java array with the correct pointer,
     and owner and pass that back */
  if(ret_ptr != NULL) {  
    ret_array = sidl_Java_I2J_new_array(env,ret_ptr, "sidl.String$Array");
  }
    return ret_array;
}


/*
 * Native function copies borrowed arrays, or increments the reference count
 * of non-borrowed arrays.  Good if you want to keep a copy of a passed
 * in array.  Returns an array, new if borrowed.
 */
static void sidl_String__copy(
  JNIEnv* env,
  jobject obj,
  jobject dest)
{
  struct sidl_string__array* csrc = sidl_String__getptr(env, obj);
  struct sidl_string__array* cdest = sidl_String__getptr(env, dest);
      
  if(csrc && cdest) {
    sidl_string__array_copy(csrc,cdest);
  } 

}

/*
 * Native function slices arrays in various ways
 */
static jobject sidl_String__slice(
  JNIEnv* env,
  jobject obj, 
  jint dimen, 
  jintArray numElem, 
  jintArray srcStart, 
  jintArray srcStride,
  jintArray newStart)
{
  struct sidl_string__array* array = sidl_String__getptr(env, obj);
  struct sidl_string__array* ret_ptr = NULL;
  jobject ret_array = NULL;
  jint* cnumElem = NULL;
  jint* csrcStart = NULL;
  jint* csrcStride = NULL;
  jint* cnewStart = NULL; 
  int i = 0; 

  if(numElem == NULL) {
    return NULL;  /*If numElem is NULL, we need to return Null, that's bad.*/
  } else {
    if((*env)->GetArrayLength(env, numElem) > 7)
      return NULL;  
    cnumElem = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      cnumElem[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, numElem, 0,
			      (*env)->GetArrayLength(env, numElem), cnumElem); 
    
  }

  if(srcStart != NULL) {
    if((*env)->GetArrayLength(env, srcStart) > 7)
      return NULL; 
    csrcStart = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      csrcStart[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, srcStart, 0, 
			    (*env)->GetArrayLength(env, srcStart), csrcStart);   
  } 

  if(srcStride != NULL) {
    if((*env)->GetArrayLength(env, srcStride) > 7)
      return NULL; 
    csrcStride = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      csrcStride[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, srcStride, 0, 
			      (*env)->GetArrayLength(env, srcStride), csrcStride);
  } 

  if(newStart != NULL) {
    if((*env)->GetArrayLength(env, newStart) > 7)
      return NULL; 
    cnewStart = malloc(7*sizeof(int32_t));
    for(i = 0; i < 7; ++i) {  /*Make sure the array is clean*/ 
      cnewStart[i] = 0;
    }
    (*env)->GetIntArrayRegion(env, newStart, 0, 
			      (*env)->GetArrayLength(env, newStart), cnewStart);    
  } 

  if (array != NULL) {
    /* jint should be equivalent to int32_t; Java int is 32 bits */
    ret_ptr = sidl_string__array_slice(array, dimen, cnumElem, csrcStart, csrcStride, cnewStart);
  }

  if(ret_ptr != NULL) {  
    ret_array = sidl_Java_I2J_new_array(env,ret_ptr, "sidl.String$Array");
  }

   return ret_array;
}

/*
 * Native routine to increment the current array reference count.
 */
static void sidl_String__addRef(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_string__array* array = sidl_String__getptr(env, obj);
  if (array != NULL) {
    sidl_string__array_addRef(array);
  }
}

/*
 * Native routine to decrement the current array reference count.
 */
static void sidl_String__deallocate(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_string__array* array = sidl_String__getptr(env, obj);
  if (array != NULL) {
    sidl_string__array_deleteRef(array);
  }
  /*sidl_String__setptr(env, obj, NULL);*/
}

/*
 * Native routine to destroy (deallocate) the current array data.
 */
static void sidl_String__destroy(
  JNIEnv* env,
  jobject obj)
{
  struct sidl_string__array* array = sidl_String__getptr(env, obj);
  if (array != NULL) {
    sidl_string__array_deleteRef(array);
  }
  sidl_String__setptr(env, obj, NULL);
}

/*
 * Native routine to reallocate data in the array.  The specified array
 * dimension and indices must match and be within valid ranges (e.g., the
 * upper bounds must be greater than or equal to lowe rbounds.  Invalid
 * indices will have unpredictable (but almost certainly bad) results.
 * This routine will deallocate the existing array data if it is not null.
 * if argument "isRow" is true, the array will be in RowOrder, other wise
 * it will be in ColumnOrder   
 */
static void sidl_String__reallocate(
  JNIEnv* env,
  jobject obj,
  jint dim,
  jarray lower,
  jarray upper,
  jboolean isRow)
{
  jint* l = NULL;
  jint* u = NULL;
  struct sidl_string__array* array = NULL;

  sidl_String__destroy(env, obj);

  l = (*env)->GetIntArrayElements(env, lower, NULL);
  u = (*env)->GetIntArrayElements(env, upper, NULL);
  if(isRow)  
    array = sidl_string__array_createRow((int32_t) dim, (int32_t*) l,
                                          (int32_t*) u);
  else
    array = sidl_string__array_createCol((int32_t) dim, (int32_t*) l,
                                          (int32_t*) u);
  (*env)->ReleaseIntArrayElements(env, lower, l, JNI_ABORT);
  (*env)->ReleaseIntArrayElements(env, upper, u, JNI_ABORT);

  sidl_String__setptr(env, obj, array);
}

/*
 * Register JNI array methods with the Java JVM.
 */
void sidl_String__register(JNIEnv* env)
{
  JNINativeMethod methods[15];
  jclass cls;

  methods[0].name      = "_dim";
  methods[0].signature = "()I";
  methods[0].fnPtr     = sidl_String__dim;
  methods[1].name      = "_lower";
  methods[1].signature = "(I)I";
  methods[1].fnPtr     = sidl_String__lower;
  methods[2].name      = "_upper";
  methods[2].signature = "(I)I";
  methods[2].fnPtr     = sidl_String__upper;
  methods[3].name      = "_get";
  methods[3].signature = "(IIIIIII)Ljava/lang/String;";
  methods[3].fnPtr     = sidl_String__get;
  methods[4].name      = "_set";
  methods[4].signature = "(IIIIIIILjava/lang/String;)V";
  methods[4].fnPtr     = sidl_String__set;
  methods[5].name      = "_destroy";
  methods[5].signature = "()V";
  methods[5].fnPtr     = sidl_String__destroy;
  methods[6].name      = "_reallocate";
  methods[6].signature = "(I[I[IZ)V";
  methods[6].fnPtr     = sidl_String__reallocate;
  methods[7].name      = "_stride";
  methods[7].signature = "(I)I";
  methods[7].fnPtr     = sidl_String__stride;  
  methods[8].name      = "_isColumnOrder";
  methods[8].signature = "()Z";
  methods[8].fnPtr     = sidl_String__isColumnOrder;       
  methods[9].name      = "_isRowOrder";
  methods[9].signature = "()Z";
  methods[9].fnPtr     = sidl_String__isRowOrder;
  methods[10].name      = "_deallocate";
  methods[10].signature = "()V";
  methods[10].fnPtr     = sidl_String__deallocate;
  methods[11].name      = "_copy";
  methods[11].signature = "(Lsidl/String$Array;)V";
  methods[11].fnPtr     = sidl_String__copy;  
  methods[12].name      = "_smartCopy";
  methods[12].signature = "()Lsidl/String$Array;";
  methods[12].fnPtr     = sidl_String__smartCopy;    
  methods[13].name      = "_slice";
  methods[13].signature = "(I[I[I[I[I)Lsidl/String$Array;";
  methods[13].fnPtr     = sidl_String__slice;    
  methods[14].name      = "_addRef";
  methods[14].signature = "()V";
  methods[14].fnPtr     = sidl_String__addRef;

  cls = (*env)->FindClass(env, "sidl/String$Array");
  if (cls) {
    (*env)->RegisterNatives(env, cls, methods, 15);
    (*env)->DeleteLocalRef(env, cls);
  }
}



