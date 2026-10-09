#include "events/notif_event_payloads.h"

#include "core/vptr_store.h"

// ---- KeyPairPayload ---------------------------------------------------------------------------------------

KeyPairPayload::KeyPairPayload() : KeyedNotifPayload(), m_keyA(), m_keyB() {}

KeyPairPayload::~KeyPairPayload() { REB_KEEP_VPTR_STORE(); }

// ---- IntPairPayload ---------------------------------------------------------------------------------------

IntPairPayload::IntPairPayload() : KeyedNotifPayload(), m_valueA(0), m_valueB(0) {}

IntPairPayload::~IntPairPayload() { REB_KEEP_VPTR_STORE(); }

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

IntQuadPayload::~IntQuadPayload() { REB_KEEP_VPTR_STORE(); }

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

StringPairPayload::~StringPairPayload() { REB_KEEP_VPTR_STORE(); }

int StringPairPayload::ReadPayload(istream* in) {
    int result = ReadKeyed(in);
    if (result) {
        m_textA.Read(in);
        m_textB.Read(in);
    }
    return result;
}

int StringPairPayload::WritePayload(ostream* out) {
    int result = WriteKeyed(out);
    if (result) {
        m_textA.Write(out);
        m_textB.Write(out);
    }
    return result;
}

// ---- NullableKeyPairPayload -------------------------------------------------------------------------------

NullableKeyPairPayload::NullableKeyPairPayload() : KeyedNotifPayload() {
    m_keyA.m_value = 0;
    m_keyB.m_value = 0;
}

NullableKeyPairPayload::~NullableKeyPairPayload() { REB_KEEP_VPTR_STORE(); }

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

ChecksumRecord::~ChecksumRecord() {}

ChecksumRecord& ChecksumRecord::operator=(const ChecksumRecord& other) {
    m_value00 = other.m_value00;
    m_value04 = other.m_value04;
    m_value08 = other.m_value08;
    m_crcA = other.m_crcA;
    m_crcB = other.m_crcB;
    return *this;
}

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
