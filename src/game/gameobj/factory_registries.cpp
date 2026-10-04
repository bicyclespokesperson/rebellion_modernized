#include "gameobj/factory_registries.h"

#include "gameobj/game_obj_root.h"
#include "orders/order_base.h"
#include "orders/orders_210_216.h"

// The factories themselves live in the modules of the classes they build; only their addresses are needed here.
// Taking the address of a function declared by its original name yields the original address.
GameObjNotifier* FUN_00577530();
GameObjNotifier* FUN_00576d60();
GameObjNotifier* FUN_005769b0();
GameObjNotifier* FUN_00526ad0();
GameObjNotifier* FUN_005764d0();
GameObjNotifier* FUN_005761e0();
GameObjNotifier* FUN_00575890();
GameObjNotifier* FUN_0052c2e0();
GameObjNotifier* FUN_0055b890();
GameObjNotifier* FUN_00566c70();
GameObjNotifier* FUN_005755c0();
GameObjNotifier* FUN_00574aa0();
GameObjNotifier* FUN_005002a0();
GameObjNotifier* FUN_00574450();
GameObjNotifier* FUN_0051c140();
GameObjNotifier* FUN_00573e70();
GameObjNotifier* FUN_00573ba0();
GameObjNotifier* FUN_00572fa0();
GameObjNotifier* FUN_005035c0();
GameObjNotifier* FUN_004fee30();
GameObjNotifier* FUN_00513970();
GameObjNotifier* FUN_00572810();
GameObjNotifier* FUN_00571eb0();
GameObjNotifier* FUN_005718e0();
GameObjNotifier* FUN_00527450();
GameObjNotifier* FUN_00570620();
GameObjNotifier* FUN_0056ffe0();
GameObjNotifier* FUN_0056f8e0();
GameObjNotifier* FUN_0052d3e0();
GameObjNotifier* FUN_0056f230();
GameObjNotifier* FUN_00549cf0();
GameObjNotifier* FUN_0056eee0();
GameObjNotifier* FUN_0056ebd0();
GameObjNotifier* FUN_0056df00();
GameObjNotifier* FUN_0056d480();
GameObjNotifier* FUN_00527740();
GameObjNotifier* FUN_0056c3f0();
GameObjNotifier* FUN_0056bdb0();
GameObjNotifier* FUN_0056b300();
GameObjNotifier* FUN_0052cfb0();
GameObjNotifier* FUN_0056ad40();
GameObjNotifier* FUN_0056aa30();
GameObjNotifier* FUN_0056a4a0();
GameObjNotifier* FUN_0056a0a0();
GameObjNotifier* FUN_00526590();
GameObjNotifier* FUN_0052c930();
GameObjNotifier* FUN_0055bc10();
GameObjNotifier* FUN_005309b0();
GameObjNotifier* FUN_00503ee0();
GameObjNotifier* FUN_004fffe0();
GameObjNotifier* FUN_00569aa0();
GameObjNotifier* FUN_00556f30();
GameObjNotifier* FUN_00562120();
GameObjNotifier* FUN_0052c610();
GameObjNotifier* FUN_0055b4f0();
GameObjNotifier* FUN_00504560();
GameObjNotifier* FUN_005271c0();
GameObjNotifier* FUN_00569760();

void* FUN_00580240();
void* FUN_00564730();
void* FUN_0057b0a0();
void* FUN_005641d0();
void* FUN_00563c80();
void* FUN_00563f20();
void* FUN_00564ce0();
void* FUN_00564480();
void* FUN_0057d520();
void* FUN_0057c8b0();
void* FUN_0057a890();
void* FUN_0057b360();
void* FUN_0057b610();
void* FUN_0057a5e0();
void* FUN_0057a340();
void* FUN_0057ab40();
void* FUN_0057adf0();
void* FUN_0057d880();
void* FUN_0054dc00();
void* FUN_0054dec0();
void* FUN_005788f0();
void* FUN_0057b8d0();
void* FUN_0057bbb0();
void* FUN_00541110();
void* FUN_005413d0();
void* FUN_00541c10();
void* FUN_00541960();
void* FUN_00541690();
void* FUN_005649e0();
void* FUN_00564fa0();
void* FUN_0057a090();
void* FUN_005639d0();
void* FUN_0057fa50();
void* FUN_0057fe20();
void* FUN_0057df50();
void* FUN_0057e5f0();
void* FUN_0057ee10();
void* FUN_0057f3c0();
void* FUN_0057ea30();
void* FUN_0057c4b0();
void* FUN_0057db30();
void* FUN_00579ce0();
void* FUN_0054cfc0();
void* FUN_00578b90();
void* FUN_005778a0();
void* FUN_00577bf0();
void* FUN_00579a20();
void* FUN_00579280();
void* FUN_00540d30();
void* FUN_00579650();
void* FUN_00563180();
void* FUN_00563580();
void* FUN_00562d70();
void* FUN_0057cb60();
void* FUN_0057ce90();
void* FUN_0057d1f0();
void* FUN_0057c160();
void* FUN_0057be20();
void* FUN_0054d8b0();
void* FUN_0054e4e0();
void* FUN_0054e810();
void* FUN_0054d580();
void* FUN_005785a0();
void* FUN_00577f20();
void* FUN_00578250();
void* FUN_005662b0();
void* FUN_00565c50();
void* FUN_00565920();
void* FUN_005655f0();
void* FUN_00566600();
void* FUN_00566950();
void* FUN_005652c0();
void* FUN_00565f80();

