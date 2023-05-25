#ifndef _E2AP_H_
#define _E2AP_H_

#include <list>
#include <tuple>
#include <mutex>
#include <cstdint>
#include <string>
#include <cstdlib>
#include <ctime>
#include <map>
#include <memory>

#define E2AP_XER_PRINT(stream,type,pdu)					\
    do {								\
	if (e2ap::xer_print)						\
	    xer_fprint((stream == NULL) ? stderr : stream,type,pdu);	\
    } while (0);

namespace e2sm {

class Model;
class Control;
class ControlOutcome;
class Indication;
class EventTrigger;
class ActionDefinition;

}

namespace e2ap {

extern bool xer_print;

/**
 * These are local function IDs.  Each service model might expose many
 * functions.  E2SM functions do not currently have global IDs, unless
 * you concat the E2SM OID and the function name.  There is no
 * requirement that function IDs be the same for different E2Setup/Reset
 * sessions, so we allow e2sm modules to register functions.
 */
typedef long RanFunctionId;

class RanFunction {
 public:
    RanFunctionId function_id;
    long revision;
    std::string name;
    std::string description;
    uint8_t *enc_definition;
    ssize_t enc_definition_len;
    int enabled;
    void *definition;
    e2sm::Model *model;
};

RanFunctionId get_next_ran_function_id();

class Message
{
 public:
    Message() : encoded(false),buf(NULL),len(0) {};
    virtual ~Message() {
	if (buf)
	    free(buf);
    }
    virtual bool encode() = 0;
    virtual unsigned char *get_buf() { if (!encoded) encode(); return buf; };
    virtual ssize_t get_len() { if (!encoded) encode(); return len; };

 protected:
    bool encoded;
    unsigned char *buf;
    ssize_t len;
};

enum ActionType {
    ACTION_REPORT = 0,
    ACTION_INSERT = 1,
    ACTION_POLICY = 2,
    __ACTION_END__ = 3,
};

class Action
{
 public:
    Action(
	long id_,ActionType type_,e2sm::ActionDefinition *definition_,
	long subsequent_action_)
	: id(id_),type(type_),definition(definition_),
	  subsequent_action(subsequent_action_) {};
    virtual ~Action() = default;

    long id;
    ActionType type;
    e2sm::ActionDefinition *definition;
    long subsequent_action;

    long time_to_wait;
    bool enabled;
    long error_cause;
    long error_cause_detail;
};

class Request : public Message
{
public:
    Request(long requestor_id_,long instance_id_)
	: requestor_id(requestor_id_),instance_id(instance_id_) {};
    Request()
	: requestor_id(-1),instance_id(-1) {};

    void set_meid(std::string &meid_) { meid = meid_; };

    long requestor_id;
    long instance_id;
    std::string meid;
};

class SubscriptionRequest : public Request
{
 public:
    SubscriptionRequest(
        long requestor_id_,long instance_id_,RanFunctionId function_id_,
	e2sm::EventTrigger *trigger_,std::list<Action *>& actions_)
	: function_id(function_id_),trigger(trigger_),actions(actions_),
	  Request(requestor_id_,instance_id_) {};
    virtual ~SubscriptionRequest() = default;

    virtual bool encode();

    RanFunctionId function_id;
    e2sm::EventTrigger *trigger;
    std::list<Action *> actions;
};

class SubscriptionResponse : public Message
{
 public:
    SubscriptionResponse()
	: requestor_id(-1),instance_id(-1),function_id(-1),
	  Message() {};
    SubscriptionResponse(
        long requestor_id_,long instance_id_,RanFunctionId function_id_,
	std::list<long> actions_admitted_,
	std::list<std::tuple<long,long,long>> actions_not_admitted_)
	: requestor_id(requestor_id_),instance_id(instance_id_),
	  function_id(function_id_),actions_admitted(actions_admitted_),
	  actions_not_admitted(actions_not_admitted_),Message() {};
    virtual ~SubscriptionResponse() {
	actions_admitted.clear();
	actions_not_admitted.clear();
    }

    virtual bool encode();

    long requestor_id;
    long instance_id;
    RanFunctionId function_id;
    std::list<long> actions_admitted;
    std::list<std::tuple<long,long,long>> actions_not_admitted;

    std::shared_ptr<SubscriptionRequest> req;
};

class SubscriptionFailure : public Message
{
 public:
    SubscriptionFailure()
	: requestor_id(-1),instance_id(-1),function_id(-1),
	  Message() {};
    SubscriptionFailure(
        long requestor_id_,long instance_id_,RanFunctionId function_id_,
	std::list<std::tuple<long,long,long>> actions_not_admitted_)
	: requestor_id(requestor_id_),instance_id(instance_id_),
	  function_id(function_id_),
	  actions_not_admitted(actions_not_admitted_),
	  Message() {};
    virtual ~SubscriptionFailure() {
	actions_not_admitted.clear();
    }

