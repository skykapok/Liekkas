#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#include <stdio.h>

#include <stdbool.h>
#include <lua.h>
#include <lauxlib.h>

#include "oal_decode.h"

#define OAL_RATE_HIGH 44100
#define OAL_RATE_MID  22050
#define OAL_RATE_LOW  16000
#define OAL_RATE_BASIC 8000
#define OAL_RATE_DEFAULT 44100



typedef ALvoid  AL_APIENTRY (*alcMacOSXMixerOutputRateProcPtr) (const ALdouble value);
static ALvoid  
alcMacOSXMixerOutputRateProc(const ALdouble value) {
  static  alcMacOSXMixerOutputRateProcPtr proc = NULL;
  if (proc == NULL) {
    proc = (alcMacOSXMixerOutputRateProcPtr) alcGetProcAddress(NULL, (const ALCchar*) "alcMacOSXMixerOutputRate");
  }

  if (proc)
    proc(value);

  return;
}


static void
_init_openal(lua_State* L) {
  ALCcontext* context;
  ALCdevice* new_device = NULL;
  alcMacOSXMixerOutputRateProc(OAL_RATE_DEFAULT);

  // Create a new OpenAL Device
  // Pass NULL to specify the system's default output device
  new_device = alcOpenDevice(NULL);
  if(new_device) {
    // Create a new OpenAL Context
    // The new context will render to the OpenAL Device just created
    context = alcCreateContext(new_device, 0);
    if(context){
      // Make the new context the Current OpenAL Context
      alcMakeContextCurrent(context);
    }else {
      luaL_error(L, "openal context error");
    }
  }else {
    luaL_error(L, "no device");
  }
}

static int
_id2string(lua_State* L) {
  ALuint id = *((ALuint*)lua_touserdata(L, 1));
  char buffer[24] = {0};
  sprintf(buffer, "%d", id);
  lua_pushstring(L, buffer);
  return 1;
}


static int
l_free_source(lua_State* L) {
  ALuint source_id = *((ALuint*)lua_touserdata(L, 1));
  alSourceStop(source_id);
  alSourcei(source_id, AL_BUFFER, 0);
  alDeleteSources(1, &source_id);
  int err;
  if((err=alGetError()) != AL_NO_ERROR) {
    luaL_error(L, "free error source id[%d]", source_id);
  }
  return 0;
}

static int
l_play_source(lua_State* L) {
  ALuint source_id = *((ALuint*)lua_touserdata(L, 1));
  alSourcePlay(source_id);
  return 0;
}


static int
l_pause_source(lua_State* L) {
  ALuint source_id = *((ALuint*)lua_touserdata(L, 1));
  alSourcePause(source_id);
  return 0;
}

static int
l_rewind_source(lua_State* L) {
  ALuint source_id = *((ALuint*)lua_touserdata(L, 1));
  alSourceRewind(source_id);
  return 0;
}

static int
l_stop_source(lua_State* L) {
  ALuint source_id = *((ALuint*)lua_touserdata(L, 1));
  alSourceStop(source_id);
  return 0;
}


static int
l_clear_source(lua_State* L) {
  ALuint source_id = *((ALuint*)lua_touserdata(L, 1));
  alSourceStop(source_id);
  alSourcei(source_id, AL_BUFFER, 0);
  int err;
  if((err=alGetError()) != AL_NO_ERROR) {
    luaL_error(L, "clear error source id[%d]", source_id);
  }
  return 0;
}

static int
_new_source(lua_State* L, ALuint source_id) {
  ALuint* p = lua_newuserdata(L, sizeof(source_id));
  *p = source_id;
  if(luaL_newmetatable(L, "mt_source")) {
    lua_pushcfunction(L, l_free_source);
    lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, _id2string);
    lua_setfield(L, -2, "__tostring");

    luaL_Reg l[] = {
      {"clear", l_clear_source},
      {"play", l_play_source},
      {"stop", l_stop_source},
      {"rewind", l_rewind_source},
      {"pause", l_pause_source},
      {NULL, NULL},
    };
    luaL_newlib(L, l);
    lua_setfield(L, -2, "__index");
  }

  lua_setmetatable(L, -2);
  return 1;
}


