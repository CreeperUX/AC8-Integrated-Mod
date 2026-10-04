// Read-only camera entry-point discovery. No hooks, game writes or calls.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
static HMODULE module;
static bool read(uintptr_t p,void* out,SIZE_T n) {
    SIZE_T got=0;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),out,n,&got) && got==n;
}
static void code(FILE* report,uintptr_t p,uintptr_t base,size_t image_size) {
    if(p<base || p>=base+image_size) { fprintf(report," outside-main-image\n"); return; }
    unsigned char bytes[96]{};
    fprintf(report," rva=%llX",static_cast<unsigned long long>(p-base));
    if(read(p,bytes,sizeof(bytes))) for(auto b:bytes) fprintf(report," %02X",b);
    fprintf(report,"\n");
}
extern "C" __declspec(dllexport) int ac8_camera_probe(void*) {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(module,path,MAX_PATH);
    auto slash=wcsrchr(path,L'\\'); if(!slash) return 0; *slash=0;
    wchar_t request[MAX_PATH]{},output[MAX_PATH]{};
    swprintf_s(request,L"%s\\camera-probe-request.txt",path);
    swprintf_s(output,L"%s\\camera-native-layout.txt",path);
    FILE *in=nullptr,*out=nullptr;
    if(_wfopen_s(&in,request,L"r") || !in) return 0;
    if(_wfopen_s(&out,output,L"w") || !out) { fclose(in); return 0; }
    auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    IMAGE_DOS_HEADER dos{}; IMAGE_NT_HEADERS64 nt{};
    if(!read(base,&dos,sizeof(dos)) || !read(base+dos.e_lfanew,&nt,sizeof(nt))) {
        fclose(in); fclose(out); return 0;
    }
    fprintf(out,"READ-ONLY camera probe; image timestamp=%08lX size=%lX\n",
        nt.FileHeader.TimeDateStamp,nt.OptionalHeader.SizeOfImage);
    char line[1024]{};
    while(fgets(line,sizeof(line),in)) {
        char kind[8]{},name[768]{}; unsigned long long address=0;
        if(sscanf_s(line,"%7s %llx %767[^\n]",kind,8u,&address,name,768u)!=3) continue;
        fprintf(out,"%s %s\n",kind,name);
        if(strcmp(kind,"FUNC")==0) {
            // UFunction::Func=0xD8 from this build's UE4SS resolved offset log.
            uintptr_t fn=0;
            if(read(address+0xD8,&fn,sizeof(fn))) code(out,fn,base,nt.OptionalHeader.SizeOfImage);
        } else if(strcmp(kind,"OBJECT")==0) {
            uintptr_t table=0;
            if(!read(address,&table,sizeof(table))) continue;
            for(unsigned slot=0;slot<192;++slot) {
                uintptr_t fn=0;
                if(!read(table+slot*sizeof(uintptr_t),&fn,sizeof(fn))) break;
                fprintf(out,"slot=%u",slot); code(out,fn,base,nt.OptionalHeader.SizeOfImage);
            }
        }
    }
    fprintf(out,"COMPLETE\n"); fclose(in); fclose(out); return 0;
}
BOOL WINAPI DllMain(HINSTANCE h,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) { module=h; DisableThreadLibraryCalls(h); }
    return TRUE;
}
