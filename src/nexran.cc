
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
#include "e2sm_zylinium.h"

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
    bool retval = false;

    mdclog_write(MDCLOG_DEBUG,"nexran Indication handler");
    if (ind->model) {
	e2sm::kpm::KpmIndication *kind;
	e2sm::nexran::SliceStatusIndication *nind;
	e2sm::zylinium::MaskStatusIndication *zind;

	if ((kind = dynamic_cast<e2sm::kpm::KpmIndication *>(ind->model)) != NULL)
	    retval = handle(kind);
	else if ((nind = dynamic_cast<e2sm::nexran::SliceStatusIndication *>(ind->model)) != NULL)
	    retval = handle(nind);
	else if ((zind = dynamic_cast<e2sm::zylinium::MaskStatusIndication *>(ind->model)) != NULL)
	    retval = handle(zind);
    }

    delete ind;
    return retval;
}

bool App::handle(e2ap::ErrorIndication *ind)
{
    mdclog_write(MDCLOG_DEBUG,"nexran ErrorIndication handler");
}

bool App::handle(e2sm::nexran::SliceStatusIndication *ind)
{
    mdclog_write(MDCLOG_DEBUG,"nexran SliceStatusIndication handler");
    std::string rname;

    if (ind->parent && ind->parent->subscription_request != nullptr) {
	rname = ind->parent->subscription_request->meid;
	mutex.lock();
	if (db[ResourceType::NodeBResource].count(rname) > 0) {
	    NodeB *nodeb = (NodeB *)db[App::ResourceType::NodeBResource][rname];
	    nodeb->update_last_indication();
	}
	mutex.unlock();
    }
}

