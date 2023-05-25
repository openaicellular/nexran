#ifndef _NEXRAN_H_
#define _NEXRAN_H_

#include <string>
#include <list>
#include <map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <ctime>

#include "rapidjson/prettywriter.h"
#include "rapidjson/document.h"

#include "InfluxDBFactory.h"

#include "ricxfcpp/messenger.hpp"

#include "restserver.h"
#include "config.h"
#include "e2ap.h"
#include "e2sm.h"
#include "e2sm_nexran.h"
#include "e2sm_kpm.h"
#include "e2sm_zylinium.h"

namespace nexran {

class AppError : public RequestError {
 public:
    AppError(int http_status_,std::list<std::string> messages_)
	: RequestError(http_status_,messages_) {};
    AppError(int http_status_,std::string message_)
	: RequestError(http_status_,message_) {};
    AppError(int http_status_)
	: RequestError(http_status_) {};
    virtual ~AppError() = default;

};

class AbstractResource {
 public:
    virtual ~AbstractResource() = default;
    virtual std::string& getName() = 0;
    virtual void serialize(rapidjson::Writer<rapidjson::StringBuffer>& writer) {};
    virtual bool update(rapidjson::Document& d,
			AppError **ae) { return false; };
};

typedef enum {
    STRING = 0,
    INT,
    UINT,
    FLOAT,
    OBJECT,
    ARRAY,
    BOOL
} JsonTypeMap;

typedef enum {
    GET = 0,
    HEAD,
    POST,
    PUT,
    DELETE,
    CONNECT,
    OPTIONS,
    TRACE,
    PATCH,
} HttpMethod;

template <class T>
class Resource : public AbstractResource {
 public:

    static bool validate_value(std::string& k,const rapidjson::Value& v)
    {
	if (T::propertyTypes.count(k) < 1)
	    return false;
	switch (T::propertyTypes[k]) {
	case JsonTypeMap::STRING: return v.IsString();
	case JsonTypeMap::INT:    return v.IsInt();
	case JsonTypeMap::UINT:   return v.IsUint();
	case JsonTypeMap::FLOAT:  return v.IsFloat();
	case JsonTypeMap::OBJECT: return v.IsObject();
	case JsonTypeMap::ARRAY:  return v.IsArray();
	case JsonTypeMap::BOOL:   return v.IsBool();
	default:                  return false;
	}
    };

    static bool validate_json
	(HttpMethod m,const rapidjson::Value& obj,AppError **ae)
    {
	bool retval = true;
	if (T::required.count(m) == 1) {
	    for (auto it = T::required[m].begin(); it != T::required[m].end(); ++it) {
		if (!obj.HasMember(it->c_str())) {
		    if (ae) {
			if (!*ae)
			    *ae = new AppError(400);
			(*ae)->add(std::string("missing required property: ") + *it);
		    }
		    if (T::propertyErrorImmediateAbort)
			return false;
		    else
			retval = false;
		}
		else if (!validate_value(*it,obj[it->c_str()])) {
		    if (ae) {
			if (!*ae)
			    *ae = new AppError(400);
			(*ae)->add(std::string("invalid type for property: ") + *it);
		    }
		    if (T::propertyErrorImmediateAbort)
			return false;
		    else
			retval = false;
		}
	    }
	}
	if (T::optional.count(m) == 1) {
	    for (auto it = T::optional[m].begin(); it != T::optional[m].end(); ++it) {
		if (!obj.HasMember(it->c_str()))
		    continue;
		else if (!validate_value(*it,obj[it->c_str()])) {
		    if (ae) {
			if (!*ae)
			    *ae = new AppError(400);
			(*ae)->add(std::string("invalid type for property: ") + *it);
		    }
		    if (T::propertyErrorImmediateAbort)
			return false;
		    else
			retval = false;
		}
	    }
	}
	if (T::disallowed.count(m) == 1) {
	    for (auto it = T::disallowed[m].begin(); it != T::disallowed[m].end(); ++it) {
		if (!obj.HasMember(it->c_str()))
		    continue;
		if (ae) {
		    if (!*ae)
			*ae = new AppError(400);
		    (*ae)->add(std::string("invalid property: ") + *it);
		}
		if (T::propertyErrorImmediateAbort)
		    return false;
		else
		    retval = false;
	    }
	}
	for (auto it = T::propertyEnums.begin(); it != T::propertyEnums.end(); ++it) {
	    if (obj.HasMember(it->first.c_str())
		&& obj[it->first.c_str()].IsString()
		&& std::find(it->second.begin(),it->second.end(),
			     std::string(obj[it->first.c_str()].GetString()))
		    == it->second.end()) {
	    if (ae) {
		if (!*ae)
		    *ae = new AppError(400);
		(*ae)->add(std::string("invalid enum value for property: ") + it->first);
	    }
	    if (T::propertyErrorImmediateAbort)
		return false;
	    else
		retval = false;
	    }
	}
	return retval;
    };
};

class AppConfig : public Resource<AppConfig> {
 public:
    static std::map<std::string,JsonTypeMap> propertyTypes;
    static std::map<std::string,std::list<std::string>> propertyEnums;
    static std::map<HttpMethod,std::list<std::string>> required;
    static std::map<HttpMethod,std::list<std::string>> optional;
    static std::map<HttpMethod,std::list<std::string>> disallowed;
    static const bool propertyErrorImmediateAbort = false;

