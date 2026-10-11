#include "tactical/tactical_weapon_profile.h"

namespace {

constexpr float kDamageFactor = 0.26666668f;
constexpr float kHalf = 0.5f;
constexpr float kCapitalFactorB = 0.167f;

}  // namespace

WeaponProfile::WeaponProfile() {
    m_f04 = kDamageFactor;
    m_f0c = 1.0f;
    m_f08 = 1.0f;
    m_f10 = 1.0f;
}

WeaponProfile::~WeaponProfile() {  }

WeaponProfileA::WeaponProfileA() { m_f08 = kHalf; }

WeaponProfileA::~WeaponProfileA() {  }

WeaponProfileB::WeaponProfileB() { m_f0c = kCapitalFactorB; }

WeaponProfileB::~WeaponProfileB() {  }

WeaponProfileC::WeaponProfileC() { m_f08 = kHalf; }

WeaponProfileC::~WeaponProfileC() {  }
