// Bring-up aid: an unhandled exception writes the registers and the code addresses found on the stack to REB_LOG
// (default rebellion.log); tools/map_lookup.py --log <file> turns them into function names.
#include <windows.h>

namespace {

void Write(HANDLE file, const char* format, ...) {
    char line[256];
    va_list args;
    va_start(args, format);
    const int n = wvsprintfA(line, format, args);
    va_end(args);
    DWORD written;
    WriteFile(file, line, static_cast<DWORD>(n), &written, nullptr);
}

bool InImageCode(DWORD address) {
    const auto base = reinterpret_cast<const BYTE*>(GetModuleHandleW(nullptr));
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const DWORD begin = reinterpret_cast<DWORD>(base) + nt->OptionalHeader.BaseOfCode;
    return address >= begin && address < begin + nt->OptionalHeader.SizeOfCode;
}

LONG WINAPI LogCrash(EXCEPTION_POINTERS* info) {
    char path[MAX_PATH] = "rebellion.log";
    GetEnvironmentVariableA("REB_LOG", path, sizeof path);
    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return EXCEPTION_CONTINUE_SEARCH;
    const EXCEPTION_RECORD* record = info->ExceptionRecord;
    const CONTEXT* c = info->ContextRecord;
    Write(file, "exception %08x at %08x (access address %08x)\r\n", record->ExceptionCode,
          static_cast<DWORD>(reinterpret_cast<DWORD_PTR>(record->ExceptionAddress)),
          record->NumberParameters > 1 ? static_cast<DWORD>(record->ExceptionInformation[1]) : 0);
    Write(file, "eax %08x ebx %08x ecx %08x edx %08x esi %08x edi %08x ebp %08x esp %08x\r\n", c->Eax, c->Ebx, c->Ecx,
          c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp);
    const DWORD registers[] = {c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi};
    const char* const names[] = {"eax", "ebx", "ecx", "edx", "esi", "edi"};
    for (int r = 0; r < 6; ++r) {
        const DWORD* memory = reinterpret_cast<const DWORD*>(registers[r]);
        if (registers[r] < 0x10000 || IsBadReadPtr(memory, 64)) continue;
        Write(file, "[%s]", names[r]);
        for (int i = 0; i < 16; ++i) Write(file, " %08x", memory[i]);
        Write(file, "\r\n");
    }
    Write(file, "code addresses on the stack:\r\n");
    const DWORD* stack = reinterpret_cast<const DWORD*>(c->Esp);
    int found = 0;
    for (int i = 0; i < 2048 && found < 40; ++i) {
        if (IsBadReadPtr(stack + i, 4)) break;
        if (InImageCode(stack[i])) {
            Write(file, "  esp+%04x  %08x\r\n", i * 4, stack[i]);
            ++found;
        }
    }
    CloseHandle(file);
    return EXCEPTION_CONTINUE_SEARCH;
}

}  // namespace

void InstallCrashLog() {
    SetUnhandledExceptionFilter(LogCrash);
}