    AppConfig()
	: kpm_interval_index(e2sm::kpm::MS5120), influxdb_url(""),
	  name("appconfig") {};
    AppConfig(e2sm::kpm::KpmPeriod_t kpm_interval_index_,
	      std::string influxdb_url_)
	: kpm_interval_index(kpm_interval_index_), influxdb_url(influxdb_url_),
	  name("appconfig") {};
    virtual ~AppConfig() = default;

    std::string& getName() { return name; };
    void serialize(rapidjson::Writer<rapidjson::StringBuffer>& writer);
    static AppConfig *create(rapidjson::Document& d,AppError **ae);
    bool update(rapidjson::Document& d,AppError **ae);

    e2sm::kpm::KpmPeriod_t kpm_interval_index;
    std::string influxdb_url;

 private:
    std::string name;
};

class Ue : public Resource<Ue> {
 public:
    static std::map<std::string,JsonTypeMap> propertyTypes;
    static std::map<std::string,std::list<std::string>> propertyEnums;
    static std::map<HttpMethod,std::list<std::string>> required;
    static std::map<HttpMethod,std::list<std::string>> optional;
    static std::map<HttpMethod,std::list<std::string>> disallowed;
    static const bool propertyErrorImmediateAbort = false;

    Ue(const std::string& imsi_)
	: imsi(imsi_) {};
    Ue(const std::string& imsi_,const std::string& tmsi_,
       const std::string& crnti_)
	: imsi(imsi_),tmsi(tmsi_),crnti(crnti_),bound_slice("") {};
    virtual ~Ue() = default;

    std::string& getName() { return imsi; }
    void serialize(rapidjson::Writer<rapidjson::StringBuffer>& writer);
    static Ue *create(rapidjson::Document& d,AppError **ae);
    bool update(rapidjson::Document& d,AppError **ae);
    bool is_bound() {
	return !bound_slice.empty();
    };
    std::string &get_bound_slice() {
	return bound_slice;
    };
    bool bind_slice(std::string &slice_name) {
	if (is_bound())
	    return false;
	bound_slice = slice_name;
    };
    bool unbind_slice() {
	if (!is_bound())
	    return false;
	bound_slice.clear();
    };

 private:
    std::string imsi;
    std::string tmsi;
    std::string crnti;
    bool connected;
    std::string bound_slice;
};

class AllocationPolicy {
 public:
    typedef enum {
	Proportional = 1,
    } Type;

