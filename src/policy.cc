
#include "mdclog/mdclog.h"

#include "nexran.h"

namespace nexran {

std::map<AllocationPolicy::Type,const char *> AllocationPolicy::type_to_string = {
    { Proportional, "proportional" },
};

int ProportionalAllocationPolicy::maybeEndThrottling()
{
    if (!isThrottling())
	return -1;
    else if (throttle && std::time(nullptr) < throttle_end)
	return -1;

    int ret = throttle_saved_share;
    is_throttling = false;
    throttle_end = 0;
    throttle_saved_share = -1;

    // Caller must call setShare() on the new value.
    return ret;
    
}

int ProportionalAllocationPolicy::maybeStartThrottling()
{
    if (!throttle)
	return -1;
    else if (isThrottling())
	return -1;
    else if (metrics.get_total_bytes() < throttle_threshold)
	return -1;

    throttle_saved_share = share;
    is_throttling = true;
    throttle_end = std::time(nullptr) + throttle_period;

    // Caller must call setShare() on the new value.
    if (throttle_share > 0)
	return throttle_share;
    else if (throttle_target > 0) {
	e2sm::kpm::entity_metrics_t *c = metrics.current();
	if (!c)
	    return -1;
	int tbytes = c->dl_bytes + c->ul_bytes;
	int nshare = (int)(share / (tbytes / (float)throttle_target));
	mdclog_write(MDCLOG_DEBUG,"maybeStartThrottling: target=%d, currentbytes=%d, new share=%d",
		     throttle_target, tbytes, nshare);
	return nshare;
    }
    else
	return -1;
}

int ProportionalAllocationPolicy::maybeUpdateThrottling()
{
    if (!throttle)
	return -1;
    else if (!isThrottling())
	return -1;
    else if (throttle_share > 0)
	return throttle_share;
    else if (throttle_target > 0) {
	e2sm::kpm::entity_metrics_t *c = metrics.current();
	if (!c)
	    return -1;
	int tbytes = c->dl_bytes + c->ul_bytes;
	int nshare = (int)(share / (tbytes / (float)throttle_target));
	mdclog_write(MDCLOG_DEBUG,"maybeUpdateThrottling: target=%d, currentbytes=%d, new share=%d",
		     throttle_target, tbytes, nshare);
	return nshare;
    }
    else
	return -1;
}

bool ProportionalAllocationPolicy::update(const rapidjson::Value& obj,AppError **ae)
{
    if (!obj.IsObject()) {
	if (ae) {
	    if (!*ae)
		*ae = new AppError(400);
	    (*ae)->add(std::string("request is not an object"));
	}
	return NULL;
    }

    if (!obj.HasMember("type")
	|| !obj["type"].IsString()
	|| strcmp(obj["type"].GetString(),getName()) != 0
	|| !obj.HasMember("share")
	|| !obj["share"].IsInt()
	|| obj["share"].GetInt() < 0
	|| obj["share"].GetInt() > 1024) {
	if (ae) {
	    if (!*ae)
		*ae = new AppError(400);
	    (*ae)->add(std::string("malformed allocation_policy property"));
	}
	return NULL;
    }

    if ((obj.HasMember("auto_equalize")
	 && !obj["auto_equalize"].IsBool())
	|| (obj.HasMember("throttle")
	    && !obj["throttle"].IsBool())) {
	if (ae) {
	    if (!*ae)
		*ae = new AppError(400);
	    (*ae)->add(std::string("malformed allocation_policy property"));
	}
	return NULL;
    }

    share = obj["share"].GetInt();
    if (obj.HasMember("auto_equalize"))
	auto_equalize = obj["auto_equalize"].GetBool();
    if (obj.HasMember("throttle"))
	throttle = obj["throttle"].GetBool();

    return true;
}

}
