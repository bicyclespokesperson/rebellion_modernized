#pragma once
// The order commands of families 0x200-0x242 and the 0x3f0 payload event (0x578ab0-0x579fa0, 0x57c3c0-0x57c7c0, 0x57da40-0x580530).
#include <cstdint>

#include "events/event_base.h"
#include "core/game_string.h"
#include "gameobj/key_queue.h"
#include "gameobj/key_int_set.h"


// kind 0x201: moves the object its key names to the destination in its request
class MoveCommand : public Command {
public:
    MoveCommand();  // 0x578ab0
    MoveCommand(const MoveCommand& other);  // 0x578b00
    ~MoveCommand() override;  // 0x578af0 (deleting 0x578ad0)

    MoveCommand* Clone() override;  // 1: 0x578b20
    void Read(istream* in) override { ReadCommand(in); }  // 2: 0x5635f0
    void Write(ostream* out) override { WriteCommand(out); }  // 3: 0x563600
    void Checksum(Crc32* crc) override { ChecksumCommand(crc); }  // 4: 0x563610
    int CheckEnabled(StatusCode* status) override;  // 6: 0x578c00
    int CheckValid(StatusCode* status) override;  // 7: 0x578d00
    int Execute() override;  // 8: 0x578f30
    int GetKind() override { return 0x201; }  // 9: 0x53cf30
};

// kind 0x202: renames the object its key names
class RenameCommand : public Command {
public:
    RenameCommand();  // 0x5790f0
    RenameCommand(const RenameCommand& other);  // 0x5791b0
    ~RenameCommand() override;  // 0x579160 (deleting 0x579140)

    RenameCommand* Clone() override;  // 1: 0x579210
    void Read(istream* in) override;  // 2: 0x57e660
    void Write(ostream* out) override;  // 3: 0x57e680
    void Checksum(Crc32* crc) override { ChecksumCommand(crc); }  // 4: 0x563610
    int CheckEnabled(StatusCode* status) override;  // 6: 0x5792f0
    int CheckValid(StatusCode* status) override;  // 7: 0x5793c0
    int Execute() override;  // 8: 0x579490
    int GetKind() override { return 0x202; }  // 9: 0x53d000
    String m_name;  // +0x50
};

// kind 0x200: scraps (destroys) the object its key names if the request allows it
class ScrapCommand : public Command {
public:
    ScrapCommand();  // 0x579570
    ScrapCommand(const ScrapCommand& other);  // 0x5795c0
    ~ScrapCommand() override;  // 0x5795b0 (deleting 0x579590)

    ScrapCommand* Clone() override;  // 1: 0x5795e0
    void Read(istream* in) override { ReadCommand(in); }  // 2: 0x5635f0
    void Write(ostream* out) override { WriteCommand(out); }  // 3: 0x563600
    void Checksum(Crc32* crc) override { ChecksumCommand(crc); }  // 4: 0x563610
    int CheckEnabled(StatusCode* status) override;  // 6: 0x5796c0
    int CheckValid(StatusCode* status) override;  // 7: 0x579790
    int Execute() override;  // 8: 0x579860
    int GetKind() override { return 0x200; }  // 9: 0x48b450
};

// kind 0x205: lets a destroyed object be deleted (GameObj::ReleaseIfDestroyed)
class ReadyForDeleteCommand : public Command {
public:
    ReadyForDeleteCommand();  // 0x579940
    ReadyForDeleteCommand(const ReadyForDeleteCommand& other);  // 0x579990
    ~ReadyForDeleteCommand() override;  // 0x579980 (deleting 0x579960)

    ReadyForDeleteCommand* Clone() override;  // 1: 0x5799b0
    void Read(istream* in) override { ReadCommand(in); }  // 2: 0x5635f0
    void Write(ostream* out) override { WriteCommand(out); }  // 3: 0x563600
    void Checksum(Crc32* crc) override { ChecksumCommand(crc); }  // 4: 0x563610
    int CheckEnabled(StatusCode* status) override;  // 6: 0x579a90
    int CheckValid(StatusCode* status) override;  // 7: 0x579b00
    int Execute() override;  // 8: 0x579b70
    int GetKind() override { return 0x205; }  // 9: 0x53ded0
};

// kind 0x240: hands its request to the root member at +0xc0 (virtual slots 112/113/116)
class RootCommand240 : public Command {
public:
    RootCommand240();  // 0x57c3c0
    RootCommand240(const RootCommand240& other);  // 0x57c410
    ~RootCommand240() override;  // 0x57c400 (deleting 0x57c3e0)

    RootCommand240* Clone() override;  // 1: 0x57c430
    void Read(istream* in) override { ReadCommand(in); }  // 2: 0x5635f0
    void Write(ostream* out) override { WriteCommand(out); }  // 3: 0x563600
    void Checksum(Crc32* crc) override { ChecksumCommand(crc); }  // 4: 0x563610
    int IsChecked() override;  // 5: 0x57c520
    int CheckEnabled(StatusCode* status) override;  // 6: 0x57c5a0
    int CheckValid(StatusCode* status) override;  // 7: 0x57c660
    int Execute() override;  // 8: 0x57c720
    int GetKind() override { return 0x240; }  // 9: 0x57c4a0
};