    static std::map<Type,const char *> type_to_string;

    virtual ~AllocationPolicy() = default;
    virtual const char *getName() = 0;
    virtual const Type getType() = 0;
    virtual void serialize(rapidjson::Writer<rapidjson::StringBuffer>& writer) = 0;
    virtual bool update(const rapidjson::Value& obj,AppError **ae) = 0;
};

class ProportionalAllocationPolicy : public AllocationPolicy {
 public:
    ProportionalAllocationPolicy(int share_,bool auto_equalize_ = false,
				 bool throttle_ = false,int throttle_threshold_ = -1,
				 int throttle_period_ = 1800,int throttle_share_ = 128,
				 int throttle_target_ = 0)
	: share(share_),auto_equalize(auto_equalize_),
	  throttle(throttle_),throttle_threshold(throttle_threshold_),
	  throttle_period(throttle_period_),throttle_share(throttle_share_),
	  throttle_target(throttle_target_),
	  is_throttling(false),throttle_end(0),throttle_saved_share(-1),
	  metrics(throttle_period_) {};
    ~ProportionalAllocationPolicy() = default;

    const char *getName() { return name; };
    int getShare() { return share; };
    bool setShare(int share_) {
	if (share_ < 0 || share_ > 1024)
	    return false;
	share = share_;
	return true;
    };
    bool isAutoEqualized() { return auto_equalize; }
    bool isThrottled() { return throttle; };
    bool isThrottling() { return is_throttling; };
    int maybeEndThrottling();
    int maybeStartThrottling();
    int maybeUpdateThrottling();
    e2sm::kpm::MetricsIndex& getMetrics() { return metrics; };
    const AllocationPolicy::Type getType() { return AllocationPolicy::Type::Proportional; }
    void serialize(rapidjson::Writer<rapidjson::StringBuffer>& writer)
    {
	writer.StartObject();
	writer.String("type");
	writer.String("proportional");
	writer.String("share");
	writer.Int(share);
	writer.String("auto_equalize");
	writer.Bool(auto_equalize);
	writer.String("throttle");
	writer.Bool(throttle);
	writer.String("throttle_threshold");
	writer.Int(throttle_threshold);
	writer.String("throttle_period");
	writer.Int(throttle_period);
	writer.String("throttle_share");
	writer.Int(throttle_share);
	writer.String("throttle_target");
	writer.Int(throttle_target);
	writer.EndObject();
    };
    bool update(const rapidjson::Value& obj,AppError **ae);

 private:
    static constexpr const char *name = "proportional";

    int share;
    bool auto_equalize;
    bool throttle;
    int throttle_threshold;
    int throttle_period;
    int throttle_share;
    int throttle_target;

    bool is_throttling;
    time_t throttle_end;
    int throttle_saved_share;
    e2sm::kpm::MetricsIndex metrics;
};

class Slice : public Resource<Slice> {
 public:
    static std::map<std::string,JsonTypeMap> propertyTypes;
    static std::map<std::string,std::list<std::string>> propertyEnums;
    static std::map<HttpMethod,std::list<std::string>> required;
    static std::map<HttpMethod,std::list<std::string>> optional;
    static std::map<HttpMethod,std::list<std::string>> disallowed;
    static const bool propertyErrorImmediateAbort = false;

    Slice(const std::string& name_)
	: name(name_),
	  allocation_policy(new ProportionalAllocationPolicy(512)) {};
    Slice(const std::string& name_,AllocationPolicy *allocation_policy_)
	: name(name_),
	  allocation_policy(allocation_policy_) {};
    virtual ~Slice() = default;

