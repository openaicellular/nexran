#ifndef _E2SM_ZYLINIUM_H_
#define _E2SM_ZYLINIUM_H_

#include <list>
#include <map>
#include <string>
#include <bitset>
#include <cstdint>

#include "e2ap.h"
#include "e2sm.h"

#define MAXMASKLEN 100

namespace e2sm
{
namespace zylinium
{

class BlockedMask
{
public:
    BlockedMask()
	: mask("0x0"),start(0.0f),end(0.0f),id(0),mask_bitset() {};
    BlockedMask(const std::string& mask_,double start_,double end_,int id_)
	: mask(mask_),start(start_),end(end_),id(id_),mask_bitset()
    {
	hex_str_to_bitset(mask_, mask_bitset);
    };

    bool hex_str_to_bitset(const std::string& hex, std::bitset<MAXMASKLEN>& bitset);
    int get_masked_bit_count(int total_available_bits)
    {
	int ret = 0;
	for (int i = 0; i < mask_bitset.size() && i < total_available_bits; ++i)
	    if (mask_bitset.test(i))
		++ret;
	return ret;
    };
    int get_available_bit_count(int total_available_bits)
    {
	int ret = 0;
	for (int i = 0; i < mask_bitset.size() && i < total_available_bits; ++i)
	    if (!mask_bitset.test(i))
		++ret;
	return ret;
    };
    bool set_mask_from_str(const std::string& hex) { mask = hex; return hex_str_to_bitset(hex, mask_bitset); };

    std::string mask;
    std::bitset<MAXMASKLEN> mask_bitset;
    double start;
    double end;
    int id;
};

class MaskConfigRequest : public e2sm::Control
{
 public:
    MaskConfigRequest(e2sm::Model *model_,
		      std::string& dl_def_,std::string& ul_def_,
		      std::list<BlockedMask>& dl_sched_,
		      std::list<BlockedMask>& ul_sched_)
	: e2sm::Control(model_),dl_def(dl_def_),ul_def(ul_def_),
	  dl_sched(dl_sched_),ul_sched(ul_sched_) {};
    virtual ~MaskConfigRequest() = default;

    virtual bool encode();

    std::string dl_def;
    std::string ul_def;
    std::list<BlockedMask> dl_sched;
    std::list<BlockedMask> ul_sched;
};

class MaskStatusRequest : public e2sm::Control
{
 public:
    MaskStatusRequest(e2sm::Model *model_)
	: e2sm::Control(model_) {};
    virtual ~MaskStatusRequest() = default;

    virtual bool encode();
};

class MaskStatusReport
{
 public:
    MaskStatusReport(BlockedMask& dl_mask_,BlockedMask& ul_mask_,
		     std::string& dl_def_,std::string& ul_def_,
		     std::list<BlockedMask>& dl_sched_,
		     std::list<BlockedMask>& ul_sched_)
	: dl_mask(dl_mask_),ul_mask(ul_mask_),dl_def(dl_def_),ul_def(ul_def_),
	  dl_sched(dl_sched_),ul_sched(ul_sched_) {};
    virtual ~MaskStatusReport() = default;
    std::string to_string(char group_delim = ' ',char item_delim = ',');

    bool encode() { return false; }

    BlockedMask dl_mask;
    BlockedMask ul_mask;
    std::string dl_def;
    std::string ul_def;
    std::list<BlockedMask> dl_sched;
    std::list<BlockedMask> ul_sched;
};

class MaskStatusIndication : public e2sm::Indication
{
 public:
    MaskStatusIndication(e2sm::Model *model_)
	: e2sm::Indication(model_) {};
    MaskStatusIndication(e2sm::Model *model_,MaskStatusReport *report_)
	: report(report_),e2sm::Indication(model_) {};
    virtual ~MaskStatusIndication() { delete report; };
    virtual bool encode() { return false; };

    MaskStatusReport *report;
};

class MaskStatusControlOutcome : public e2sm::ControlOutcome
{
 public:
    MaskStatusControlOutcome(e2sm::Model *model_,MaskStatusReport *report_)
	: e2sm::ControlOutcome(model_),report(report_) {};
    virtual ~MaskStatusControlOutcome() = default;

    bool encode() { return false; };

 private:
    MaskStatusReport *report;
};

class EventTrigger : public e2sm::EventTrigger
{
 public:
    EventTrigger(e2sm::Model *model_)
	: e2sm::EventTrigger(model_) {};
    virtual ~EventTrigger() = default;

    virtual bool encode();
};

class AgentInterface
{
 public:
    virtual bool handle(e2sm::zylinium::MaskStatusIndication *ind) = 0;
};

class ZyliniumModel : public e2sm::Model
{
 public:
    ZyliniumModel(AgentInterface *agent_if_)
	: agent_if(agent_if_),e2sm::Model("ORAN-E2SM-ZYLINIUM","1.3.6.1.4.1.1.1.2.999") {};
    virtual ~ZyliniumModel() = default;
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

#endif /* _E2SM_ZYLINIUM_H_ */