// kind 0x242: hands its request to the root member at +0xc0 (virtual slots 114/115/117)
class RootCommand242 : public Command {
public:
    RootCommand242();  // 0x57da40
    RootCommand242(const RootCommand242& other);  // 0x57da90
    ~RootCommand242() override;  // 0x57da80 (deleting 0x57da60)

    RootCommand242* Clone() override;  // 1: 0x57dab0
    void Read(istream* in) override { ReadCommand(in); }  // 2: 0x5635f0
    void Write(ostream* out) override { WriteCommand(out); }  // 3: 0x563600
    void Checksum(Crc32* crc) override { ChecksumCommand(crc); }  // 4: 0x563610
    int CheckEnabled(StatusCode* status) override;  // 6: 0x57dba0
    int CheckValid(StatusCode* status) override;  // 7: 0x57dc60
    int Execute() override;  // 8: 0x57dd20
    int GetKind() override { return 0x242; }  // 9: 0x57db20
};

// kind 0x214: sets the destination of the units a manager carries (the commands of DestinationOrder)
class DestinationCommand : public Command {
public:
    DestinationCommand();  // 0x57ddc0
    DestinationCommand(const DestinationCommand& other);  // 0x57de80
    ~DestinationCommand() override;  // 0x57de30 (deleting 0x57de10)

    DestinationCommand* Clone() override;  // 1: 0x57dee0
    void Read(istream* in) override;  // 2: 0x57dfc0
    void Write(ostream* out) override;  // 3: 0x57dfe0
    void Checksum(Crc32* crc) override;  // 4: 0x57f430
    int CheckEnabled(StatusCode* status) override;  // 6: 0x57e000
    int CheckValid(StatusCode* status) override;  // 7: 0x57e0d0
    int Execute() override;  // 8: 0x57e380
    int GetKind() override { return 0x214; }  // 9: 0x537100
    ObjectKeyPairSet m_pairs;  // +0x50
};

// kind 0x215: renames a manufacturing manager (families 0xa0-0xaf)
class ManagerRenameCommand : public Command {
public:
    ManagerRenameCommand();  // 0x57e460
    ManagerRenameCommand(const ManagerRenameCommand& other);  // 0x57e520
    ~ManagerRenameCommand() override;  // 0x57e4d0 (deleting 0x57e4b0)

    ManagerRenameCommand* Clone() override;  // 1: 0x57e580
    void Read(istream* in) override;  // 2: 0x57e660
    void Write(ostream* out) override;  // 3: 0x57e680
    void Checksum(Crc32* crc) override { ChecksumCommand(crc); }  // 4: 0x563610
    int CheckEnabled(StatusCode* status) override;  // 6: 0x57e6a0
    int CheckValid(StatusCode* status) override;  // 7: 0x57e7a0
    int Execute() override;  // 8: 0x57e870
    int GetKind() override { return 0x215; }  // 9: 0x537c60
    String m_name;  // +0x50
};

// kind 0x213: a command on a manufacturing manager (virtual slots 115/116/126)
class ManagerCommand213 : public Command {
public:
    ManagerCommand213();  // 0x57e950
    ManagerCommand213(const ManagerCommand213& other);  // 0x57e9a0
    ~ManagerCommand213() override;  // 0x57e990 (deleting 0x57e970)

    ManagerCommand213* Clone() override;  // 1: 0x57e9c0
    void Read(istream* in) override { ReadCommand(in); }  // 2: 0x5635f0
    void Write(ostream* out) override { WriteCommand(out); }  // 3: 0x563600
    void Checksum(Crc32* crc) override { ChecksumCommand(crc); }  // 4: 0x563610
    int CheckEnabled(StatusCode* status) override;  // 6: 0x57eaa0
    int CheckValid(StatusCode* status) override;  // 7: 0x57eb70
    int Execute() override;  // 8: 0x57ec40
    int GetKind() override { return 0x213; }  // 9: 0x538fc0
};

// kind 0x216: a command on a manufacturing manager with one value (virtual slots 118/119/127)
class ManagerCommand216 : public Command {
public:
    ManagerCommand216();  // 0x57ed20
    ManagerCommand216(const ManagerCommand216& other);  // 0x57ed70
    ~ManagerCommand216() override;  // 0x57ed60 (deleting 0x57ed40)

    ManagerCommand216* Clone() override;  // 1: 0x57eda0
    void Read(istream* in) override;  // 2: 0x57ee80
    void Write(ostream* out) override;  // 3: 0x57eea0
    void Checksum(Crc32* crc) override;  // 4: 0x57eec0
    int IsChecked() override;  // 5: 0x57eef0
    int CheckEnabled(StatusCode* status) override;  // 6: 0x57efb0
    int CheckValid(StatusCode* status) override;  // 7: 0x57f080
    int Execute() override;  // 8: 0x57f150
    int GetKind() override { return 0x216; }  // 9: 0x539100
    int m_value;  // +0x50
};

