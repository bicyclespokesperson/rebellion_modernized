#include "gameobj/game_obj.h"
#include "gameobj/object_type_db.h"
#include "orders/order_checks.h"

// Notification logging is disabled.

int GameObj::LogChange(const char* tag, const char* property, const char* newText, const char* oldText,
                       const char* extra, NotifyContext* ctx)
{
  return 1;
}

int GameObj::LogFlag(const char* tag, const char* property, int value, intptr_t unused, NotifyContext* ctx)
{
  return 1;
}

int GameObj::LogSide(const char* tag, const char* property, uint32_t newSide, uint32_t oldSide, intptr_t unused,
                     NotifyContext* ctx)
{
  return 1;
}

int GameObj::LogInt(const char* tag, const char* property, int newValue, int oldValue, intptr_t unused,
                    NotifyContext* ctx)
{
  return 1;
}

int GameObj::LogString(const char* tag, const char* property, std::string* newValue, std::string* oldValue,
                       intptr_t unused, NotifyContext* ctx)
{
  return 1;
}

int GameObj::LogKey(const char* tag, const char* property, ObjectKey* newKey, ObjectKey* oldKey, intptr_t unused,
                    NotifyContext* ctx)
{
  return 1;
}

int GameObj::LogTypeKey(const char* tag, const char* property, ObjectKey* newKey, ObjectKey* oldKey, intptr_t unused,
                        NotifyContext* ctx)
{
  return 1;
}