void* FUN_00581450();
void* FUN_00582980();
void* FUN_00582710();
void* FUN_005828b0();
void* FUN_005827e0();
void* FUN_00580f30();
void* FUN_00581010();
void* FUN_005810d0();
void* FUN_00581190();
void* FUN_00581250();
void* FUN_00580a10();
void* FUN_00580950();
void* FUN_00580580();
void* CreateMoveOrder();
void* CreateConfirmedMoveOrder();
void* CreateObjectMoveOrder();
void* CreateReadyForDeleteOrder();
void* CreateRenameOrder();
void* CreateRenameOrder215();
void* FUN_004f5250();
void* FUN_004f5490();
void* FUN_004f45b0();
void* CreateScrapOrder();
void* FUN_00628e60();

// Registers `kind`'s event factory in the table at 0x6b92f8 (0x54f140).
int FUN_0054f140(int kind, void* (*factory)());

constexpr int kMaxKeyedFactories = 0x800;

struct KeyedFactoryEntry {
    int key;
    void* (*create)();
};

extern int g_keyedFactoryCount;                 // 0x6b6fd8
extern KeyedFactoryEntry g_keyedFactories[];    // 0x6b2fd8, kMaxKeyedFactories entries

void RegisterGameObjFactories() {
    GameObjRoot::RegisterFactory(0x98, FUN_00577530);
    GameObjRoot::RegisterFactory(0x62, FUN_00576d60);
    GameObjRoot::RegisterFactory(0x44, FUN_005769b0);
    GameObjRoot::RegisterFactory(0x20, FUN_00526ad0);
    GameObjRoot::RegisterFactory(0x63, FUN_005764d0);
    GameObjRoot::RegisterFactory(0x43, FUN_005761e0);
    GameObjRoot::RegisterFactory(0x65, FUN_00575890);
    GameObjRoot::RegisterFactory(0x2a, FUN_0052c2e0);
    GameObjRoot::RegisterFactory(0xa0, FUN_0055b890);
    GameObjRoot::RegisterFactory(0x90, FUN_00566c70);
    GameObjRoot::RegisterFactory(0x35, FUN_005755c0);
    GameObjRoot::RegisterFactory(0x71, FUN_00574aa0);
    GameObjRoot::RegisterFactory(0x18, FUN_005002a0);
    GameObjRoot::RegisterFactory(0x6a, FUN_00574450);
    GameObjRoot::RegisterFactory(0x25, FUN_0051c140);
    GameObjRoot::RegisterFactory(0x51, FUN_00573e70);
    GameObjRoot::RegisterFactory(0x34, FUN_00573ba0);
    GameObjRoot::RegisterFactory(0x52, FUN_00572fa0);
    GameObjRoot::RegisterFactory(0x1c, FUN_005035c0);
    GameObjRoot::RegisterFactory(0x8, FUN_004fee30);
    GameObjRoot::RegisterFactory(0xf1, FUN_00513970);
    GameObjRoot::RegisterFactory(0x33, FUN_00572810);
    GameObjRoot::RegisterFactory(0x72, FUN_00571eb0);
    GameObjRoot::RegisterFactory(0x56, FUN_005718e0);
    GameObjRoot::RegisterFactory(0x22, FUN_00527450);
    GameObjRoot::RegisterFactory(0x58, FUN_00570620);
    GameObjRoot::RegisterFactory(0x31, FUN_0056ffe0);
    GameObjRoot::RegisterFactory(0x32, FUN_0056f8e0);
    GameObjRoot::RegisterFactory(0x2c, FUN_0052d3e0);
    GameObjRoot::RegisterFactory(0x38, FUN_0056f230);
    GameObjRoot::RegisterFactory(0xf8, FUN_00549cf0);
    GameObjRoot::RegisterFactory(0x30, FUN_0056eee0);
    GameObjRoot::RegisterFactory(0x41, FUN_0056ebd0);
    GameObjRoot::RegisterFactory(0x64, FUN_0056df00);
    GameObjRoot::RegisterFactory(0x73, FUN_0056d480);
    GameObjRoot::RegisterFactory(0x24, FUN_00527740);
    GameObjRoot::RegisterFactory(0x53, FUN_0056c3f0);
    GameObjRoot::RegisterFactory(0x54, FUN_0056bdb0);
    GameObjRoot::RegisterFactory(0x55, FUN_0056b300);
    GameObjRoot::RegisterFactory(0x2d, FUN_0052cfb0);
    GameObjRoot::RegisterFactory(0x61, FUN_0056ad40);
    GameObjRoot::RegisterFactory(0x42, FUN_0056aa30);
    GameObjRoot::RegisterFactory(0x92, FUN_0056a4a0);
    GameObjRoot::RegisterFactory(0x69, FUN_0056a0a0);
    GameObjRoot::RegisterFactory(0x80, FUN_00526590);
    GameObjRoot::RegisterFactory(0x28, FUN_0052c930);
    GameObjRoot::RegisterFactory(0xa2, FUN_0055bc10);
    GameObjRoot::RegisterFactory(0xf3, FUN_005309b0);
    GameObjRoot::RegisterFactory(0x3c, FUN_00503ee0);
    GameObjRoot::RegisterFactory(0x14, FUN_004fffe0);
    GameObjRoot::RegisterFactory(0x57, FUN_00569aa0);
    GameObjRoot::RegisterFactory(0xf9, FUN_00556f30);
    GameObjRoot::RegisterFactory(0xfa, FUN_00562120);
    GameObjRoot::RegisterFactory(0x29, FUN_0052c610);
    GameObjRoot::RegisterFactory(0xa4, FUN_0055b4f0);
    GameObjRoot::RegisterFactory(0x10, FUN_00504560);
    GameObjRoot::RegisterFactory(0x23, FUN_005271c0);
    GameObjRoot::RegisterFactory(0xf2, FUN_00569760);
}