// kind 0x210: orders items from a manufacturing manager; carries the {item, count} set of the order
class ManufactureCommand : public Command {
public:
    ManufactureCommand();  // 0x57f230
    ManufactureCommand(const ManufactureCommand& other);  // 0x57f2f0
    ~ManufactureCommand() override;  // 0x57f2a0 (deleting 0x57f280)

    ManufactureCommand* Clone() override;  // 1: 0x57f350
    void Read(istream* in) override;  // 2: 0x57dfc0
    void Write(ostream* out) override;  // 3: 0x57dfe0
    void Checksum(Crc32* crc) override;  // 4: 0x57f430
    int CheckEnabled(StatusCode* status) override;  // 6: 0x57f470
    int CheckValid(StatusCode* status) override;  // 7: 0x57f540
    int Execute() override;  // 8: 0x57f890
    int GetKind() override { return 0x210; }  // 9: 0x538d30
    ObjectKeyIntSet m_items;  // +0x50
};

// kind 0x221: a command on the objects of families 0x08-0x0f (virtual slots 111/112/116)
class FleetCommand221 : public Command {
public:
    FleetCommand221();  // 0x57f970
    FleetCommand221(const FleetCommand221& other);  // 0x57f9c0
    ~FleetCommand221() override;  // 0x57f9b0 (deleting 0x57f990)

    FleetCommand221* Clone() override;  // 1: 0x57f9e0
    void Read(istream* in) override { ReadCommand(in); }  // 2: 0x5635f0
    void Write(ostream* out) override { WriteCommand(out); }  // 3: 0x563600
    void Checksum(Crc32* crc) override { ChecksumCommand(crc); }  // 4: 0x563610
    int CheckEnabled(StatusCode* status) override;  // 6: 0x57fac0
    int CheckValid(StatusCode* status) override;  // 7: 0x57fb90
    int Execute() override;  // 8: 0x57fc60
    int GetKind() override { return 0x221; }  // 9: 0x582780
};

// kind 0x220: a command on the objects of families 0x08-0x0f (virtual slots 113/114/117)
class FleetCommand220 : public Command {
public:
    FleetCommand220();  // 0x57fd40
    FleetCommand220(const FleetCommand220& other);  // 0x57fd90
    ~FleetCommand220() override;  // 0x57fd80 (deleting 0x57fd60)

    FleetCommand220* Clone() override;  // 1: 0x57fdb0
    void Read(istream* in) override { ReadCommand(in); }  // 2: 0x5635f0
    void Write(ostream* out) override { WriteCommand(out); }  // 3: 0x563600
    void Checksum(Crc32* crc) override { ChecksumCommand(crc); }  // 4: 0x563610
    int CheckEnabled(StatusCode* status) override;  // 6: 0x57fe90
    int CheckValid(StatusCode* status) override;  // 7: 0x57ff60
    int Execute() override;  // 8: 0x580030
    int GetKind() override { return 0x220; }  // 9: 0x5829f0
};

// kind 0x3f0: posts a four-integer notification payload for the object it names
class PayloadEvent3f0 : public TimedEvent {
public:
    PayloadEvent3f0();  // 0x580110
    PayloadEvent3f0(const PayloadEvent3f0& other);  // 0x580180
    ~PayloadEvent3f0() override;  // 0x5801b0 (deleting 0x580130)

    PayloadEvent3f0* Clone() override;  // 1: 0x5801c0
    void Read(istream* in) override;  // 2: 0x5802b0
    void Write(ostream* out) override;  // 3: 0x5802f0
    void Checksum(Crc32* crc) override;  // 4: 0x580330
    int CheckEnabled(StatusCode* status) override;  // 6: 0x580370
    int CheckValid(StatusCode* status) override;  // 7: 0x5803e0
    int Execute() override;  // 8: 0x580450
    int GetKind() override { return 0x3f0; }  // 9: 0x580230
    PayloadEvent3f0(int valueA, int valueB, int valueC);  // 0x580150
    int m_valueA;  // +0x44
    int m_valueB;  // +0x48
    int m_valueC;  // +0x4c
};

// Factories registered in the event factory table.
MoveCommand* CreateMoveCommand();  // 0x578b90
RenameCommand* CreateRenameCommand();  // 0x579280
ScrapCommand* CreateScrapCommand();  // 0x579650
ReadyForDeleteCommand* CreateReadyForDeleteCommand();  // 0x579a20
RootCommand240* CreateRootCommand240();  // 0x57c4b0
RootCommand242* CreateRootCommand242();  // 0x57db30
DestinationCommand* CreateDestinationCommand();  // 0x57df50
ManagerRenameCommand* CreateManagerRenameCommand();  // 0x57e5f0
ManagerCommand213* CreateManagerCommand213();  // 0x57ea30
ManagerCommand216* CreateManagerCommand216();  // 0x57ee10
ManufactureCommand* CreateManufactureCommand();  // 0x57f3c0
FleetCommand221* CreateFleetCommand221();  // 0x57fa50
FleetCommand220* CreateFleetCommand220();  // 0x57fe20
PayloadEvent3f0* CreatePayloadEvent3f0();  // 0x580240
