#ifndef _E2SM_KPM_H_
#define _E2SM_KPM_H_

#include <list>
#include <map>
#include <string>
#include <cstdint>

#include "e2ap.h"
#include "e2sm.h"

namespace e2sm
{
namespace kpm
{

class KpmIndication : e2sm::Indication
{
 public:
    KpmIndication(e2sm::Model *model_)
	: e2sm::Indication(model_) {};
    virtual ~KpmIndication() = default;

    //xxx
};

class EventTrigger : public e2sm::EventTrigger
{
 public:
    EventTrigger(e2sm::Model *model_,long period_)
	: period(period_),e2sm::EventTrigger(model_) {};
    EventTrigger(e2sm::Model *model_)
	: period(14),e2sm::EventTrigger(model_) {};
    virtual ~EventTrigger() = default;

    virtual bool encode();

 protected:
    long period;
};

class AgentInterface
{
 public:
    virtual bool handle(e2sm::kpm::KpmIndication *ind) = 0;
};

class KpmModel : public e2sm::Model
{
 public:
    KpmModel(AgentInterface *agent_if_)
	: agent_if(agent_if_),e2sm::Model("ORAN-E2SM-KPM","1.3.6.1.4.1.1.1.2.2") {};
    virtual ~KpmModel() = default;
    virtual int init() { return 0; };
    virtual void stop() {};

    Indication *decode(e2ap::Indication *ind,
		       unsigned char *header,ssize_t header_len,
		       unsigned char *message,ssize_t message_len);
    ControlOutcome *decode(e2ap::ControlAck *ack,
			   unsigned char *outcome,ssize_t outcome_len);
    ControlOutcome *decode(e2ap::ControlFailure *failure,
			   unsigned char *outcome,ssize_t outcome_len);

 protected:
    AgentInterface *agent_if;
};

}
}

#endif /* _E2SM_KPM_H_ */
