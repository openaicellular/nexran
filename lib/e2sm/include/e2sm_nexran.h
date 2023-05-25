#ifndef _E2SM_NEXRAN_H_
#define _E2SM_NEXRAN_H_

#include <list>
#include <map>
#include <string>
#include <cstdint>

#include "e2ap.h"
#include "e2sm.h"

namespace e2sm
{
namespace nexran
{

class ProportionalAllocationPolicy
{
 public:
    ProportionalAllocationPolicy()
	: share(1024) {};
    ProportionalAllocationPolicy(int share_)
	: share(share_) {};
    virtual ~ProportionalAllocationPolicy() = default;

    int share;
};

class SliceConfig
{
 public:
    SliceConfig(std::string &name_,ProportionalAllocationPolicy *policy_)
	: name(name_),policy(policy_) {};
    virtual ~SliceConfig() = default;

    std::string name;
    ProportionalAllocationPolicy *policy;
};

class UeStatus
{
 public:
    UeStatus(std::string &imsi_,bool connected_,std::string &crnti_)
	: imsi(imsi_),connected(connected_),crnti(crnti_) {};
    virtual ~UeStatus() = default;

    std::string imsi;
    bool connected;
    std::string crnti;
};

class SliceStatus
{
 public:
    SliceStatus(std::string &name_,ProportionalAllocationPolicy *policy_,
		std::list<UeStatus *> &ue_list_)
	: name(name_),policy(policy_),ue_list(ue_list_) {};
    virtual ~SliceStatus() = default;

    std::string name;
    ProportionalAllocationPolicy *policy;
    std::list<UeStatus *> ue_list;
};

class SliceStatusIndication : public e2sm::Indication
{
 public:
    SliceStatusIndication(e2sm::Model *model_)
	: status(NULL),e2sm::Indication(model_) {};
    SliceStatusIndication(e2sm::Model *model_,SliceStatus *status_)
	: status(status_),e2sm::Indication(model_) {};
    virtual ~SliceStatusIndication() { if (status) delete status; };

    SliceStatus *status;
};

class SliceConfigRequest : public e2sm::Control
{
 public:
    SliceConfigRequest(e2sm::Model *model_,SliceConfig *slice_config)
	: e2sm::Control(model_) { configs.push_back(slice_config); };
    SliceConfigRequest(e2sm::Model *model_,std::list<SliceConfig *> &configs_)
	: configs(configs_),e2sm::Control(model_) {};
    virtual ~SliceConfigRequest() = default;

    virtual bool encode();

 private:
    std::list<SliceConfig *> configs;
};

class SliceDeleteRequest : public e2sm::Control
{
 public:
    SliceDeleteRequest(e2sm::Model *model_,std::string &name)
	: e2sm::Control(model_) { names.push_back(name); };
    SliceDeleteRequest(e2sm::Model *model_,std::list<std::string> &names_)
	: names(names_),e2sm::Control(model_) {};
    virtual ~SliceDeleteRequest() = default;

    virtual bool encode();

 private:
    std::list<std::string> names;
};

class SliceUeBindRequest : public e2sm::Control
{
 public:
    SliceUeBindRequest(e2sm::Model *model_,std::string &slice_,std::string imsi_)
        : slice(slice_),e2sm::Control(model_) { imsis.push_back(imsi_); };
    SliceUeBindRequest(e2sm::Model *model_,std::string &slice_,std::list<std::string> imsis_)
        : slice(slice_),imsis(imsis_),e2sm::Control(model_) {};
    virtual ~SliceUeBindRequest() = default;

    virtual bool encode();

 private:
    std::string slice;
    std::list<std::string> imsis;
};

class SliceUeUnbindRequest : public e2sm::Control
{
 public:
    SliceUeUnbindRequest(e2sm::Model *model_,std::string &slice_,std::string imsi_)
        : slice(slice_),e2sm::Control(model_) { imsis.push_back(imsi_); };
    SliceUeUnbindRequest(e2sm::Model *model_,std::string &slice_,std::list<std::string> imsis_)
        : slice(slice_),imsis(imsis_),e2sm::Control(model_) {};
    virtual ~SliceUeUnbindRequest() = default;

    virtual bool encode();

 private:
    std::string slice;
    std::list<std::string> imsis;
};

class SliceStatusRequest : public e2sm::Control
{
 public:
    SliceStatusRequest(e2sm::Model *model_)
	: e2sm::Control(model_) {};
    SliceStatusRequest(e2sm::Model *model_,std::string &name)
	: e2sm::Control(model_) { names.push_back(name); };
    SliceStatusRequest(e2sm::Model *model_,std::list<std::string> &names_)
	: names(names_),e2sm::Control(model_) {};
    virtual ~SliceStatusRequest() = default;

    virtual bool encode();

 private:
    std::list<std::string> names;
};

class SliceStatusReport : public e2sm::Indication
{
 public:
    SliceStatusReport(e2sm::Model *model_,SliceStatus *status)
	: e2sm::Indication(model_) { statuses.push_back(status); };
    SliceStatusReport(e2sm::Model *model_,std::list<SliceStatus *> &statuses_)
	: statuses(statuses_),e2sm::Indication(model_) {};
    virtual ~SliceStatusReport() = default;

    bool encode() { return false; }

 private:
    std::list<SliceStatus *> statuses;
};

class SliceStatusControlOutcome : public e2sm::ControlOutcome
{
 public:
    SliceStatusControlOutcome(e2sm::Model *model_,SliceStatus *status)
	: e2sm::ControlOutcome(model_) { statuses.push_back(status); };
    SliceStatusControlOutcome(e2sm::Model *model_,std::list<SliceStatus *> &statuses_)
	: statuses(statuses_),e2sm::ControlOutcome(model_) {};
    virtual ~SliceStatusControlOutcome() = default;

    bool encode() { return false; };

 private:
    std::list<SliceStatus *> statuses;
};

class EventTrigger : public e2sm::EventTrigger
{
 public:
    EventTrigger(e2sm::Model *model_,long period_)
	: period(period_),on_events(false),e2sm::EventTrigger(model_) {};
    EventTrigger(e2sm::Model *model_)
	: period(-1),on_events(true),e2sm::EventTrigger(model_) {};
    virtual ~EventTrigger() = default;

    virtual bool encode();

 protected:
    long period;
    bool on_events;
};

class AgentInterface
{
 public:
    virtual bool handle(e2sm::nexran::SliceStatusIndication *ind) = 0;
};

class NexRANModel : public e2sm::Model
{
 public:
    NexRANModel(AgentInterface *agent_if_)
	: agent_if(agent_if_),e2sm::Model("ORAN-E2SM-NEXRAN","1.3.6.1.4.1.1.1.2.100") {};
    virtual ~NexRANModel() = default;
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

#endif /* _E2SM_NEXRAN_H_ */