    std::string& getName() { return name; }
    AllocationPolicy *getPolicy() { return allocation_policy; }
    void serialize(rapidjson::Writer<rapidjson::StringBuffer>& writer);
    static Slice *create(rapidjson::Document& d,AppError **ae);
    bool update(rapidjson::Document& d,AppError **ae);
    bool bind_ue(Ue *ue) {
	if (ues.count(ue->getName()) > 0)
	    return false;
	ues[std::string(ue->getName())] = ue;
	return true;
    };
    bool unbind_ue(std::string& imsi) {
	if (ues.count(imsi) < 1)
	    return false;
	ues.erase(imsi);
	return true;
    };
    void unbind_all_ues() {
	for (auto it = ues.begin(); it != ues.end(); ++it)
	    it->second->unbind_slice();
	ues.clear();
    };

 private:
    std::string name;
    AllocationPolicy *allocation_policy;
    std::map<std::string, Ue *> ues;
};

class NodeB : public Resource<NodeB> {
 public:
    static std::map<std::string,JsonTypeMap> propertyTypes;
    static std::map<std::string,std::list<std::string>> propertyEnums;
    static std::map<HttpMethod,std::list<std::string>> required;
    static std::map<HttpMethod,std::list<std::string>> optional;
    static std::map<HttpMethod,std::list<std::string>> disallowed;
    static const bool propertyErrorImmediateAbort = false;

    typedef enum {
	UNKNOWN = 0,
	GNB = 1,
	GNB_CU_UP,
	GNB_DU,
	EN_GNB,
	ENB,
	NG_ENB,
	__END__
    } Type;

    static const char *type_to_type_string(Type t);
    static Type type_string_to_type(const char *ts);
    static const char *get_name_prefix(Type type,uint8_t id_len);

    static std::unique_ptr<std::string>	build_name(
	Type type,const char *mcc,const char *mnc,
	int32_t id,uint8_t id_len);

    NodeB(Type type_,const char *mcc_,const char *mnc_,
	  int32_t id_,uint8_t id_len_)
	: type(type_),mcc(std::string(mcc_)),mnc(std::string(mnc_)),
	  id(id_),id_len(id_len_),connected(false),total_prbs(-1),
	  name(build_name(type_,mcc_,mnc_,id_,id_len_)),
	  dl_mask_def("0x0"),ul_mask_def("0x0"),
	  dl_mask_sched(),ul_mask_sched() {};
    virtual ~NodeB() = default;

    std::string& getName() { return *name; }
    void serialize(rapidjson::Writer<rapidjson::StringBuffer>& writer);
    static NodeB *create(rapidjson::Document& d,AppError **ae);
    bool update(rapidjson::Document& d,AppError **ae);
    bool bind_slice(Slice *slice) {
	if (slices.count(slice->getName()) > 0)
	    return false;
	slices[std::string(slice->getName())] = slice;
	return true;
    };
    bool unbind_slice(std::string& slice_name) {
	if (slices.count(slice_name) < 1)
	    return false;
	slices.erase(slice_name);
	return true;
    };
    bool is_slice_bound(std::string& slice_name) {
	if (slices.count(slice_name) > 0)
	    return true;
	return false;
    };
    std::map<std::string,Slice *>& get_slices() {
	return slices;
    };
    e2sm::zylinium::MaskConfigRequest *make_mask_config_request(e2sm::Model *model) {
	return new e2sm::zylinium::MaskConfigRequest(
	    model,dl_mask_def,ul_mask_def,dl_mask_sched,ul_mask_sched);
    };
    void update_last_indication(void) {
	struct timeval now;
	gettimeofday(&now,NULL);
	last_indication = static_cast<double>(now.tv_sec) + now.tv_usec / 1000000.0f;
	connected = true;
    };
    void update_config(int total_prbs_) {
	total_prbs = total_prbs_;
    };
    void update_mask_from_status(e2sm::zylinium::MaskStatusReport *report) {
	dl_mask = report->dl_mask;
	ul_mask = report->ul_mask;
	dl_mask_def = report->dl_def;
	ul_mask_def = report->ul_def;
	dl_mask_sched = report->dl_sched;
	ul_mask_sched = report->ul_sched;
    };
    int get_total_prbs() { return total_prbs; };

