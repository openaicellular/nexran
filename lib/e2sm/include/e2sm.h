#ifndef _E2SM_H_
#define _E2SM_H_

#include <list>
#include <map>
#include <string>
#include <cstdint>

#include "e2ap.h"

#define E2SM_MAX_DEF_SIZE 16384

namespace e2ap {

//class Subscription;
class ErrorIndication;
class Control;

}

namespace e2sm
{

/**
 * The RicInterface is implemented by a Model on the RIC side.  It
 * provides the necessary E2SM message-handling functions in a RIC app.
 */
class RicModelInterface {
 public:
    //virtual int handle_sub_resp(e2ap::Subscription *sub) = 0;
    //virtual int handle_sub_failure(e2ap::Subscription *sub) = 0;
    virtual int handle_control_ack(e2ap::Control *control) = 0;
    virtual int handle_control_failure(e2ap::Control *control) = 0;
    virtual int handle_indication(e2ap::Indication *indication) = 0;
    virtual int handle_error_indication(e2ap::ErrorIndication *err) = 0;
};

class Model;

class ModelData
{
 public:
    ModelData() : model(NULL) {};
    ModelData(Model *model_) : model(model_) {};

    Model *get_model() { return model; }

 protected:
    Model *model;
};

class EventTrigger : public ModelData
{
 public:
    EventTrigger(Model *model_)
	: buf(NULL),buf_len(0),encoded(false),ModelData(model_) {};
    virtual ~EventTrigger() {
	if (buf)
	    free(buf);
    };

    virtual bool encode() = 0;

    virtual unsigned char *get_buf() { if (!encoded) encode(); return buf; };
    virtual size_t get_buf_len() { if (!encoded) encode(); return buf_len; };

 protected:
    bool encoded;
    unsigned char *buf;
    size_t buf_len;
};

class ActionDefinition : public ModelData
{
 public:
    ActionDefinition(Model *model_)
	: buf(NULL),buf_len(0),encoded(false),ModelData(model_) {};
    virtual ~ActionDefinition() {
	if (buf)
	    free(buf);
    };

    virtual bool encode() = 0;

    virtual unsigned char *get_buf() { if (!encoded) encode(); return buf; };
    virtual size_t get_buf_len() { if (!encoded) encode(); return buf_len; };

 protected:
    bool encoded;
    unsigned char *buf;
    size_t buf_len;
};

class Control : public ModelData
{
 public:
    Control(Model *model_)
	: header(NULL),header_len(0),message(NULL),message_len(0),
	  call_process_id(NULL),call_process_id_len(0),encoded(false),
	  ModelData(model_) {};
    virtual ~Control() {
	if (header)
	    free(header);
	if (message)
	    free(message);
	if (call_process_id)
	    free(call_process_id);
    };

    virtual bool encode() = 0;

    virtual unsigned char *get_header() { if (!encoded) encode(); return header; };
    virtual size_t get_header_len() { if (!encoded) encode(); return header_len; };
    virtual unsigned char *get_message() { if (!encoded) encode(); return message; };
    virtual size_t get_message_len() { if (!encoded) encode(); return message_len; };
    virtual unsigned char *get_call_process_id() { if (!encoded) encode(); return call_process_id; };
    virtual size_t get_call_process_id_len() { if (!encoded) encode(); return call_process_id_len; };

 protected:
    bool encoded;
    unsigned char *header;
    size_t header_len;
    unsigned char *message;
    size_t message_len;
    unsigned char *call_process_id;
    size_t call_process_id_len;
};

class ControlOutcome : public ModelData
{
 public:
    ControlOutcome(Model *model_)
	: outcome(NULL),outcome_len(0),encoded(false),ModelData(model_) {};
    virtual ~ControlOutcome() {
	if (outcome)
	    free(outcome);
    };

    virtual bool encode() = 0;

    virtual unsigned char *get_outcome() { if (!encoded) encode(); return outcome; };
    virtual size_t get_outcome_len() { if (!encoded) encode(); return outcome_len; };

 protected:
    bool encoded;
    unsigned char *outcome;
    size_t outcome_len;
};

class Indication : public ModelData
{
 public:
    Indication(Model *model_)
	: header(NULL),header_len(0),message(NULL),message_len(0),
	  encoded(false),ModelData(model_),parent(NULL) {};
    Indication(Model *model_,unsigned char *header_,size_t header_len_,
	       unsigned char *message_,size_t message_len_,bool encoded_)
	: header(NULL),header_len(0),message(NULL),message_len(0),
	  encoded(encoded_),ModelData(model_),parent(NULL) {};
    virtual ~Indication() {
	if (header)
	    free(header);
	if (message)
	    free(message);
    };

    virtual bool encode() = 0;

    virtual unsigned char *get_header() { return header; };
    virtual size_t get_header_len() { return header_len; };
    virtual unsigned char *get_message() { return message; };
    virtual size_t get_message_len() { return message_len; };

    e2ap::Indication *parent;

 protected:
    bool encoded;
    unsigned char *header;
    size_t header_len;
    unsigned char *message;
    size_t message_len;
};

/**
 * An e2sm::Model encapsulates both the definition and running state of
 * a service model.
 */
class Model
{
 public:
    Model(std::string &name_,std::string &oid_)
	: name(name_),oid(oid_) {};
    Model(const char *name_,const char *oid_)
	: name(name_),oid(oid_) {};
    virtual ~Model() = default;
    virtual int init() = 0;
    virtual void stop() = 0;
    virtual const std::string& get_name() { return name; };
    virtual const std::string& get_oid() { return oid; };
    virtual void add_function(e2ap::RanFunctionId function_id,
			      e2ap::RanFunction *function) {
	functions[function_id] = function;
    }

    virtual Indication *decode(e2ap::Indication *ind,
			       unsigned char *header,ssize_t header_len,
			       unsigned char *message,ssize_t message_len) = 0;
    virtual ControlOutcome *decode(e2ap::ControlAck *ack,
				   unsigned char *outcome,ssize_t outcome_len) = 0;
    virtual ControlOutcome *decode(e2ap::ControlFailure *failure,
				   unsigned char *outcome,ssize_t outcome_len) = 0;

 protected:
    const std::string name;
    const std::string oid;
    std::map<e2ap::RanFunctionId,e2ap::RanFunction *> functions;
};

}

#endif /* _E2SM_H_ */
