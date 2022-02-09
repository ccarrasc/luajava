#include "jni.h"
#include "lua.hpp"

#include "jua.h"
#include "juaapi.h"
#include "luaexception.h"

// For template usage
const char JAVA_CLASS_META_REGISTRY[] = "__JavaClassMetatable";
const char JAVA_OBJECT_META_REGISTRY[] = "__JavaObjectMetatable";
const char JAVA_ARRAY_META_REGISTRY[] =  "__JavaArrayMetatable";

// Bindings
// java.lang.Class, Class::forName
jclass    java_lang_class_class   = NULL;
jmethodID java_lang_class_forname = NULL;
// party.iroiro.jua.JuaAPI
jclass    juaapi_class          = NULL;
jmethodID juaapi_classnew       = NULL;
jmethodID juaapi_classindex     = NULL;
jmethodID juaapi_classinvoke    = NULL;
jmethodID juaapi_classnewindex  = NULL;
jmethodID juaapi_objectindex    = NULL;
jmethodID juaapi_objectinvoke   = NULL;
jmethodID juaapi_objectnewindex = NULL;
jmethodID juaapi_arraylen       = NULL;
jmethodID juaapi_arrayindex     = NULL;
jmethodID juaapi_arraynewindex  = NULL;
// java.lang.Throwable
jclass java_lang_throwable_class = NULL;
jmethodID throwable_getmessage   = NULL;
jmethodID throwable_tostring     = NULL;

/**
 * The new panic function that panics the JVM
 */
int fatalError(lua_State * L) {
  JNIEnv * env = getJNIEnv(L);
  env->FatalError(lua_tostring(L, -1));
  return 0;
}

/**
 * Returns a global reference to the class matching the name
 *
 * Throws C++ LuaException when class not found or whatever.
 */
jclass bindJavaClass(JNIEnv * env, const char * name) {
  jclass tempClass;
  tempClass = env->FindClass(name);
  if (tempClass == NULL) {
      throw LuaException("Could not find the class");
  } else {
    jclass classRef = (jclass) env->NewGlobalRef(tempClass);
    // https://stackoverflow.com/q/33481144/17780636
    // env->DeleteLocalRef(tempClass);
    if (classRef == NULL) {
      throw LuaException("Could not bind the class");
    } else {
      return classRef;
    }
  }
}

/**
 * Returns the methodID or throws LuaException
 */
jmethodID bindJavaStaticMethod(JNIEnv * env, jclass c, const char * name, const char * sig) {
  jmethodID id = env->GetStaticMethodID(c, name, sig);
  if (id == NULL) {
    throw LuaException("Could not find the method");
  }
  return id;
}

/**
 * Returns the methodID or throws LuaException
 */
jmethodID bindJavaMethod(JNIEnv * env, jclass c, const char * name, const char * sig) {
  jmethodID id = env->GetMethodID(c, name, sig);
  if (id == NULL) {
    throw LuaException("Could not find the method");
  }
  return id;
}

// TODO: switch to reinterpret_cast<jclass> etc.
/**
 * Init JNI cache bindings
 * See Jua.java
 *
 * Throws LuaException when exceptions occur
 */
void initBindings(JNIEnv * env) {
  java_lang_class_class = bindJavaClass(env, "java/lang/Class");
  java_lang_class_forname = bindJavaStaticMethod(env, java_lang_class_class,
          "forName", "(Ljava/lang/String;)Ljava/lang/Class;");

  java_lang_throwable_class = bindJavaClass(env, "java/lang/Throwable");
  throwable_getmessage = bindJavaMethod(env, java_lang_throwable_class,
          "getMessage", "()Ljava/lang/String;");
  throwable_tostring = bindJavaMethod(env, java_lang_throwable_class,
          "toString", "()Ljava/lang/String;");

  juaapi_class = bindJavaClass(env, "party/iroiro/jua/JuaAPI");
  juaapi_classnew = bindJavaStaticMethod(env, juaapi_class,
          "classNew", "(ILjava/lang/Class;I)I");
  juaapi_classindex = bindJavaStaticMethod(env, juaapi_class,
          "classIndex", "(ILjava/lang/Class;Ljava/lang/String;)I");
  juaapi_classinvoke = bindJavaStaticMethod(env, juaapi_class,
          "classInvoke", "(ILjava/lang/Class;Ljava/lang/String;I)I");
  juaapi_classnewindex = bindJavaStaticMethod(env, juaapi_class,
          "classNewIndex", "(ILjava/lang/Class;Ljava/lang/String;)I");
  juaapi_objectindex = bindJavaStaticMethod(env, juaapi_class,
          "objectIndex", "(ILjava/lang/Object;Ljava/lang/String;)I");
  juaapi_objectinvoke = bindJavaStaticMethod(env, juaapi_class,
          "objectInvoke", "(ILjava/lang/Object;Ljava/lang/String;I)I");
  juaapi_objectnewindex = bindJavaStaticMethod(env, juaapi_class,
          "objectNewIndex", "(ILjava/lang/Object;Ljava/lang/String;)I");
  juaapi_arraylen = bindJavaStaticMethod(env, juaapi_class,
          "arrayLength", "(Ljava/lang/Object;)I");
  juaapi_arrayindex = bindJavaStaticMethod(env, juaapi_class,
          "arrayIndex", "(ILjava/lang/Object;I)I");
  juaapi_arraynewindex = bindJavaStaticMethod(env, juaapi_class,
          "arrayNewIndex", "(ILjava/lang/Object;I)I");
}