 private:
    static const char *type_string_map[NodeB::Type::__END__];

    std::unique_ptr<std::string> name;
    Type type;
    std::string mcc;
    std::string mnc;
    int32_t id;
    uint8_t id_len;
    bool connected;
    double last_indication;
    int total_prbs;
    std::map<std::string,Slice *> slices;
    e2sm::zylinium::BlockedMask dl_mask;
    e2sm::zylinium::BlockedMask ul_mask;
    std::string dl_mask_def;
    std::string ul_mask_def;
    std::list<e2sm::zylinium::BlockedMask> dl_mask_sched;
    std::list<e2sm::zylinium::BlockedMask> ul_mask_sched;
};

class SliceMetrics {
 public:
    SliceMetrics(const Slice& slice_)
	: slice(slice_) {};
    virtual ~SliceMetrics() {};

 private:
    const Slice& slice;
};

/*
 * This class tracks northbound requests that map to multiple requests
 * to different RAN nodes (NodeBs).  A watcher thread in the App looks
 * for completion/timeout and sends a response to the caller.  Note that
 * this does not handle the case where the caller disconnects before we
 * timeout; not our problem.
 */
class RequestGroup
{
 public:
    enum RequestState {
	PENDING = 0,
	SUCCESS = 1,
	FAILURE = 2
    };

    class RequestStatus {
     public:
	RequestStatus(std::shared_ptr<e2ap::Request> req_)
	    : req(req_),state(PENDING) {};
	~RequestStatus() = default;

	std::shared_ptr<e2ap::Request> req;
	RequestState state;
    };

    RequestGroup(std::shared_ptr<RequestContext> ctx_,int timeout_ = 8)
	: ctx(ctx_),timeout(std::time(nullptr) + timeout_) {};
    virtual ~RequestGroup() = default;

    virtual bool add(std::shared_ptr<e2ap::Request> req) {
	requests[req->instance_id] = new RequestStatus(req);
    };
    virtual void update(long instance_id,RequestState state) {
	if (requests.count(instance_id) <= 0)
	    return;
	requests[instance_id]->state = state;
    };
    virtual bool is_done(int *succeeded,int *failed,int *pending) {
	bool ret = true;
	for (auto it = requests.begin(); it != requests.end(); ++it) {
	    switch (it->second->state) {
	    case PENDING:
		if (pending)
		    ++*pending;
		ret = false;
		break;
	    case SUCCESS:
		if (succeeded)
		    ++*succeeded;
		break;
	    case FAILURE:
		if (failed)
		    ++*failed;
		break;
	    default:
		break;
	    }
	}
	return ret;
    };
    virtual bool is_expired() {
	if (std::time(nullptr) > timeout)
	    return true;
	return false;
    };

    std::shared_ptr<RequestContext> get_ctx() { return ctx; };