static int
l_create_source(lua_State* L) {
  int err=AL_NO_ERROR;
  alGetError(); // clear error
  ALuint source_id;
  alGenSources(1, &source_id);
  err = alGetError();
  if(err == AL_NO_ERROR) {
    //Now try attaching source to null buffer
    alSourcei(source_id, AL_BUFFER, 0);
    err = alGetError();
    if(err != AL_NO_ERROR) {
      luaL_error(L, "create source error[%d]", err);
    }
  }else {
    luaL_error(L, "create source error[%d]", err);
  }

  return _new_source(L, source_id);
}

static int
l_free_bufferid(lua_State* L) {
  ALuint buffer_id = *((ALuint*)lua_touserdata(L, 1));
  alDeleteBuffers(1, &buffer_id);
  return 0;
}


static int
_new_bufferid(lua_State* L, ALuint buffer_id) {
  ALuint* p = lua_newuserdata(L, sizeof(buffer_id));
  *p = buffer_id;
  if(luaL_newmetatable(L, "mt_buffer")) {
    lua_pushcfunction(L, l_free_bufferid);
    lua_setfield(L, -2, "__gc");
    lua_pushcfunction(L, _id2string);
    lua_setfield(L, -2, "__tostring");
  }

  lua_setmetatable(L, -2);
  return 1;
}


static int
l_create_bufferid(lua_State* L) {
  int err=AL_NO_ERROR;
  alGetError(); // clear error
  ALuint buffer_id;
  alGenBuffers(1, &buffer_id);
  err = alGetError();
  if(err != AL_NO_ERROR) {
    luaL_error(L, "create bufferid error[%d]", err);
  }

  return _new_bufferid(L, buffer_id);
}

static int
l_bind_buffer(lua_State* L) {
  ALuint buffer_id = *((ALuint*)lua_touserdata(L, 1));
  struct oal_info* info = lua_touserdata(L, 2);

  int err=AL_NO_ERROR;
  alGetError(); // clear error
  alBufferData(buffer_id, info->format, info->data, info->size, info->freq);
  err = alGetError();
  if(err != AL_NO_ERROR) {
    luaL_error(L, "bind buffer error[%d]", err);
  }

  return 0;
}

static int
l_set(lua_State* L) {
  ALuint source_id = *((ALuint*)lua_touserdata(L, 1));
  ALuint buffer_id = *((ALuint*)lua_touserdata(L, 2));
  lua_Number pitch = lua_tonumber(L, 3);
  lua_Number pan = lua_tonumber(L, 4);
  lua_Number gain = lua_tonumber(L, 5);
  int loop = lua_toboolean(L, 6);

  ALint state;

  int err=AL_NO_ERROR;
  alGetError(); // clear error

  alGetSourcei(source_id, AL_SOURCE_STATE, &state);
  if (state == AL_PLAYING) {
    alSourceStop(source_id);
  }

  alSourcei(source_id, AL_BUFFER, buffer_id);
  alSourcef(source_id, AL_PITCH, pitch);
  alSourcei(source_id, AL_LOOPING, loop);
  alSourcef(source_id, AL_GAIN, gain);
  float sourcePosAL[] = {pan, 0.0f, 0.0f}; //Set position - just using left and right panning
  alSourcefv(source_id, AL_POSITION, sourcePosAL);

  err = alGetError();
  if(err == AL_NO_ERROR) {
    lua_pushinteger(L, source_id);
  }else if(alcGetCurrentContext() == NULL) {
    luaL_error(L, "posting bad OpenAL context message");
  }

  lua_pushvalue(L, 1);
  return 1;
}


static bool _is_load = false;
// maybe never call
static void
_free_oal() {
  if(_is_load) {
    ALCcontext  *currentContext = alcGetCurrentContext();
    ALCdevice *device = alcGetContextsDevice(currentContext); 

    alcDestroyContext(currentContext);
    alcCloseDevice(device);
  }
}


int
luaopen_oal(lua_State* L) {
  (void)(_free_oal);  // unused

  // init openal
  if (!_is_load) {
    _init_openal(L);
    _is_load = true;
  }

  // set lib
  luaL_checkversion(L);
  luaL_Reg l[] = {
    {"create_source", l_create_source},
    {"create_bufferid", l_create_bufferid},
    {"buffer_bind", l_bind_buffer},
    {"source_set", l_set},
    {NULL, NULL},
  };

  luaL_newlib(L, l);
  return 1;
}

