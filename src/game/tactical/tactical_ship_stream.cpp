#include "tactical/tactical_externals.h"
#include "tactical/tactical_groups.h"
#include "tactical/tactical_objmgr.h"
#include "tactical/tactical_registry.h"
#include "tactical/tactical_ship.h"

// Save / network-sync serialization of the battle units: every unit writes, reads and checksums the same fields in the
// same order (0x5b6a00 / 0x5b6eb0 / 0x5b7370 and the carrier parts).

namespace {

template <typename T>
int bitsOf(const T& value) {
    int bits;
    __builtin_memcpy(&bits, &value, sizeof bits);
    return bits;
}

struct Reader {
    istream* m_in;
    template <typename T>
    void operator()(T& field) { ReadValue32(m_in, &field); }
    void arc(TacticalWeaponArc& arc) { arc.Read(m_in); }
};

struct Writer {
    ostream* m_out;
    template <typename T>
    void operator()(T& field) { WriteValue32(m_out, &field); }
    void arc(TacticalWeaponArc& arc) { arc.Write(m_out); }
};

struct Summer {
    Crc32* m_crc;
    template <typename T>
    void operator()(T& field) { m_crc->AddInt(bitsOf(field)); }
    void arc(TacticalWeaponArc& arc) { arc.Checksum(m_crc); }
};

// The fields every unit persists between the string and the weapon arcs.
template <typename Visitor>
void visitLeadingFields(TacticalShipBase& ship, Visitor& v) {
    v(ship.m_fA_04);
    v(ship.m_fA_08);
    v(ship.m_fB_04);
    v(ship.m_fC_04);
    v(ship.m_fD_04);
    v(ship.m_fD_08);
    v(ship.m_objectFamily);
    v(ship.m_objectId);
    v(ship.m_chargingCount);
    v(ship.m_totalTorpedoes);
    v(ship.m_torpedoRange);
    v(ship.m_typeIndex);
    v(ship.m_unitKind);
}

template <typename Visitor>
void visitMiddleFields(TacticalShipBase& ship, Visitor& v) {
    v(ship.m_hasGravityWell);
    v(ship.m_unk154);
    v(ship.m_hitCount);
    v(ship.m_unk15c);
    v(ship.m_unk160);
    v(ship.m_carrierHandle);
    v(ship.m_side);
    v(ship.m_hyperdriveReady);
    v(ship.m_state);
    v(ship.m_groupHandle);
    v(ship.m_unk178);
    for (unsigned i = 0; i < 4; ++i) {
        v.arc(ship.m_arcs[i]);
        v(ship.m_chargeQueue[i]);
    }
}

template <typename Visitor>
void visitTrailingFields(TacticalShipBase& ship, Visitor& v) {
    v(ship.m_unk35c);
    v(ship.m_speedCH);
    v(ship.m_shieldCH);
    v(ship.m_hyperdriveCH);
    v(ship.m_tractorCH);
    v(ship.m_weaponCH);
    v(ship.m_tractorTarget);
    for (unsigned i = 0; i < 4; ++i) v(ship.m_tractoredBy[i]);
    v(ship.m_value394);
    v(ship.m_value398);
    v(ship.m_tractorRange);
    v(ship.m_value3a0);
    v(ship.m_unk3a4);
    v(ship.m_unk3a8);
    v(ship.m_unk3ac);
    v(ship.m_unk3b0);
    v(ship.m_unk3b4);
    v(ship.m_tractorPower);
    v(ship.m_shieldMax);
    v(ship.m_shield);
    v(ship.m_hullMax);
    v(ship.m_hull);
    v(ship.m_firepowerTotal);
    v(ship.m_unk3d8);
    v(ship.m_unk3dc);
    v(ship.m_unk3e0);
    v(ship.m_weaponRecharge);
    v(ship.m_shieldRecharge);
    v(ship.m_hyperdriveCount);
    v(ship.m_unk3f0);
    v(ship.m_unk3f4);
    v(ship.m_speed);
    v(ship.m_commanderBonus);
    v(ship.m_unk400);
    v(ship.m_unk404);
    v(ship.m_unk408);
    v(ship.m_unk40c);
    v(ship.m_unk414);
    v(ship.m_unk410);
}

}  // namespace

// ---- weapon arc ---------------------------------------------------------------------------------------------------

void TacticalWeaponArc::Read(istream* in) {
    ReadValue32(in, &m_countA);
    ReadValue32(in, &m_countB);
    ReadValue32(in, &m_countC);
    m_damageA = 0;
    m_damageB = 0;
    m_damageC = 0;
    m_profileA.Read(in);
    m_profileB.Read(in);
    m_profileC.Read(in);
    m_unk54 = 0;
    m_unk58 = 0;
    m_unk5c = 0;
    ReadValue32(in, &m_unk60);
    ReadValue32(in, &m_chargeState);
    ReadValue32(in, &m_charge);
    ReadValue32(in, &m_chargeMax);
    ReadValue32(in, &m_firepower);
}

