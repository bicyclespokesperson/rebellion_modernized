// Dumps every resource of a PE file as "type name lang size fnv1a64" lines (sorted by the order of the resource directory),
// the format of pe_dump_ref.py, so the two can be diffed (src/port/minwin_impl/res/tests/compare_pefile.sh).
#include <stdio.h>

#include "../pe_module.h"

using namespace mw::res;

static std::string Name(const ResId& id) { return id.isString ? "'" + id.name + "'" : std::to_string(id.id); }

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    Module* m = Module::Open(argv[1]);
    if (!m) {
        fprintf(stderr, "not a PE with resources: %s\n", argv[1]);
        return 1;
    }
    for (const ResId& type : m->ListTypes()) {
        for (const ResId& name : m->ListNames(type)) {
            Entry e;
            if (!m->Find(type, name, &e)) {
                printf("%s %s MISSING\n", Name(type).c_str(), Name(name).c_str());
                continue;
            }
            uint64_t h = 1469598103934665603ull;
            for (uint32_t i = 0; i < e.size; ++i) h = (h ^ e.data[i]) * 1099511628211ull;
            printf("%s %s %u %u %016llx\n", Name(type).c_str(), Name(name).c_str(), e.language, e.size, static_cast<unsigned long long>(h));
        }
    }
    return 0;
}
