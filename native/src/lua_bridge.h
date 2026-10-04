#pragma once
#include "vendor/ue4ss/LuaMadeSimple.hpp"
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

// Only use the public exported LuaMadeSimple API, never lua_State internals.
// Header is a snapshot of the supplied UE4SS source (fmt -> std::format only).
// The bundled runtime is pinned before constructing any C++ Lua wrapper.
inline bool compatible_lua_runtime() {
    HMODULE module=GetModuleHandleW(L"UE4SS.dll");
    wchar_t path[MAX_PATH]{};
    if (!module || !GetModuleFileNameW(module,path,MAX_PATH)) return false;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if (file==INVALID_HANDLE_VALUE) return false;
    BCRYPT_ALG_HANDLE alg=nullptr;
    BCRYPT_HASH_HANDLE hash=nullptr;
    bool ok=BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0;
    if (ok) ok=BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0)>=0;
    unsigned char buffer[65536],digest[32]{};
    DWORD read=0;
    while (ok) {
        if (!ReadFile(file,buffer,sizeof(buffer),&read,nullptr)) { ok=false; break; }
        if (!read) break;
        ok=BCryptHashData(hash,buffer,read,0)>=0;
    }
    if (ok) ok=BCryptFinishHash(hash,digest,sizeof(digest),0)>=0;
    if (hash) BCryptDestroyHash(hash);
    if (alg) BCryptCloseAlgorithmProvider(alg,0);
    CloseHandle(file);
    constexpr unsigned char expected[32]={
        0x68,0x0A,0x02,0x68,0x90,0xAB,0xB4,0xD0,0xDF,0x22,0x11,0x25,0x1F,0x8D,0xEF,0xC1,
        0x68,0x1A,0x58,0x42,0x75,0xF1,0x52,0x1D,0xCC,0x0F,0xE3,0x0A,0xF4,0x80,0x00,0x6F};
    return ok && memcmp(digest,expected,sizeof(expected))==0;
}
using LuaView=RC::LuaMadeSimple::Lua;
// Verified bundled constructor: registry reference at +0xA0, total 0xA8.
static_assert(sizeof(LuaView)==0xA8 && alignof(LuaView)==8,"UE4SS Lua wrapper ABI changed");
template<size_t N> bool read_numbers(const LuaView& lua,double (&args)[N]) {
    if (lua.get_stack_size()!=N) return false;
    // get_number removes the value, so pop in reverse order.
    for (size_t i=N;i>0;--i) {
        if (!lua.is_number(-1)) return false;
        args[i-1]=lua.get_number(-1);
        if (!std::isfinite(args[i-1])) return false;
    }
    return true;
}
inline bool live_pointer_number(double n) {
    return n>=65536 && n<=static_cast<double>(0x00007fffffff0000ull) &&
        n==std::floor(n) && (static_cast<uintptr_t>(n)&7)==0;
}