bool App::handle(e2sm::kpm::KpmIndication *kind)
{
    mdclog_write(MDCLOG_INFO,"KpmIndication: %s",
		 kind->report->to_string('\n',',').c_str());

    e2sm::kpm::KpmReport *report = kind->report;
    std::string rname;

    if (kind->parent && kind->parent->subscription_request != nullptr) {
	rname = kind->parent->subscription_request->meid;
	mutex.lock();
	if (db[ResourceType::NodeBResource].count(rname) > 0) {
	    NodeB *nodeb = (NodeB *)db[App::ResourceType::NodeBResource][rname];
	    nodeb->update_config(report->available_ul_prbs);
	    nodeb->update_last_indication();
	}
	mutex.unlock();
    }

    if (influxdb && (report->slices.size() || report->ues.size())) {
	influxdb->batchOf(report->slices.size() + report->ues.size());
	for (auto it = report->slices.begin(); it != report->slices.end(); ++it) {
	    influxdb->write(influxdb::Point{"slice"}
		.addField("dl_bytes", (long long int)it->second.dl_bytes)
		.addField("ul_bytes", (long long int)it->second.ul_bytes)
		.addField("dl_prbs", (long long int)it->second.dl_prbs)
		.addField("ul_prbs", (long long int)it->second.ul_prbs)
		.addField("tx_pkts", (long long int)it->second.tx_pkts)
		.addField("tx_errors", (long long int)it->second.tx_errors)
		.addField("tx_brate", (long long int)it->second.tx_brate)
		.addField("rx_pkts", (long long int)it->second.rx_pkts)
		.addField("rx_errors", (long long int)it->second.rx_errors)
		.addField("rx_brate", (long long int)it->second.rx_brate)
		.addField("dl_cqi", it->second.dl_cqi)
		.addField("dl_ri", it->second.dl_ri)
		.addField("dl_pmi", it->second.dl_pmi)
		.addField("ul_phr", it->second.ul_phr)
		.addField("ul_sinr", it->second.ul_sinr)
		.addField("ul_mcs", it->second.ul_mcs)
		.addField("ul_samples", (long long int)it->second.ul_samples)
		.addField("dl_mcs", it->second.dl_mcs)
		.addField("dl_samples", (long long int)it->second.dl_samples)
		.addTag("slice", it->first.c_str())
		.addTag("nodeb", rname.c_str()));
	}
	for (auto it = report->ues.begin(); it != report->ues.end(); ++it) {
	    influxdb->write(influxdb::Point{"ue"}
		.addField("dl_bytes", (long long int)it->second.dl_bytes)
		.addField("ul_bytes", (long long int)it->second.ul_bytes)
		.addField("dl_prbs", (long long int)it->second.dl_prbs)
		.addField("ul_prbs", (long long int)it->second.ul_prbs)
		.addField("tx_pkts", (long long int)it->second.tx_pkts)
		.addField("tx_errors", (long long int)it->second.tx_errors)
		.addField("tx_brate", (long long int)it->second.tx_brate)
		.addField("rx_pkts", (long long int)it->second.rx_pkts)
		.addField("rx_errors", (long long int)it->second.rx_errors)
		.addField("rx_brate", (long long int)it->second.rx_brate)
		.addField("dl_cqi", it->second.dl_cqi)
		.addField("dl_ri", it->second.dl_ri)
		.addField("dl_pmi", it->second.dl_pmi)
		.addField("ul_phr", it->second.ul_phr)
		.addField("ul_sinr", it->second.ul_sinr)
		.addField("ul_mcs", it->second.ul_mcs)
		.addField("ul_samples", (long long int)it->second.ul_samples)
		.addField("dl_mcs", it->second.dl_mcs)
		.addField("dl_samples", (long long int)it->second.dl_samples)
		.addTag("ue", std::to_string(it->first).c_str())
		.addTag("nodeb", rname.c_str()));
	}
	try {
	    influxdb->flushBatch();
	}
	catch (...) {
	    mdclog_write(MDCLOG_ERR,"failed to write KPM points to influxdb");
	}
    }

    // If we don't have BW reports for all slices, do not modify proportions?
    // Add up all slice dl_bytes, get proportions
    // map those to share proportions
    // figure out the delta for each slice to equalize
    // 10, 5, 10 // 512, 512, 512
    // new target: (25/3)=8.333... for each
    // 25/3-10=-1.667, 25/3-5=3.333, 25/3-10=-1.667
    // -1.667/10 == -16.67% (apply that to current share, so remove 16.67%
    // of its share, and so on)
    // new shares: 512+(512*-.1667) = 426
    //             512+(512*.6667) = 853
    //             512+(512*-.1667) = 426
    // If all share factors are within 5%, make no changes.
    
    // ugh, this is also assuming a single nodeb, because our slice
    // share policy is global, not per nodeb
    
    // but -- we could also keep a per-nodeb specialization :-D
    // maybe hide it from user, who knows

    if (report->slices.size() == 0) {
	mdclog_write(MDCLOG_DEBUG,"no slices in KPM report; not autoequalizing");
	if (mdclog_level_get() == MDCLOG_DEBUG) {
	    mutex.lock();
	    std::stringstream ss;
	    for (auto it = db[ResourceType::SliceResource].begin();
		 it != db[ResourceType::SliceResource].end();
		 ++it) {
		Slice *slice = (Slice *)it->second;
		if (dynamic_cast<ProportionalAllocationPolicy *>(slice->getPolicy()) == NULL)
		    continue;
		ProportionalAllocationPolicy *policy = \
		    dynamic_cast<ProportionalAllocationPolicy *>(slice->getPolicy());
		if (false && !policy->isAutoEqualized())
		    continue;
		ss << it->first << "[share=" << policy->getShare() << "]" << std::endl;
	    }
	    mutex.unlock();
	    mdclog_write(MDCLOG_DEBUG,"current shares: %s",ss.str().c_str());
	}
	return true;
    }

    // Have to lock at this point; we're going to iterate over
    // and possibly adjust the slice proportions.
    mutex.lock();

    // Index some stuff locally for easier iteration; save share modification factors.
    std::map<std::string,Slice *> slices;
    std::map<std::string,Slice *> report_slices;
    std::map<std::string,ProportionalAllocationPolicy *> policies;
    std::map<std::string,float> new_share_factors;
    //std::map<std::string,int> new_shares;
    uint64_t slices_total = 0;
    uint64_t slices_prb_total = 0;

    // Create local indexes.
    for (auto it = db[ResourceType::SliceResource].begin(); it != db[ResourceType::SliceResource].end(); ++it) {
	std::string slice_name = it->first;
	Slice *slice = (Slice *)it->second;
	ProportionalAllocationPolicy *policy = \
	    dynamic_cast<ProportionalAllocationPolicy *>(slice->getPolicy());
	if (!policy)
	    continue;

	slices[slice_name] = slice;
	policies[slice_name] = policy;
	// placeholder until later iteration
	new_share_factors[slice_name] = 0.0f;
    }

    int num_autoeq_slices = 0;
    for (auto it = report->slices.begin(); it != report->slices.end(); ++it) {
	std::string slice_name = it->first;
	// If this is not a slice we know of, ignore.
	if (slices.count(slice_name) == 0)
	    continue;
	Slice *slice = (Slice *)slices[slice_name];
	ProportionalAllocationPolicy *policy = \
	    dynamic_cast<ProportionalAllocationPolicy *>(slice->getPolicy());
	if (!policy)
	    continue;

	report_slices[slice_name] = slice;

	// NB: only auto-eq amongst metrics for auto-eq'd slices.
	// XXX: is this right?
	if (policy->isAutoEqualized()) {
	    slices_total += it->second.dl_bytes;
	    slices_prb_total += it->second.dl_prbs;
	    ++num_autoeq_slices;
	}

	policy->getMetrics().add(it->second);
    }

    // First, check if any slices should be released from throttling.
    for (auto it = slices.begin(); it != slices.end(); ++it) {
	std::string slice_name = it->first;
	Slice *slice = (Slice *)it->second;
	ProportionalAllocationPolicy *policy = policies[slice_name];
	if (!policy->isThrottled() || !policy->isThrottling())
	    continue;

	// Ensure we flush old metrics, even if we didn't add any new ones
	// from the current report.
	policy->getMetrics().flush();

	int new_share = policy->maybeEndThrottling();
	if (new_share > -1) {
	    mdclog_write(MDCLOG_DEBUG,"stopping throttling slice '%s' (%d -> %d)",
			 slice_name.c_str(),policy->getShare(),new_share);
	    // NB: we just compute a share factor, because that's what the
	    // auto-equalizing code cares about.
	    // nshare = cshare + (cshare * f)
	    // n - c = c * f
	    // (n - c) / c = f
	    // 110 = 100 + 100 * .1
	    // (110 - 100) / 100 = f
	    int cur_share = policy->getShare();
	    new_share_factors[slice_name] = (new_share - cur_share) / (float)cur_share;
	}
	else {
	    new_share = policy->maybeUpdateThrottling();
	    if (new_share > -1 && new_share != policy->getShare()) {
		int cur_share = policy->getShare();
		new_share_factors[slice_name] = (new_share - cur_share) / (float)cur_share;
	    }
	}
    }

    // Second, check if any slices should be newly throttled.
    for (auto it = slices.begin(); it != slices.end(); ++it) {
	std::string slice_name = it->first;
	Slice *slice = (Slice *)it->second;
	ProportionalAllocationPolicy *policy = policies[slice_name];
	if (!policy->isThrottled() || policy->isThrottling())
	    continue;

	// Ensure we flush old metrics, even if we didn't add any new ones
	// from the current report.
	e2sm::kpm::MetricsIndex& metrics = policy->getMetrics();
	mdclog_write(MDCLOG_DEBUG,"considering throttle start for slice '%s': %ld (%d)",
		     slice_name.c_str(),metrics.get_total_bytes(),metrics.size());
	metrics.flush();
	mdclog_write(MDCLOG_DEBUG,"considering throttle start for slice '%s': %ld (%d) (post flush)",
		     slice_name.c_str(),metrics.get_total_bytes(),metrics.size());

	int new_share = policy->maybeStartThrottling();
	if (new_share > -1) {
	    mdclog_write(MDCLOG_DEBUG,"starting throttling slice '%s' (%d -> %d)",
			 slice_name.c_str(),policy->getShare(),new_share);
	    // NB: we just compute a share factor, because that's what the
	    // auto-equalizing code cares about.
	    int cur_share = policy->getShare();
	    new_share_factors[slice_name] = (new_share - cur_share) / (float)cur_share;
	}
    }

    // Begin auto-equalize checks.
    bool any_above_threshold = false;
    uint64_t available_prbs_per_slice = 0;
    if (num_autoeq_slices > 0)
	available_prbs_per_slice = (report->period_ms * 2 * report->available_dl_prbs) / num_autoeq_slices;
    uint64_t prb_threshold = (uint64_t)(0.15f * available_prbs_per_slice);

    if (available_prbs_per_slice > 0) {
	// Check PRB utilization.  If no slice has utilized at least 15% of an
	// even PRB allocation, do nothing.
	for (auto it = report_slices.begin(); it != report_slices.end(); ++it) {
	    std::string slice_name = it->first;
	    uint64_t dl_prbs = report->slices[slice_name].dl_prbs;

	    if (dl_prbs > prb_threshold) {
		any_above_threshold = true;
		break;
	    }
	}
    }

    // Create the new share factors.
    if (any_above_threshold) {
	mdclog_write(MDCLOG_INFO,"PRB utilization threshold (%lu/%lu) reached; checking for new share factors",
		     prb_threshold,available_prbs_per_slice);
	any_above_threshold = false;
	for (auto it = report_slices.begin(); it != report_slices.end(); ++it) {
	    std::string slice_name = it->first;
	    uint64_t dl_bytes = report->slices[slice_name].dl_bytes;
	    ProportionalAllocationPolicy *policy = policies[slice_name];
	    if (!policy->isAutoEqualized()) {
		mdclog_write(MDCLOG_DEBUG,"skipping slice '%s'; not autoequalized",
			     slice_name.c_str());
		continue;
	    }

	    if (new_share_factors[slice_name] != 0.0f) {
		mdclog_write(MDCLOG_DEBUG,"skipping slice '%s' with existing new_share_factor %f",
			     slice_name.c_str(),new_share_factors[slice_name]);
		continue;
	    }

	    float nf = ((float)slices_total / num_autoeq_slices - dl_bytes) / dl_bytes;
	    if (nf > 0.05f || nf < -0.05f) {
		any_above_threshold = true;
		mdclog_write(MDCLOG_DEBUG,"candidate proportional share factor above threshold (%s): %f",
			     slice_name.c_str(),nf);
	    }
	    else
		mdclog_write(MDCLOG_DEBUG,"candidate proportional share factor below threshold (%s): %f",
			     slice_name.c_str(),nf);

	}
    }
    // XXX: have to do a second time through the loop to actually set the
    // new share factors, sigh
    if (any_above_threshold) {
	for (auto it = report->slices.begin(); it != report->slices.end(); ++it) {
	    std::string slice_name = it->first;
	    uint64_t dl_bytes = report->slices[slice_name].dl_bytes;
	    if (policies.count(slice_name) == 0)
		continue;
	    ProportionalAllocationPolicy *policy = policies[slice_name];
	    if (!policy->isAutoEqualized())
		continue;

	    if (new_share_factors[slice_name] != 0.0f)
		continue;

	    new_share_factors[slice_name] = ((float)slices_total / num_autoeq_slices - dl_bytes) / dl_bytes;
	    mdclog_write(MDCLOG_DEBUG,"new proportional share factor (%s): %f",
			 slice_name.c_str(),new_share_factors[slice_name]);
	}
    }

    // Handle any updates; log either way.
    for (auto it = new_share_factors.begin(); it != new_share_factors.end(); ++it) {
	std::string slice_name = it->first;
	// Update the policies and push out control messages to bound
	// nodebs.
	Slice *slice = slices[slice_name];
	ProportionalAllocationPolicy *policy = policies[slice_name];
	int cshare = policy->getShare();
	int nshare = std::min((int)(cshare + (cshare * it->second)),1024);
	if (nshare < 1 || nshare < 64)
	    //nshare = 1;
	    nshare = 64;
	if (influxdb) {
	    influxdb->write(influxdb::Point{"share"}
		.addField("share", nshare)
		.addTag("slice", slice_name.c_str()));
	}
	policy->setShare(nshare);
	if (cshare == nshare) {
	    mdclog_write(MDCLOG_INFO,"slice '%s' share unchanged: %d",
			 slice_name.c_str(),nshare);
	    continue;
	}
	mdclog_write(MDCLOG_INFO,"slice '%s' share: %d -> %d",
		     slice_name.c_str(),cshare,nshare);
	e2sm::nexran::ProportionalAllocationPolicy *npolicy = \
	    new e2sm::nexran::ProportionalAllocationPolicy(nshare);
	e2sm::nexran::SliceConfig *sc = new e2sm::nexran::SliceConfig(slice_name,npolicy);
	e2sm::nexran::SliceConfigRequest *sreq = new e2sm::nexran::SliceConfigRequest(nexran,sc);
	sreq->encode();

	for (auto it2 = db[ResourceType::NodeBResource].begin();
	     it2 != db[ResourceType::NodeBResource].end();
	     ++it2) {
	    NodeB *nodeb = (NodeB *)it2->second;

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

    mutex.unlock();
    return true;
}

bool App::handle(e2sm::zylinium::MaskStatusIndication *ind)
{
    mdclog_write(MDCLOG_INFO,"MaskStatusIndication: %s",
		 ind->report->to_string('\n',',').c_str());

    std::string rname;
    int total_prbs = -1;
    if (ind->parent && ind->parent->subscription_request != nullptr) {
	mutex.lock();
	rname = ind->parent->subscription_request->meid;
	if (db[ResourceType::NodeBResource].count(rname) > 0) {
	    NodeB *nodeb = (NodeB *)db[App::ResourceType::NodeBResource][rname];
	    nodeb->update_mask_from_status(ind->report);
	    nodeb->update_last_indication();
	    total_prbs = nodeb->get_total_prbs();
	}
	mutex.unlock();
    }

    if (influxdb && total_prbs > 0) {
	int dl_masked_bits = ind->report->dl_mask.get_masked_bit_count(total_prbs);
	int ul_masked_bits = ind->report->ul_mask.get_masked_bit_count(total_prbs);
	int dl_available_bits = ind->report->dl_mask.get_available_bit_count(total_prbs);
	int ul_available_bits = ind->report->ul_mask.get_available_bit_count(total_prbs);

	influxdb->write(influxdb::Point{"masks"}
	    .addField("dl_masked_bits", (long long int)dl_masked_bits)
	    .addField("ul_masked_bits", (long long int)ul_masked_bits)
	    .addField("dl_available_bits", (long long int)dl_available_bits)
	    .addField("ul_available_bits", (long long int)ul_available_bits)
            .addTag("nodeb", rname.c_str()));
    }

    return true;
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
    should_stop = true;
    rmr_thread->join();
    delete rmr_thread;
    rmr_thread = NULL;
    response_thread->join();
    delete response_thread;
    response_thread = NULL;
    running = false;
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
	NodeB *nodeb = (NodeB *)resource;

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
	    new e2sm::kpm::EventTrigger(kpm,app_config.kpm_interval_index);
	std::list<e2ap::Action *> actions;
	actions.push_back(new e2ap::Action(1,e2ap::ACTION_REPORT,NULL,-1));
	std::shared_ptr<e2ap::SubscriptionRequest> req = \
	    std::make_shared<e2ap::SubscriptionRequest>(
		e2ap.get_requestor_id(),e2ap.get_next_instance_id(),
		0,trigger,actions);
	req->set_meid(rname);
	e2ap.send_subscription_request(req,rname);

	e2sm::zylinium::MaskStatusRequest *msreq = \
	    new e2sm::zylinium::MaskStatusRequest(zylinium);
	std::shared_ptr<e2ap::ControlRequest> mscreq = std::make_shared<e2ap::ControlRequest>(
            e2ap.get_requestor_id(),e2ap.get_next_instance_id(),
	    2,msreq,e2ap::CONTROL_REQUEST_ACK);
	mscreq->set_meid(rname);
	e2ap.send_control_request(mscreq,rname);

	e2sm::zylinium::EventTrigger *ztrigger = \
	    new e2sm::zylinium::EventTrigger(zylinium);
	std::list<e2ap::Action *> zactions;
	zactions.push_back(new e2ap::Action(1,e2ap::ACTION_REPORT,NULL,-1));
	std::shared_ptr<e2ap::SubscriptionRequest> zreq = \
	    std::make_shared<e2ap::SubscriptionRequest>(
		e2ap.get_requestor_id(),e2ap.get_next_instance_id(),
		2,ztrigger,zactions);
	zreq->set_meid(rname);
	e2ap.send_subscription_request(zreq,rname);

	e2sm::zylinium::MaskConfigRequest *mcreq = \
	    nodeb->make_mask_config_request(zylinium);
	std::shared_ptr<e2ap::ControlRequest> mccreq = std::make_shared<e2ap::ControlRequest>(
            e2ap.get_requestor_id(),e2ap.get_next_instance_id(),
	    2,mcreq,e2ap::CONTROL_REQUEST_ACK);
	mccreq->set_meid(rname);
	e2ap.send_control_request(mccreq,rname);
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

	e2ap.delete_all_subscriptions(rname);
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

    if (rt == App::ResourceType::NodeBResource) {
	NodeB *nodeb = (NodeB *)db[App::ResourceType::NodeBResource][rname];

	e2sm::zylinium::MaskConfigRequest *mreq = \
	    nodeb->make_mask_config_request(zylinium);
	std::shared_ptr<e2ap::ControlRequest> creq = \
	    std::make_shared<e2ap::ControlRequest>(
            e2ap.get_requestor_id(),e2ap.get_next_instance_id(),
	    2,mreq,e2ap::CONTROL_REQUEST_ACK);
	creq->set_meid(rname);
	e2ap.send_control_request(creq,rname);
    }
    else if (rt == App::ResourceType::SliceResource) {
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

bool App::handle_appconfig_update(void)
{
    if (app_config.influxdb_url != influxdb_url) {
	if (influxdb)
	    influxdb.reset(nullptr);
	if (app_config.influxdb_url.length() > 0) {
	    influxdb = influxdb::InfluxDBFactory::Get(app_config.influxdb_url.c_str());
	    influxdb_url = app_config.influxdb_url;
	}
	else {
	    influxdb_url = std::string();
	}
    }

    return true;
}

}