    virtual bool encode();

    long requestor_id;
    long instance_id;
    RanFunctionId function_id;
    std::list<std::tuple<long,long,long>> actions_not_admitted;

    std::shared_ptr<SubscriptionRequest> req;
};

class SubscriptionDeleteRequest : public Request
{
 public:
    SubscriptionDeleteRequest(
        long requestor_id_,long instance_id_,RanFunctionId function_id_)
	: function_id(function_id_),Request(requestor_id_,instance_id_) {};
    virtual ~SubscriptionDeleteRequest() = default;

    virtual bool encode();

    RanFunctionId function_id;
};

class SubscriptionDeleteResponse : public Message
{
 public:
    SubscriptionDeleteResponse()
	: requestor_id(-1),instance_id(-1),function_id(-1),Message() {};
    SubscriptionDeleteResponse(
        long requestor_id_,long instance_id_,RanFunctionId function_id_)
	: requestor_id(requestor_id_),instance_id(instance_id_),
	  function_id(function_id_),Message() {};
    virtual ~SubscriptionDeleteResponse() = default;

    virtual bool encode();

    long requestor_id;
    long instance_id;
    RanFunctionId function_id;

    std::shared_ptr<SubscriptionDeleteRequest> req;
};

class SubscriptionDeleteFailure : public Message
{
 public:
    SubscriptionDeleteFailure()
	: requestor_id(-1),instance_id(-1),function_id(-1),
	  cause(-1),cause_detail(-1),Message() {};
    SubscriptionDeleteFailure(
        long requestor_id_,long instance_id_,RanFunctionId function_id_,
	long cause_,long cause_detail_)
	: requestor_id(requestor_id_),instance_id(instance_id_),
	  function_id(function_id_),cause(cause_),cause_detail(cause_detail_),
	  Message() {};
    virtual ~SubscriptionDeleteFailure() = default;

    virtual bool encode();

    long requestor_id;
    long instance_id;
    RanFunctionId function_id;
    long cause;
    long cause_detail;

    std::shared_ptr<SubscriptionDeleteRequest> req;
};

typedef enum ControlRequestAck
{
    CONTROL_REQUEST_NONE = 0,
    CONTROL_REQUEST_ACK,
    CONTROL_REQUEST_NACK
} ControlRequestAck_t;

class ControlRequest : public Request
{
 public:
    ControlRequest(
        long requestor_id_,long instance_id_,RanFunctionId function_id_,
	e2sm::Control *control_,ControlRequestAck_t ack_request_)
	: function_id(function_id_),control(control_),
	  ack_request(ack_request_),Request(requestor_id_,instance_id_) {};
    virtual ~ControlRequest() = default;

    virtual bool encode();

    RanFunctionId function_id;
    e2sm::Control *control;
    ControlRequestAck_t ack_request;
};

class ControlAck : public Message
{
 public:
    ControlAck()
	: requestor_id(-1),instance_id(-1),function_id(-1),
	  status(-1),outcome(NULL),Message() {};
    ControlAck(
        long requestor_id_,long instance_id_,RanFunctionId function_id_,
	long status_,e2sm::ControlOutcome *outcome_)
	: requestor_id(requestor_id_),instance_id(instance_id_),
	  function_id(function_id_),status(status_),outcome(outcome_),
	  Message() {};
    virtual ~ControlAck() = default;

    virtual bool encode();

    long requestor_id;
    long instance_id;
    RanFunctionId function_id;
    long status;
    e2sm::ControlOutcome *outcome;

    std::shared_ptr<ControlRequest> req;
};

class ControlFailure : public Message
{
 public:
    ControlFailure()
	: requestor_id(-1),instance_id(-1),function_id(-1),
	  cause(-1),cause_detail(-1),outcome(NULL),Message() {};
    ControlFailure(
        long requestor_id_,long instance_id_,RanFunctionId function_id_,
	long cause_,long cause_detail_,e2sm::ControlOutcome *outcome_)
	: requestor_id(requestor_id_),instance_id(instance_id_),
	  function_id(function_id_),cause(cause_),cause_detail(cause_detail_),
	  outcome(outcome_),Message() {};
    virtual ~ControlFailure() = default;

    virtual bool encode();

    long requestor_id;
    long instance_id;
    RanFunctionId function_id;
    long cause;
    long cause_detail;
    e2sm::ControlOutcome *outcome;