void RegisterEventFactories() {
    FUN_0054f140(0x3f0, FUN_00580240);
    FUN_0054f140(0x30f, FUN_00564730);
    FUN_0054f140(0x307, FUN_0057b0a0);
    FUN_0054f140(0x309, FUN_005641d0);
    FUN_0054f140(0x304, FUN_00563c80);
    FUN_0054f140(0x305, FUN_00563f20);
    FUN_0054f140(0x311, FUN_00564ce0);
    FUN_0054f140(0x310, FUN_00564480);
    FUN_0054f140(0x317, FUN_0057d520);
    FUN_0054f140(0x30a, FUN_0057c8b0);
    FUN_0054f140(0x308, FUN_0057a890);
    FUN_0054f140(0x302, FUN_0057b360);
    FUN_0054f140(0x318, FUN_0057b610);
    FUN_0054f140(0x300, FUN_0057a5e0);
    FUN_0054f140(0x31d, FUN_0057a340);
    FUN_0054f140(0x306, FUN_0057ab40);
    FUN_0054f140(0x316, FUN_0057adf0);
    FUN_0054f140(0x313, FUN_0057d880);
    FUN_0054f140(0x30c, FUN_0054dc00);
    FUN_0054f140(0x30d, FUN_0054dec0);
    FUN_0054f140(0x315, FUN_005788f0);
    FUN_0054f140(0x320, FUN_0057b8d0);
    FUN_0054f140(0x31a, FUN_0057bbb0);
    FUN_0054f140(0x30e, FUN_00541110);
    FUN_0054f140(0x312, FUN_005413d0);
    FUN_0054f140(0x321, FUN_00541c10);
    FUN_0054f140(0x30b, FUN_00541960);
    FUN_0054f140(0x314, FUN_00541690);
    FUN_0054f140(0x31b, FUN_005649e0);
    FUN_0054f140(0x319, FUN_00564fa0);
    FUN_0054f140(0x301, FUN_0057a090);
    FUN_0054f140(0x303, FUN_005639d0);
    FUN_0054f140(0x221, FUN_0057fa50);
    FUN_0054f140(0x220, FUN_0057fe20);
    FUN_0054f140(0x214, FUN_0057df50);
    FUN_0054f140(0x215, FUN_0057e5f0);
    FUN_0054f140(0x216, FUN_0057ee10);
    FUN_0054f140(0x210, FUN_0057f3c0);
    FUN_0054f140(0x213, FUN_0057ea30);
    FUN_0054f140(0x240, FUN_0057c4b0);
    FUN_0054f140(0x242, FUN_0057db30);
    FUN_0054f140(0x372, FUN_00579ce0);
    FUN_0054f140(0x250, FUN_0054cfc0);
    FUN_0054f140(0x201, FUN_00578b90);
    FUN_0054f140(0x370, FUN_005778a0);
    FUN_0054f140(0x371, FUN_00577bf0);
    FUN_0054f140(0x205, FUN_00579a20);
    FUN_0054f140(0x202, FUN_00579280);
    FUN_0054f140(0x230, FUN_00540d30);
    FUN_0054f140(0x200, FUN_00579650);
    FUN_0054f140(0x225, FUN_00563180);
    FUN_0054f140(0x224, FUN_00563580);
    FUN_0054f140(0x373, FUN_00562d70);
    FUN_0054f140(0x388, FUN_0057cb60);
    FUN_0054f140(0x389, FUN_0057ce90);
    FUN_0054f140(0x38a, FUN_0057d1f0);
    FUN_0054f140(0x380, FUN_0057c160);
    FUN_0054f140(0x387, FUN_0057be20);
    FUN_0054f140(0x38c, FUN_0054d8b0);
    FUN_0054f140(0x392, FUN_0054e4e0);
    FUN_0054f140(0x393, FUN_0054e810);
    FUN_0054f140(0x38b, FUN_0054d580);
    FUN_0054f140(0x394, FUN_005785a0);
    FUN_0054f140(0x382, FUN_00577f20);
    FUN_0054f140(0x381, FUN_00578250);
    FUN_0054f140(0x38e, FUN_005662b0);
    FUN_0054f140(0x386, FUN_00565c50);
    FUN_0054f140(0x385, FUN_00565920);
    FUN_0054f140(0x384, FUN_005655f0);
    FUN_0054f140(0x38f, FUN_00566600);
    FUN_0054f140(0x390, FUN_00566950);
    FUN_0054f140(0x383, FUN_005652c0);
    FUN_0054f140(0x38d, FUN_00565f80);
}

