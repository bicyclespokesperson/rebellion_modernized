#include "events/notif_event_payloads.h"


// ---- KeyPairPayload ---------------------------------------------------------------------------------------

KeyPairPayload::KeyPairPayload() : KeyedNotifPayload(), m_keyA(), m_keyB() {}

KeyPairPayload::~KeyPairPayload() = default;

// ---- IntPairPayload ---------------------------------------------------------------------------------------

IntPairPayload::IntPairPayload() : KeyedNotifPayload(), m_valueA(0), m_valueB(0) {}

IntPairPayload::~IntPairPayload() = default;

int IntPairPayload::ReadPayload(istream* in) {
    int result = ReadKeyed(in);
    if (result) {
        ReadValue32(in, &m_valueA);
        ReadValue32(in, &m_valueB);
    }
    return result;
}

int IntPairPayload::WritePayload(ostream* out) {
    int result = WriteKeyed(out);
    if (result) {
        WriteValue32(out, &m_valueA);
        WriteValue32(out, &m_valueB);
    }
    return result;
}

// ---- IntQuadPayload ---------------------------------------------------------------------------------------

IntQuadPayload::IntQuadPayload() : KeyedNotifPayload(), m_valueA(0), m_valueB(0), m_valueC(0), m_valueD(0) {}

IntQuadPayload::~IntQuadPayload() = default;

int IntQuadPayload::ReadPayload(istream* in) {
    int result = ReadKeyed(in);
    if (result) {
        ReadValue32(in, &m_valueA);
        ReadValue32(in, &m_valueB);
        ReadValue32(in, &m_valueC);
        ReadValue32(in, &m_valueD);
    }
    return result;
}

int IntQuadPayload::WritePayload(ostream* out) {
    int result = WriteKeyed(out);
    if (result) {
        WriteValue32(out, &m_valueA);
        WriteValue32(out, &m_valueB);
        WriteValue32(out, &m_valueC);
        WriteValue32(out, &m_valueD);
    }
    return result;
}

// ---- StringPairPayload ------------------------------------------------------------------------------------

StringPairPayload::StringPairPayload() : KeyedNotifPayload(), m_textA(), m_textB() {}

StringPairPayload::~StringPairPayload() = default;

int StringPairPayload::ReadPayload(istream* in) {
    int result = ReadKeyed(in);
    if (result) {
        ReadLegacyString(in, m_textA);
        ReadLegacyString(in, m_textB);
    }
    return result;
}

int StringPairPayload::WritePayload(ostream* out) {
    int result = WriteKeyed(out);
    if (result) {
        WriteLegacyString(out, m_textA);
        WriteLegacyString(out, m_textB);
    }
    return result;
}

// ---- NullableKeyPairPayload -------------------------------------------------------------------------------

NullableKeyPairPayload::NullableKeyPairPayload() : KeyedNotifPayload() {
    m_keyA.m_value = 0;
    m_keyB.m_value = 0;
}

NullableKeyPairPayload::~NullableKeyPairPayload() = default;

int NullableKeyPairPayload::ReadPayload(istream* in) {
    int result = ReadKeyed(in);
    if (result) {
        reinterpret_cast<ObjectKey*>(&m_keyA)->Read(in);
        reinterpret_cast<ObjectKey*>(&m_keyB)->Read(in);
    }
    return result;
}

int NullableKeyPairPayload::WritePayload(ostream* out) {
    int result = WriteKeyed(out);
    if (result) {
        reinterpret_cast<ObjectKey*>(&m_keyA)->Write(out);
        reinterpret_cast<ObjectKey*>(&m_keyB)->Write(out);
    }
    return result;
}

// ---- ChecksumRecord ---------------------------------------------------------------------------------------

ChecksumRecord::ChecksumRecord() : m_value00(0), m_value04(0), m_value08(0), m_crcA(), m_crcB() {}

ChecksumRecord::~ChecksumRecord() = default;

ChecksumRecord& ChecksumRecord::operator=(const ChecksumRecord& other) = default;

int ChecksumRecord::Load(istream* in) {
    StreamRead32(in, &m_value00);
    StreamRead32(in, &m_value04);
    StreamRead32(in, &m_value08);
    m_crcA.LoadState(in);
    m_crcB.LoadState(in);
    return 1;
}

int ChecksumRecord::Save(ostream* out) {
    StreamWrite32(out, &m_value00);
    StreamWrite32(out, &m_value04);
    StreamWrite32(out, &m_value08);
    m_crcA.SaveState(out);
    m_crcB.SaveState(out);
    return 1;
}

int ChecksumRecord::Read(istream* in) { return Load(in); }

int ChecksumRecord::Write(ostream* out) { return Save(out); }