void TacticalWeaponArc::Write(ostream* out) {
    WriteValue32(out, &m_countA);
    WriteValue32(out, &m_countB);
    WriteValue32(out, &m_countC);
    m_profileA.Write(out);
    m_profileB.Write(out);
    m_profileC.Write(out);
    WriteValue32(out, &m_unk60);
    WriteValue32(out, &m_chargeState);
    WriteValue32(out, &m_charge);
    WriteValue32(out, &m_chargeMax);
    WriteValue32(out, &m_firepower);
}

void TacticalWeaponArc::Checksum(Crc32* crc) {
    crc->AddInt(m_countA);
    crc->AddInt(m_countB);
    crc->AddInt(m_countC);
    m_profileA.Checksum(crc);
    m_profileB.Checksum(crc);
    m_profileC.Checksum(crc);
    crc->AddInt(m_unk60);
    crc->AddInt(m_chargeState);
    crc->AddInt(bitsOf(m_charge));
    crc->AddInt(bitsOf(m_chargeMax));
    crc->AddInt(bitsOf(m_firepower));
}

// ---- ship ---------------------------------------------------------------------------------------------------------

void TacticalShipBase::WriteState(ostream* out) {
    Writer writer{out};
    MovingObject::Write(out);
    int id = static_cast<int>(m_id);
    WriteValue32(out, &id);
    visitLeadingFields(*this, writer);
    m_name.Write(out);
    visitMiddleFields(*this, writer);
    visitTrailingFields(*this, writer);
}

void TacticalShipBase::ReadState(istream* in) {
    Reader reader{in};
    MovingObject::Read(in);
    uint32_t id = 0;
    ReadValue32(in, &id);
    TacticalUnit::SetId(id);
    visitLeadingFields(*this, reader);
    m_name.Read(in);
    visitMiddleFields(*this, reader);
    visitTrailingFields(*this, reader);
}

void TacticalShipBase::ChecksumState(Crc32* crc) {
    Summer summer{crc};
    MovingObject::Checksum(crc);
    crc->AddInt(static_cast<int>(m_id));
    visitLeadingFields(*this, summer);
    visitMiddleFields(*this, summer);
    visitTrailingFields(*this, summer);
}

void TacticalShipBase::WriteExtra(ostream* out) { m_unk360.Write(out); }

void TacticalShipBase::ReadExtra(istream* in) { m_unk360.Read(in); }

void TacticalShipBase::ChecksumExtra(Crc32* crc) { m_unk360.Checksum(crc); }

// ---- carrier --------------------------------------------------------------------------------------------------------

void TacticalHangar::Write(ostream* out) {
    WriteValue32(out, &m_ownerHandle);
    WriteValue32(out, &m_inFlight);
    WriteValue32(out, &m_aboard);
    WriteValue32(out, &m_capacity);
    WriteValue32(out, &m_launchTime);
    WriteValue32(out, &m_launchInterval);
    m_docked->Write(out);
    m_launched->Write(out);
    m_unk28->Write(out);
}

void TacticalHangar::Read(istream* in) {
    ReadValue32(in, &m_ownerHandle);
    ReadValue32(in, &m_inFlight);
    ReadValue32(in, &m_aboard);
    ReadValue32(in, &m_capacity);
    ReadValue32(in, &m_launchTime);
    ReadValue32(in, &m_launchInterval);
    m_docked->Read(in);
    m_launched->Read(in);
    m_unk28->Read(in);
}

void TacticalHangar::Checksum(Crc32* crc) {
    crc->AddInt(m_ownerHandle);
    crc->AddInt(m_inFlight);
    crc->AddInt(m_aboard);
    crc->AddInt(m_capacity);
    crc->AddInt(bitsOf(m_launchTime));
    crc->AddInt(bitsOf(m_launchInterval));
    m_docked->Checksum(crc);
    m_launched->Checksum(crc);
    m_unk28->Checksum(crc);
}

void TacticalCapitalShip::WriteState(ostream* out) { TacticalShipBase::WriteState(out); }

void TacticalCapitalShip::WriteExtra(ostream* out) {
    TacticalShipBase::WriteExtra(out);
    TacticalHangar::Write(out);
}

void TacticalCapitalShip::ReadState(istream* in) { TacticalShipBase::ReadState(in); }

void TacticalCapitalShip::ReadExtra(istream* in) {
    TacticalShipBase::ReadExtra(in);
    TacticalHangar::Read(in);
}

void TacticalCapitalShip::ChecksumState(Crc32* crc) { TacticalShipBase::ChecksumState(crc); }

void TacticalCapitalShip::ChecksumExtra(Crc32* crc) {
    TacticalShipBase::ChecksumExtra(crc);
    TacticalHangar::Checksum(crc);
}