void RegisterOrderFactories() {
    RegisterOrderFactory(0x234, FUN_00581450);
    RegisterOrderFactory(0x220, FUN_00582980);
    RegisterOrderFactory(0x221, FUN_00582710);
    RegisterOrderFactory(0x222, FUN_005828b0);
    RegisterOrderFactory(0x223, FUN_005827e0);
    RegisterOrderFactory(0x214, CreateDestinationOrder);
    RegisterOrderFactory(0x215, CreateRenameOrder215);
    RegisterOrderFactory(0x216, CreateOrder216);
    RegisterOrderFactory(0x211, CreateManufactureOrder211);
    RegisterOrderFactory(0x212, CreateManufactureOrder212);
    RegisterOrderFactory(0x210, CreateManufactureOrder210);
    RegisterOrderFactory(0x213, CreateOrder213);
    RegisterOrderFactory(0x260, FUN_00580f30);
    RegisterOrderFactory(0x261, FUN_00581010);
    RegisterOrderFactory(0x262, FUN_005810d0);
    RegisterOrderFactory(0x263, FUN_00581190);
    RegisterOrderFactory(0x268, FUN_00581250);
    RegisterOrderFactory(0x270, FUN_00580a10);
    RegisterOrderFactory(0x3, FUN_00580950);
    RegisterOrderFactory(0x250, FUN_00580580);
    RegisterOrderFactory(0x201, CreateMoveOrder);
    RegisterOrderFactory(0x202, CreateConfirmedMoveOrder);
    RegisterOrderFactory(0x204, CreateObjectMoveOrder);
    RegisterOrderFactory(0x205, CreateReadyForDeleteOrder);
    RegisterOrderFactory(0x203, CreateRenameOrder);
    RegisterOrderFactory(0x240, FUN_004f5250);
    RegisterOrderFactory(0x241, FUN_004f5490);
    RegisterOrderFactory(0x242, FUN_004f45b0);
    RegisterOrderFactory(0x200, CreateScrapOrder);
    RegisterOrderFactory(0x280, FUN_00628e60);
}

void* CreateKeyedObject(int key) {
    void* (*create)() = nullptr;
    for (int i = 0; i < g_keyedFactoryCount && !create; ++i) {
        if (g_keyedFactories[i].key == key) create = g_keyedFactories[i].create;
    }
    if (create) return create();
    return nullptr;
}

int RegisterKeyedFactory(int key, void* (*factory)()) {
    bool absent = true;
    for (int i = 0; i < g_keyedFactoryCount && absent; ++i) absent = g_keyedFactories[i].key != key;
    if (absent && static_cast<uint32_t>(g_keyedFactoryCount) + 1U < kMaxKeyedFactories) {
        g_keyedFactories[g_keyedFactoryCount].key = key;
        g_keyedFactories[g_keyedFactoryCount].create = factory;
        ++g_keyedFactoryCount;
    }
    return 0;
}