#define LUA_METAFIELD_GC "__gc"
#define LUA_METAFIELD_LEN "__len"
#define LUA_METAFIELD_INDEX "__index"
#define LUA_METAFIELD_NEWINDEX "__newindex"
/**
 * Inits JAVA_CLASS_META_REGISTRY, JAVA_OBJECT_META_REGISTRY
 */
void initMetaRegistry(lua_State * L) {
  if (luaL_newmetatable(L, JAVA_CLASS_META_REGISTRY) == 1) {
    lua_pushcfunction(L, &gc<JAVA_CLASS_META_REGISTRY>);
    lua_setfield(L, -2, LUA_METAFIELD_GC);
    lua_pushcfunction(L, &jclassIndex);
    lua_setfield(L, -2, LUA_METAFIELD_INDEX);
    lua_pushcfunction(L, &jclassNewIndex);
    lua_setfield(L, -2, LUA_METAFIELD_NEWINDEX);
    lua_pop(L, 1);
  }
  if (luaL_newmetatable(L, JAVA_OBJECT_META_REGISTRY) == 1) {
    lua_pushcfunction(L, &gc<JAVA_OBJECT_META_REGISTRY>);
    lua_setfield(L, -2, LUA_METAFIELD_GC);
    lua_pushcfunction(L, &jobjectIndex);
    lua_setfield(L, -2, LUA_METAFIELD_INDEX);
    lua_pushcfunction(L, &jobjectNewIndex);
    lua_setfield(L, -2, LUA_METAFIELD_NEWINDEX);
    lua_pop(L, 1);
  }
  if (luaL_newmetatable(L, JAVA_ARRAY_META_REGISTRY) == 1) {
    lua_pushcfunction(L, &gc<JAVA_ARRAY_META_REGISTRY>);
    lua_setfield(L, -2, LUA_METAFIELD_GC);
    lua_pushcfunction(L, &jarrayLength);
    lua_setfield(L, -2, LUA_METAFIELD_LEN);
    lua_pushcfunction(L, &jarrayIndex);
    lua_setfield(L, -2, LUA_METAFIELD_INDEX);
    lua_pushcfunction(L, &jarrayNewIndex);
    lua_setfield(L, -2, LUA_METAFIELD_NEWINDEX);
    lua_pop(L, 1);
  }
}

int getStateIndex(lua_State * L) {
  lua_pushstring(L, JAVA_STATE_INDEX);
  lua_rawget(L, LUA_REGISTRYINDEX);
  lua_Integer stateIndex = lua_tointeger(L, -1);
  lua_pop(L, 1);
  return (int) stateIndex;
}

void updateJNIEnv(JNIEnv * env, lua_State * L) {
  JNIEnv ** udEnv;
  lua_pushstring(L, JNIENV_INDEX);
  lua_rawget(L, LUA_REGISTRYINDEX);
  udEnv = (JNIEnv **) lua_touserdata(L, -1);
  *udEnv = env;
  lua_pop(L, 1);
}

JNIEnv * getJNIEnv(lua_State * L) {
  JNIEnv * env;
  lua_pushstring(L, JNIENV_INDEX);
  lua_rawget(L, LUA_REGISTRYINDEX);
  env = * (JNIEnv **) lua_touserdata(L, -1);
  lua_pop(L, 1);
  return env;
}