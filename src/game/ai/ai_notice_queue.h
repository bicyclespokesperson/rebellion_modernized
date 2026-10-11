#pragma once

#include "ai/ai_task_base.h"
#include "core/containers.h"
#include "core/stream_io.h"

// The notes the AI player was sent, in AiNoticeGroups by group id. The tree holds the IdNode part of the groups.
class AiNoticeQueue : public IdAvlTree {
public:
    AiNoticeQueue();
    ~AiNoticeQueue() override;
    void Load(istream* in);
    void Save(ostream* out);
    void Add(NotifPayload* notice);
    AiNoticeGroup* DetachFinished();
    void NewGroup(NotifPayload* notice);

    int m_x0c;
};
