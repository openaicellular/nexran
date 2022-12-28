
#include <chrono>

#include "mdclog/mdclog.h"
#include "rmr/RIC_message_types.h"
#include "ricxfcpp/message.hpp"
#include "ricxfcpp/messenger.hpp"

#include "nexran.h"
#include "e2ap.h"
#include "e2sm.h"
#include "e2sm_nexran.h"
#include "e2sm_kpm.h"

namespace nexran {

static void rmr_callback(
    xapp::Message &msg,int mtype,int subid,int payload_len,
    xapp::Msg_component payload,void *data)
{
    ((App *)data)->handle_rmr_message(
        msg,mtype,msg.Get_subid(),payload_len,payload);
}

void App::handle_rmr_message(
    xapp::Message &msg,int mtype,int subid,int payload_len,
    xapp::Msg_component &payload)
{
    mdclog_write(MDCLOG_DEBUG,"RMR message (type %d, source %s)",
		 mtype,msg.Get_meid().get());

    switch (mtype) {
    case RIC_SUB_REQ:
    case RIC_SUB_RESP:
    case RIC_SUB_FAILURE:
    case RIC_SUB_DEL_REQ:
    case RIC_SUB_DEL_RESP:
    case RIC_SUB_DEL_FAILURE:
    case RIC_SERVICE_UPDATE:
    case RIC_SERVICE_UPDATE_ACK:
    case RIC_SERVICE_UPDATE_FAILURE:
    case RIC_CONTROL_REQ:
    case RIC_CONTROL_ACK:
    case RIC_CONTROL_FAILURE:
    case RIC_INDICATION:
    case RIC_SERVICE_QUERY:
	break;
    default:
	mdclog_write(MDCLOG_WARN,"unsupported RMR message type %d",mtype);
	return;
    }

    e2ap.handle_message(payload.get(),payload_len,subid,
			std::string((char *)msg.Get_meid().get()),
			std::string((char *)msg.Get_xact().get()));

    return;
}

bool App::send_message(const unsigned char *buf,ssize_t buf_len,
		       int mtype,int subid,const std::string& meid,
		       const std::string& xid)
{
    std::unique_ptr<xapp::Message> msg = Alloc_msg(buf_len);
    msg->Set_mtype(mtype);
    msg->Set_subid(subid);
    msg->Set_len(buf_len);
    xapp::Msg_component payload = msg->Get_payload();
    memcpy((char *)payload.get(),(char *)buf,
	   ((msg->Get_available_size() < buf_len)
	    ? msg->Get_available_size() : buf_len));
    std::shared_ptr<unsigned char> msg_meid((unsigned char *)strdup(meid.c_str()));
    msg->Set_meid(msg_meid);
    if (!xid.empty()) {
	std::shared_ptr<unsigned char> msg_xid((unsigned char *)strdup(xid.c_str()));
	msg->Set_xact(msg_xid);
    }
    return msg->Send();
}

/*
 * These handlers work at the App level.  The E2AP request tracking
 * library stores requests and watches for responses, and by the time
 * these handlers are invoked, it has constructed that mapping for us.
 * These handlers map request/response pairs to RequestGroups that came
 * from the northbound interface.
 */
bool App::handle(e2ap::SubscriptionResponse *resp)
{
    mdclog_write(MDCLOG_DEBUG,"nexran SubscriptionResponse handler");
}

bool App::handle(e2ap::SubscriptionFailure *resp)
{
    mdclog_write(MDCLOG_DEBUG,"nexran SubscriptionFailure handler");
}

bool App::handle(e2ap::SubscriptionDeleteResponse *resp)
{
    mdclog_write(MDCLOG_DEBUG,"nexran SubscriptionDeleteResponse handler");
}

bool App::handle(e2ap::SubscriptionDeleteFailure *resp)
{
    mdclog_write(MDCLOG_DEBUG,"nexran SubscriptionDeleteFailure handler");
}
    
bool App::handle(e2ap::ControlAck *control)
{
    mdclog_write(MDCLOG_DEBUG,"nexran ControlAck handler");
}

bool App::handle(e2ap::ControlFailure *control)
{
    mdclog_write(MDCLOG_DEBUG,"nexran ControlFailure handler");
}

bool App::handle(e2ap::Indication *ind)
{
    mdclog_write(MDCLOG_DEBUG,"nexran Indication handler");
}

bool App::handle(e2ap::ErrorIndication *ind)
{
    mdclog_write(MDCLOG_DEBUG,"nexran ErrorIndication handler");
}

bool App::handle(e2sm::nexran::SliceStatusIndication *ind)
{
    mdclog_write(MDCLOG_DEBUG,"nexran SliceStatusIndication handler");
}

bool App::handle(e2sm::kpm::KpmIndication *ind)
{
    mdclog_write(MDCLOG_DEBUG,"kpm KpmIndication handler");
}

void App::response_handler()
{
    std::unique_lock<std::mutex> lock(mutex);

    while (!should_stop) {
	if (cv.wait_for(lock,std::chrono::seconds(1)) == std::cv_status::timeout)
	    continue;
	for (auto it = request_groups.begin(); it != request_groups.end(); ++it) {
	    int succeeded = 0,failed = 0,pending = 0;
	    RequestGroup *group = *it;
	    if (group->is_done(&succeeded,&failed,&pending)
		|| group->is_expired()) {
		std::shared_ptr<RequestContext> ctx = group->get_ctx();
		Pistache::Http::ResponseWriter& response = ctx->get_response();
		if (pending)
		    response.send(static_cast<Pistache::Http::Code>(504));
		else if (failed)
		    response.send(static_cast<Pistache::Http::Code>(502));
		else
		    response.send(static_cast<Pistache::Http::Code>(200));

		request_groups.erase(it);
	    }
	}
	lock.unlock();
	cv.notify_one();
    }
}

void App::init()
{
    db[ResourceType::SliceResource][std::string("default")] = new Slice("default");
}

void App::start()
{
    if (running)
	return;

    should_stop = false;

    /*
     * Init the E2.  Note there is nothing to do until we are configured
     * via northbound interface with objects.  Eventually we should
     * store config in the RNIB and restore things on startup, possibly.
     */
    e2ap.init();

    Add_msg_cb(RIC_SUB_RESP,rmr_callback,this);
    Add_msg_cb(RIC_SUB_FAILURE,rmr_callback,this);
    Add_msg_cb(RIC_SUB_DEL_RESP,rmr_callback,this);
    Add_msg_cb(RIC_SUB_DEL_FAILURE,rmr_callback,this);
    Add_msg_cb(RIC_CONTROL_ACK,rmr_callback,this);
    Add_msg_cb(RIC_CONTROL_FAILURE,rmr_callback,this);
    Add_msg_cb(RIC_INDICATION,rmr_callback,this);

    rmr_thread = new std::thread(&App::Listen,this);
    response_thread = new std::thread(&App::response_handler,this);

    /*
     * Init and start the northbound interface.
     * NB: the RMR Messenger superclass is already running at this point.
     */
    server.init(this);
    server.start();
    running = true;
}

void App::stop()
{
    /* Stop the northbound interface. */
    server.stop();
    /* Stop the RMR Messenger superclass. */
    Stop();
    rmr_thread->join();
    delete rmr_thread;
    rmr_thread = NULL;
    response_thread->join();
    delete response_thread;
    response_thread = NULL;
    running = false;
    should_stop = false;
}

void App::serialize(ResourceType rt,
		    rapidjson::Writer<rapidjson::StringBuffer>& writer)
{
    const char *label;

    mutex.lock();
    writer.StartObject();
    writer.String(rtype_to_label_plural[rt]);
    writer.StartArray();
    for (auto it = db[rt].begin(); it != db[rt].end(); ++it)
	it->second->serialize(writer);
    writer.EndArray();
    writer.EndObject();
    mutex.unlock();
}

bool App::serialize(ResourceType rt,std::string& rname,
		    rapidjson::Writer<rapidjson::StringBuffer>& writer,
		    AppError **ae)
{
    mutex.lock();
    if (db[rt].count(rname) < 1) {
	mutex.unlock();
	if (ae) {
	    if (*ae == NULL)
		*ae = new AppError(404);
	    (*ae)->add(std::string(rtype_to_label[rt])
		       + std::string(" does not exist"));
	}
	return false;
    }

    db[rt][rname]->serialize(writer);
    mutex.unlock();
    return true;
}

bool App::add(ResourceType rt,AbstractResource *resource,
	      rapidjson::Writer<rapidjson::StringBuffer>& writer,
	      AppError **ae)
{
    std::string& rname = resource->getName();

    mutex.lock();
    if (db[rt].count(rname) > 0) {
	if (ae) {
	    if (*ae == NULL)
		*ae = new AppError(403);
	    (*ae)->add(std::string(rtype_to_label[rt])
		       + std::string(" already exists"));
	}
	mutex.unlock();
	return false;
    }

    db[rt][rname] = resource;
    resource->serialize(writer);
    mutex.unlock();

    if (rt == App::ResourceType::NodeBResource) {
	/*
	e2sm::nexran::EventTrigger *trigger = \
	    new e2sm::nexran::EventTrigger(nexran,1000);
	std::list<e2ap::Action *> actions;
	actions.push_back(new e2ap::Action(1,e2ap::ACTION_REPORT,NULL,-1));
	std::shared_ptr<e2ap::SubscriptionRequest> req = \
	    std::make_shared<e2ap::SubscriptionRequest>(
		e2ap.get_requestor_id(),e2ap.get_next_instance_id(),
		1,trigger,actions);
	req->set_meid(rname);
	e2ap.send_subscription_request(req,rname);
	*/

	e2sm::nexran::SliceStatusRequest *sreq = \
	    new e2sm::nexran::SliceStatusRequest(nexran);
	std::shared_ptr<e2ap::ControlRequest> creq = std::make_shared<e2ap::ControlRequest>(
            e2ap.get_requestor_id(),e2ap.get_next_instance_id(),
	    1,sreq,e2ap::CONTROL_REQUEST_ACK);
	creq->set_meid(rname);
	e2ap.send_control_request(creq,rname);

	e2sm::kpm::EventTrigger *trigger = \
	    new e2sm::kpm::EventTrigger(kpm);
	std::list<e2ap::Action *> actions;
	actions.push_back(new e2ap::Action(1,e2ap::ACTION_REPORT,NULL,-1));
	std::shared_ptr<e2ap::SubscriptionRequest> req = \
	    std::make_shared<e2ap::SubscriptionRequest>(
		e2ap.get_requestor_id(),e2ap.get_next_instance_id(),
		0,trigger,actions);
	req->set_meid(rname);
	e2ap.send_subscription_request(req,rname);
    }

    mdclog_write(MDCLOG_DEBUG,"added %s %s",
		 rtype_to_label[rt],resource->getName().c_str());

    return true;
}

bool App::del(ResourceType rt,std::string& rname,
	      AppError **ae)
{
    mutex.lock();
    if (db[rt].count(rname) < 1) {
	mutex.unlock();
	if (ae) {
	    if (!*ae)
		*ae = new AppError(404);
	    (*ae)->add(std::string("not found"));
	}
	return false;
    }

    mdclog_write(MDCLOG_DEBUG,"deleting %s %s",
		 rtype_to_label[rt],rname.c_str());

    if (rt == App::ResourceType::UeResource) {
	Ue *ue = (Ue *)db[App::ResourceType::UeResource][rname];
	std::string &imsi = ue->getName();

	if (ue->is_bound()
	    && db[App::ResourceType::SliceResource].count(ue->get_bound_slice()) > 0) {
	    std::string &slice_name = ue->get_bound_slice();
	    Slice *slice = (Slice *)db[App::ResourceType::SliceResource][slice_name];

	    if (!slice->unbind_ue(imsi)) {
		mutex.unlock();
		if (ae) {
		    if (!*ae)
			*ae = new AppError(404);
		    (*ae)->add(std::string("ue not bound to this slice"));
		}
		return false;
	    }

	    e2sm::nexran::SliceUeUnbindRequest *sreq = \
		new e2sm::nexran::SliceUeUnbindRequest(nexran,slice_name,imsi);

	    for (auto it = db[ResourceType::NodeBResource].begin();
		 it != db[ResourceType::NodeBResource].end();
		 ++it) {
		NodeB *nodeb = (NodeB *)it->second;

		if (!nodeb->is_slice_bound(slice_name))
		    continue;

		// Each request needs a different RequestId, so we have to
		// re-encode each time.
		std::shared_ptr<e2ap::ControlRequest> creq = std::make_shared<e2ap::ControlRequest>(
                    e2ap.get_requestor_id(),e2ap.get_next_instance_id(),
		    1,sreq,e2ap::CONTROL_REQUEST_ACK);
		creq->set_meid(nodeb->getName());
		e2ap.send_control_request(creq,nodeb->getName());
	    }
	}
    }
    else if (rt == App::ResourceType::SliceResource) {
	Slice *slice = (Slice *)db[App::ResourceType::SliceResource][rname];

        e2sm::nexran::SliceDeleteRequest *sreq = \
	    new e2sm::nexran::SliceDeleteRequest(nexran,rname);

	for (auto it = db[ResourceType::NodeBResource].begin();
	     it != db[ResourceType::NodeBResource].end();
	     ++it) {
	    NodeB *nodeb = (NodeB *)it->second;

	    if (!nodeb->is_slice_bound(rname))
		continue;

	    // Each request needs a different RequestId, so we have to
	    // re-encode each time.
	    std::shared_ptr<e2ap::ControlRequest> creq = std::make_shared<e2ap::ControlRequest>(
                e2ap.get_requestor_id(),e2ap.get_next_instance_id(),
		1,sreq,e2ap::CONTROL_REQUEST_ACK);
	    creq->set_meid(nodeb->getName());
	    e2ap.send_control_request(creq,nodeb->getName());
	}

	slice->unbind_all_ues();
    }
    else if (rt == App::ResourceType::NodeBResource) {
	NodeB *nodeb = (NodeB *)db[App::ResourceType::NodeBResource][rname];
	std::map<std::string,Slice *>& slices = nodeb->get_slices();

	if (!slices.empty()) {
	    std::list<std::string> deletes;
	    for (auto it = slices.begin(); it != slices.end(); ++it)
		deletes.push_back(it->first);

	    e2sm::nexran::SliceDeleteRequest *sreq = \
		new e2sm::nexran::SliceDeleteRequest(nexran,deletes);

	    std::shared_ptr<e2ap::ControlRequest> creq = std::make_shared<e2ap::ControlRequest>(
                e2ap.get_requestor_id(),e2ap.get_next_instance_id(),
		1,sreq,e2ap::CONTROL_REQUEST_ACK);
	    creq->set_meid(nodeb->getName());
	    e2ap.send_control_request(creq,nodeb->getName());
	}
    }

    delete db[rt][rname];
    db[rt].erase(rname);

    mutex.unlock();

    return true;
}

bool App::update(ResourceType rt,std::string& rname,
		 rapidjson::Document& d,
		 AppError **ae)
{
    mutex.lock();
    if (db[rt].count(rname) < 1) {
	mutex.unlock();
	if (ae) {
	    if (!*ae)
		*ae = new AppError(404);
	    (*ae)->add(std::string("not found"));
	}
	return false;
    }

    if (!db[rt][rname]->update(d,ae)) {
	mutex.unlock();
	return false;
    }

    if (rt == App::ResourceType::SliceResource) {
	Slice *slice = (Slice *)db[App::ResourceType::SliceResource][rname];

	ProportionalAllocationPolicy *policy = dynamic_cast<ProportionalAllocationPolicy *>(slice->getPolicy());
	e2sm::nexran::ProportionalAllocationPolicy *npolicy = \
	    new e2sm::nexran::ProportionalAllocationPolicy(policy->getShare());
	e2sm::nexran::SliceConfig *sc = new e2sm::nexran::SliceConfig(slice->getName(),npolicy);
	e2sm::nexran::SliceConfigRequest *sreq = new e2sm::nexran::SliceConfigRequest(nexran,sc);
	sreq->encode();

	for (auto it = db[ResourceType::NodeBResource].begin();
	     it != db[ResourceType::NodeBResource].end();
	     ++it) {
	    NodeB *nodeb = (NodeB *)it->second;

	    if (!nodeb->is_slice_bound(rname))
		continue;

	    // Each request needs a different RequestId, so we have to
	    // re-encode each time.
	    std::shared_ptr<e2ap::ControlRequest> creq = std::make_shared<e2ap::ControlRequest>(
                e2ap.get_requestor_id(),e2ap.get_next_instance_id(),
		1,sreq,e2ap::CONTROL_REQUEST_ACK);
	    creq->set_meid(nodeb->getName());
	    e2ap.send_control_request(creq,nodeb->getName());
	}
    }

    mutex.unlock();

    mdclog_write(MDCLOG_DEBUG,"updated %s %s",
		 rtype_to_label[rt],rname.c_str());

    return true;
}

bool App::bind_slice_nodeb(std::string& slice_name,std::string& nodeb_name,
			   AppError **ae)
{
    mutex.lock();
    if (db[App::ResourceType::SliceResource].count(slice_name) < 1) {
	mutex.unlock();
	if (ae) {
	    if (!*ae)
		*ae = new AppError(404);
	    (*ae)->add(std::string("slice does not exist"));
	}
	return false;
    }
    if (db[App::ResourceType::NodeBResource].count(nodeb_name) < 1) {
	mutex.unlock();
	if (ae) {
	    if (!*ae)
		*ae = new AppError(404);
	    (*ae)->add(std::string("nodeb does not exist"));
	}
	return false;
    }
    Slice *slice = (Slice *)db[App::ResourceType::SliceResource][slice_name];
    NodeB *nodeb = (NodeB *)db[App::ResourceType::NodeBResource][nodeb_name];
	
    if (!nodeb->bind_slice(slice)) {
	mutex.unlock();
	if (ae) {
	    if (!*ae)
		*ae = new AppError(403);
	    (*ae)->add(std::string("slice already bound to nodeb"));
	}
	return false;
    }

    ProportionalAllocationPolicy *policy = dynamic_cast<ProportionalAllocationPolicy *>(slice->getPolicy());
    e2sm::nexran::ProportionalAllocationPolicy *npolicy = \
	new e2sm::nexran::ProportionalAllocationPolicy(policy->getShare());
    e2sm::nexran::SliceConfig *sc = new e2sm::nexran::SliceConfig(slice->getName(),npolicy);
    e2sm::nexran::SliceConfigRequest *sreq = new e2sm::nexran::SliceConfigRequest(nexran,sc);
    std::shared_ptr<e2ap::ControlRequest> creq = std::make_shared<e2ap::ControlRequest>(
        e2ap.get_requestor_id(),e2ap.get_next_instance_id(),
	1,sreq,e2ap::CONTROL_REQUEST_ACK);
    creq->set_meid(nodeb->getName());
    e2ap.send_control_request(creq,nodeb->getName());

    mutex.unlock();

    mdclog_write(MDCLOG_DEBUG,"bound slice %s to nodeb %s",
		 slice_name.c_str(),nodeb->getName().c_str());

    return true;
}

bool App::unbind_slice_nodeb(std::string& slice_name,std::string& nodeb_name,
			     AppError **ae)
{
    mutex.lock();
    if (db[App::ResourceType::SliceResource].count(slice_name) < 1) {
	mutex.unlock();
	if (ae) {
	    if (!*ae)
		*ae = new AppError(404);
	    (*ae)->add(std::string("slice does not exist"));
	}
	return false;
    }
    if (db[App::ResourceType::NodeBResource].count(nodeb_name) < 1) {
	mutex.unlock();
	if (ae) {
	    if (!*ae)
		*ae = new AppError(404);
	    (*ae)->add(std::string("nodeb does not exist"));
	}
	return false;
    }
    NodeB *nodeb = (NodeB *)db[App::ResourceType::NodeBResource][nodeb_name];
	
    if (!nodeb->unbind_slice(slice_name)) {
	mutex.unlock();
	if (ae) {
	    if (!*ae)
		*ae = new AppError(404);
	    (*ae)->add(std::string("slice not bound to this nodeb"));
	}
	return false;
    }

    e2sm::nexran::SliceDeleteRequest *sreq = \
	new e2sm::nexran::SliceDeleteRequest(nexran,slice_name);
    std::shared_ptr<e2ap::ControlRequest> creq = std::make_shared<e2ap::ControlRequest>(
        e2ap.get_requestor_id(),e2ap.get_next_instance_id(),
	1,sreq,e2ap::CONTROL_REQUEST_ACK);
    creq->set_meid(nodeb->getName());
    e2ap.send_control_request(creq,nodeb->getName());

    mutex.unlock();

    mdclog_write(MDCLOG_DEBUG,"unbound slice %s from nodeb %s",
		 slice_name.c_str(),nodeb->getName().c_str());

    return true;
}

bool App::bind_ue_slice(std::string& imsi,std::string& slice_name,
			AppError **ae)
{
    mutex.lock();
    if (db[App::ResourceType::SliceResource].count(slice_name) < 1) {
	mutex.unlock();
	if (ae) {
	    if (!*ae)
		*ae = new AppError(404);
	    (*ae)->add(std::string("slice does not exist"));
	}
	return false;
    }
    if (db[App::ResourceType::UeResource].count(imsi) < 1) {
	mutex.unlock();
	if (ae) {
	    if (!*ae)
		*ae = new AppError(404);
	    (*ae)->add(std::string("ue does not exist"));
	}
	return false;
    }
    Ue *ue = (Ue *)db[App::ResourceType::UeResource][imsi];
    Slice *slice = (Slice *)db[App::ResourceType::SliceResource][slice_name];
	
    if (ue->is_bound() || !slice->bind_ue(ue)) {
	mutex.unlock();
	if (ae) {
	    if (!*ae)
		*ae = new AppError(403);
	    (*ae)->add(std::string("ue already bound to slice"));
	}
	return false;
    }
    ue->bind_slice(slice_name);

    e2sm::nexran::SliceUeBindRequest *sreq = \
	new e2sm::nexran::SliceUeBindRequest(nexran,slice->getName(),ue->getName());

    for (auto it = db[ResourceType::NodeBResource].begin();
	 it != db[ResourceType::NodeBResource].end();
	 ++it) {
	NodeB *nodeb = (NodeB *)it->second;

	if (!nodeb->is_slice_bound(slice_name))
	    continue;

	// Each request needs a different RequestId, so we have to
	// re-encode each time.
	std::shared_ptr<e2ap::ControlRequest> creq = std::make_shared<e2ap::ControlRequest>(
            e2ap.get_requestor_id(),e2ap.get_next_instance_id(),
	    1,sreq,e2ap::CONTROL_REQUEST_ACK);
	creq->set_meid(nodeb->getName());
	e2ap.send_control_request(creq,nodeb->getName());
    }

    mutex.unlock();

    mdclog_write(MDCLOG_DEBUG,"bound ue %s to nodeb %s",
		 imsi.c_str(),slice_name.c_str());

    return true;
}

bool App::unbind_ue_slice(std::string& imsi,std::string& slice_name,
			  AppError **ae)
{
    mutex.lock();
    if (db[App::ResourceType::SliceResource].count(slice_name) < 1) {
	mutex.unlock();
	if (ae) {
	    if (!*ae)
		*ae = new AppError(404);
	    (*ae)->add(std::string("slice does not exist"));
	}
	return false;
    }
    if (db[App::ResourceType::UeResource].count(imsi) < 1) {
	mutex.unlock();
	if (ae) {
	    if (!*ae)
		*ae = new AppError(404);
	    (*ae)->add(std::string("ue does not exist"));
	}
	return false;
    }
    Slice *slice = (Slice *)db[App::ResourceType::SliceResource][slice_name];

    if (!slice->unbind_ue(imsi)) {
	mutex.unlock();
	if (ae) {
	    if (!*ae)
		*ae = new AppError(404);
	    (*ae)->add(std::string("ue not bound to this slice"));
	}
	return false;
    }

    e2sm::nexran::SliceUeUnbindRequest *sreq = \
	new e2sm::nexran::SliceUeUnbindRequest(nexran,slice->getName(),imsi);

    for (auto it = db[ResourceType::NodeBResource].begin();
	 it != db[ResourceType::NodeBResource].end();
	 ++it) {
	NodeB *nodeb = (NodeB *)it->second;

	if (!nodeb->is_slice_bound(slice_name))
	    continue;

	// Each request needs a different RequestId, so we have to
	// re-encode each time.
	std::shared_ptr<e2ap::ControlRequest> creq = std::make_shared<e2ap::ControlRequest>(
            e2ap.get_requestor_id(),e2ap.get_next_instance_id(),
	    1,sreq,e2ap::CONTROL_REQUEST_ACK);
	creq->set_meid(nodeb->getName());
	e2ap.send_control_request(creq,nodeb->getName());
    }

    mutex.unlock();

    mdclog_write(MDCLOG_DEBUG,"unbound ue %s from nodeb %s",
		 imsi.c_str(),slice_name.c_str());

    return true;
}

}