    std::shared_ptr<ControlRequest> req;
};

class Indication : public Message
{
 public:
    Indication()
	: requestor_id(-1),instance_id(-1),function_id(-1),
	  action_id(-1),serial_number(-1),type(-1),
	  call_process_id(NULL),call_process_id_len(-1),
	  subscription_request(NULL),Message() {};
    Indication(
        long requestor_id_,long instance_id_,RanFunctionId function_id_,
	long action_id_,long serial_number_,long type_,
	unsigned char *call_process_id_,size_t call_process_id_len_)
	: requestor_id(requestor_id_),instance_id(instance_id_),
	  function_id(function_id_),action_id(action_id_),
	  serial_number(serial_number_),type(type_),
	  call_process_id(call_process_id_),call_process_id_len(call_process_id_len_),
	  subscription_request(NULL),Message() {};
    virtual ~Indication();

    virtual bool encode();

    long requestor_id;
    long instance_id;
    RanFunctionId function_id;
    long action_id;
    long serial_number;
    long type;
    unsigned char *call_process_id;
    size_t call_process_id_len;
    e2sm::Indication *model;
    std::shared_ptr<SubscriptionRequest> subscription_request;
};

class ErrorIndication : public Message
{
 public:
    ErrorIndication()
	: requestor_id(-1),instance_id(-1),function_id(-1),
	  cause(-1),cause_detail(-1),Message() {};
    ErrorIndication(
        long requestor_id_,long instance_id_,RanFunctionId function_id_,
	long cause_,long cause_detail_)
	: requestor_id(requestor_id_),instance_id(instance_id_),
	  function_id(function_id_),cause(cause_),cause_detail(cause_detail_),
	  Message() {};
    virtual ~ErrorIndication() = default;

    virtual bool encode() {};

    long requestor_id;
    long instance_id;
    RanFunctionId function_id;
    long cause;
    long cause_detail;
};

class AgentInterface {
 public:
    virtual bool send_message(const unsigned char *buf,ssize_t buf_len,
			      int mtype,int subid,const std::string& meid,
			      const std::string& xid) = 0;
    virtual bool handle(SubscriptionResponse *resp) = 0;
    virtual bool handle(SubscriptionFailure *resp) = 0;
    virtual bool handle(SubscriptionDeleteResponse *resp) = 0;
    virtual bool handle(SubscriptionDeleteFailure *resp) = 0;
    virtual bool handle(ControlAck *control) = 0;
    virtual bool handle(ControlFailure *control) = 0;
    virtual bool handle(Indication *ind) = 0;
    virtual bool handle(ErrorIndication *ind) = 0;
};

class E2AP {
 public:
    E2AP(AgentInterface *agent_if_)
	: agent_if(agent_if_),requestor_id(1),next_instance_id(1)
    { std::srand(std::time(nullptr)); requestor_id = std::rand() % 65535; };
    virtual ~E2AP() = default;
    virtual bool init();

    long get_requestor_id() {
	const std::lock_guard<std::mutex> lock(mutex);
	return requestor_id;
    };
    long get_next_instance_id() {
	const std::lock_guard<std::mutex> lock(mutex);
	return next_instance_id++;
    };

    bool handle_message(const unsigned char *buf,ssize_t len,int subid,
			std::string meid,std::string xid);

    bool send_control_request(std::shared_ptr<ControlRequest> req,
			      const std::string& meid);
    bool send_subscription_request(std::shared_ptr<SubscriptionRequest> req,
				   const std::string& meid);
    bool send_subscription_delete_request(std::shared_ptr<SubscriptionDeleteRequest> req,
					  const std::string& meid);

    /*
     * NB: at present time, the RIC aggregates subscriptions on its
     * requestorID, so we cannot use that to lookup our subscription.  We
     * have to use the RMR subid.
     */
    std::shared_ptr<SubscriptionRequest> lookup_pending_subscription
        (std::string& xid);
    std::shared_ptr<SubscriptionResponse> lookup_subscription
        (int subid);
    std::shared_ptr<SubscriptionDeleteRequest> lookup_pending_subscription_delete
        (std::string& xid);
    bool delete_all_subscriptions
        (std::string& meid);
    std::shared_ptr<ControlRequest> lookup_control
        (long requestor_id,long instance_id);

 private:
    bool _send_subscription_delete_request(std::shared_ptr<SubscriptionDeleteRequest> req,
					   const std::string& meid, bool locked);
 protected:
    std::mutex mutex;
    long requestor_id;
    long next_instance_id;
    std::list<e2sm::Model *> models;
    std::map<long,std::shared_ptr<ControlRequest>> controls;
    std::map<std::string,std::shared_ptr<SubscriptionRequest>> pending_subscriptions;
    std::map<int,std::shared_ptr<SubscriptionResponse>> subscriptions;
    std::map<std::string,std::shared_ptr<SubscriptionDeleteRequest>> pending_deletes;
    AgentInterface *agent_if;
};

}

#endif /* _E2AP_H_ */
