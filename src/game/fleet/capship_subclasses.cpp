#include "fleet/capship_subclasses.h"


namespace {
const FamilyRange kCapitalShipFamilies(0x14, 0x18);
const FamilyRange kDeathStarFamilies(0x18, 0x1c);

int applyAllOnSideView(CapShip* ship, NotifyContext* ctx) {
    return ship->IsSideView() ? ship->CapShip::ApplyAll(ctx) : 0;
}
}  // namespace

CapitalShip::CapitalShip() {}

CapitalShip::~CapitalShip() {  }

int CapitalShip::CreateViews(int controlKind, NotifyContext* ctx) { return CapShip::CreateViews(controlKind, ctx); }

GameObj* CapitalShip::CreateViewObject() { return new CapitalShip(); }

int CapitalShip::Read(istream* in) { return CapShip::Read(in); }

int CapitalShip::Write(ostream* out) { return CapShip::Write(out); }

int CapitalShip::Checksum(Crc32* crc) { return CapShip::Checksum(crc); }

int CapitalShip::ApplyAll(NotifyContext* ctx) { return applyAllOnSideView(this, ctx); }

DeathStar::DeathStar() {}

DeathStar::~DeathStar() {  }

int DeathStar::CreateViews(int controlKind, NotifyContext* ctx) { return CapShip::CreateViews(controlKind, ctx); }

GameObj* DeathStar::CreateViewObject() { return new DeathStar(); }

int DeathStar::Read(istream* in) { return CapShip::Read(in); }

int DeathStar::Write(ostream* out) { return CapShip::Write(out); }

int DeathStar::Checksum(Crc32* crc) { return CapShip::Checksum(crc); }

int DeathStar::ApplyAll(NotifyContext* ctx) { return applyAllOnSideView(this, ctx); }

GameObjNotifier* CreateCapitalShip() { return new CapitalShip(); }

GameObjNotifier* CreateDeathStar() { return new DeathStar(); }

CapitalShipIterator::CapitalShipIterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &kCapitalShipFamilies, stateFilter) {}

CapitalShipIterator::~CapitalShipIterator() {  }

DeathStarIterator::DeathStarIterator(GameObjNotifier* parent, uint32_t stateFilter)
    : ChildIterator(parent, &kDeathStarFamilies, stateFilter) {}

DeathStarIterator::DeathStarIterator(GameObjNotifier* parent, uint32_t stateFilter, uint32_t side)
    : ChildIterator(parent, side, &kDeathStarFamilies, stateFilter) {}

DeathStarIterator::~DeathStarIterator() {  }