 protected:
    std::shared_ptr<RequestContext> ctx;
    time_t timeout;
    std::map<long,RequestStatus *> requests;
};

class App
    : public xapp::Messenger,
      public e2ap::AgentInterface,
      public e2sm::nexran::AgentInterface,
      public e2sm::kpm::AgentInterface,
      public e2sm::zylinium::AgentInterface
{
 public:
    typedef enum {
	NodeBResource = 0,
	SliceResource,
	UeResource,
    } ResourceType;

    App(Config &config_)
	: e2ap(this),config(config_),running(false),should_stop(false),
	  rmr_thread(NULL),response_thread(NULL),
	  xapp::Messenger(NULL,not config_[Config::ItemName::RMR_NOWAIT]->b),
	  nexran(new e2sm::nexran::NexRANModel(this)),
	  kpm(new e2sm::kpm::KpmModel(this)),
	  zylinium(new e2sm::zylinium::ZyliniumModel(this)),
	  app_config((e2sm::kpm::KpmPeriod_t)config_[Config::ItemName::KPM_INTERVAL_INDEX]->i,
		     std::string(config_[Config::ItemName::INFLUXDB_URL]->s)) { };
    virtual ~App() = default;
    virtual void init();
    virtual void start();
    virtual void stop();
    virtual void response_handler();

    // xapp::Messenger callback
    virtual void handle_rmr_message(
	xapp::Message &msg,int mtype,int subid,int payload_len,
	xapp::Msg_component &payload);

    // e2ap::RicAgentInterface util/handler functions
    bool send_message(const unsigned char *buf,ssize_t buf_len,
		      int mtype,int subid,const std::string& meid,
		      const std::string& xid);
    bool handle(e2ap::SubscriptionResponse *resp);
    bool handle(e2ap::SubscriptionFailure *resp);
    bool handle(e2ap::SubscriptionDeleteResponse *resp);
    bool handle(e2ap::SubscriptionDeleteFailure *resp);
    bool handle(e2ap::ControlAck *control);
    bool handle(e2ap::ControlFailure *control);
    bool handle(e2ap::Indication *indication);
    bool handle(e2ap::ErrorIndication *ind);

    // e2sm::nexran::AgentInterface handler functions
    bool handle(e2sm::nexran::SliceStatusIndication *ind);
    // e2sm::kpm::AgentInterface handler functions
    bool handle(e2sm::kpm::KpmIndication *ind);
    // e2sm::zylinium::AgentInterface handler functions
    bool handle(e2sm::zylinium::MaskStatusIndication *ind);

    void serialize(ResourceType rt,
		   rapidjson::Writer<rapidjson::StringBuffer>& writer);
    bool serialize(ResourceType rt,std::string& rname,
		   rapidjson::Writer<rapidjson::StringBuffer>& writer,
		   AppError **ae);
    bool add(ResourceType rt,AbstractResource *resource,
	     rapidjson::Writer<rapidjson::StringBuffer>& writer,
	     AppError **ae);
    bool del(ResourceType rt,std::string& rname,
	     AppError **ae);
    bool create(std::shared_ptr<RequestContext> ctx,ResourceType rt,
		rapidjson::Document& d);
    bool update(ResourceType rt,std::string& rname,
		rapidjson::Document& d,AppError **ae);

    bool bind_slice_nodeb(std::string& slice_name,std::string& nodeb_name,
			  AppError **ae);
    bool unbind_slice_nodeb(std::string& slice_name,std::string& nodeb_name,
			    AppError **ae);
    bool bind_ue_slice(std::string& imsi,std::string& slice_name,
		       AppError **ae);
    bool unbind_ue_slice(std::string& imsi,std::string& slice_name,
			 AppError **ae);
    bool handle_appconfig_update(void);

    Config &config;
    AppConfig app_config;

 private:
    std::thread *rmr_thread;
    std::thread *response_thread;
    bool should_stop;
    e2ap::E2AP e2ap;
    e2sm::nexran::NexRANModel *nexran;
    e2sm::kpm::KpmModel *kpm;
    e2sm::zylinium::ZyliniumModel *zylinium;
    bool running;
    RestServer server;
    std::mutex mutex;
    std::condition_variable cv;
    std::list<RequestGroup *> request_groups;
    std::map<ResourceType,std::map<std::string,AbstractResource *>> db;
    std::map<ResourceType,const char *> rtype_to_label = {
	{ ResourceType::NodeBResource, "nodeb" },
	{ ResourceType::SliceResource, "slice" },
	{ ResourceType::UeResource, "ue" }
    };
    std::map<ResourceType,const char *> rtype_to_label_plural = {
	{ ResourceType::NodeBResource, "nodebs" },
	{ ResourceType::SliceResource, "slices" },
	{ ResourceType::UeResource, "ues" }
    };
    std::unique_ptr<influxdb::InfluxDB> influxdb;
    std::string influxdb_url;
};

}

#endif /* _NEXRAN_H_ */
