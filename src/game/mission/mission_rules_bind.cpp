#include <cstddef>

#include "data/gnprtb.h"
#include "mission/mission_rules.h"

// The GNPRTB loaders of the mission rules (0x55bfd0, 0x55e290, 0x55e340, 0x55e5b0, 0x55e840, 0x55ea00).



namespace {

struct GnprtbBinding {
    int id;
    int* target;
};

template <size_t N>
int bindAll(const GnprtbBinding (&bindings)[N]) {
    int allBound = 1;
    for (const GnprtbBinding& binding : bindings) allBound = BindGnprtbParameter(binding.id, binding.target) && allBound;
    return allBound;
}

}  // namespace

int BindGnprtb18() {
    static const GnprtbBinding kBindings[] = {
        {0x1800, &g_gnprtb1800},
        {0x1801, &g_gnprtb1801},
        {0x1802, &g_gnprtb1802},
        {0x1803, &g_gnprtb1803},
        {0x1804, &g_gnprtb1804},
        {0x1805, &g_gnprtb1805},
        {0x1806, &g_gnprtb1806},
        {0x1807, &g_gnprtb1807},
        {0x1808, &g_gnprtb1808},
        {0x1809, &g_gnprtb1809},
        {0x180a, &g_gnprtb180A},
        {0x180b, &g_gnprtb180B},
        {0x180c, &g_gnprtb180C},
        {0x180d, &g_gnprtb180D},
        {0x180e, &g_gnprtb180E},
        {0x180f, &g_gnprtb180F},
        {0x1810, &g_gnprtb1810},
        {0x1811, &g_gnprtb1811},
        {0x1812, &g_gnprtb1812},
        {0x1813, &g_gnprtb1813},
        {0x1814, &g_gnprtb1814},
        {0x1815, &g_gnprtb1815},
        {0x1816, &g_gnprtb1816},
        {0x1817, &g_gnprtb1817},
        {0x1818, &g_gnprtb1818},
        {0x1819, &g_gnprtb1819},
        {0x181a, &g_gnprtb181A},
        {0x181b, &g_gnprtb181B},
        {0x181c, &g_gnprtb181C},
        {0x181d, &g_gnprtb181D},
        {0x181e, &g_gnprtb181E},
        {0x181f, &g_gnprtb181F},
        {0x1820, &g_gnprtb1820},
        {0x1821, &g_gnprtb1821},
        {0x1822, &g_gnprtb1822},
        {0x1823, &g_gnprtb1823},
        {0x1824, &g_gnprtb1824},
        {0x1825, &g_gnprtb1825},
        {0x1827, &g_gnprtb1827},
        {0x1828, &g_gnprtb1828},
        {0x1829, &g_gnprtb1829},
        {0x182a, &g_gnprtb182A},
        {0x182b, &g_gnprtb182B},
        {0x182c, &g_gnprtb182C},
        {0x182d, &g_gnprtb182D},
        {0x182e, &g_gnprtb182E},
    };
    return bindAll(kBindings);
}

int BindGnprtb10() {
    static const GnprtbBinding kBindings[] = {
        {0x1000, &g_gnprtb1000},
    };
    return bindAll(kBindings);
}

int BindGnprtb0E() {
    static const GnprtbBinding kBindings[] = {
        {0xe00, &g_gnprtb0E00},
        {0xe01, &g_gnprtb0E01},
        {0xe02, &g_gnprtb0E02},
        {0xe03, &g_gnprtb0E03},
        {0xe04, &g_gnprtb0E04},
        {0xe05, &g_gnprtb0E05},
    };
    return bindAll(kBindings);
}

int BindGnprtb0C() {
    static const GnprtbBinding kBindings[] = {
        {0xc00, &g_gnprtb0C00},
        {0xc01, &g_gnprtb0C01},
        {0xc02, &g_gnprtb0C02},
        {0xc03, &g_gnprtb0C03},
        {0xc04, &g_gnprtb0C04},
        {0xc05, &g_gnprtb0C05},
        {0xc06, &g_gnprtb0C06},
        {0xc07, &g_gnprtb0C07},
        {0xc08, &g_gnprtb0C08},
        {0xc09, &g_gnprtb0C09},
        {0xc0a, &g_gnprtb0C0A},
        {0xc0b, &g_gnprtb0C0B},
        {0xc0c, &g_gnprtb0C0C},
        {0xc0d, &g_gnprtb0C0D},
        {0xc0e, &g_gnprtb0C0E},
        {0xc0f, &g_gnprtb0C0F},
    };
    return bindAll(kBindings);
}

int BindGnprtb06() {
    static const GnprtbBinding kBindings[] = {
        {0x600, &g_gnprtb0600},
        {0x601, &g_gnprtb0601},
        {0x602, &g_gnprtb0602},
        {0x603, &g_gnprtb0603},
        {0x604, &g_gnprtb0604},
        {0x605, &g_gnprtb0605},
        {0x606, &g_gnprtb0606},
    };
    return bindAll(kBindings);
}

int BindGnprtb02() {
    static const GnprtbBinding kBindings[] = {
        {0x200, &g_gnprtb0200},
        {0x201, &g_gnprtb0201},
        {0x202, &g_gnprtb0202},
        {0x203, &g_gnprtb0203},
        {0x204, &g_gnprtb0204},
        {0x205, &g_gnprtb0205},
    };
    return bindAll(kBindings);
}
