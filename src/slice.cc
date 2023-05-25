
#include "nexran.h"

namespace nexran {

std::map<std::string,JsonTypeMap> Slice::propertyTypes = {
    { "name",JsonTypeMap::STRING },
    { "allocation_policy",JsonTypeMap::OBJECT },
};
std::map<std::string,std::list<std::string>> Slice::propertyEnums = {};
std::map<HttpMethod,std::list<std::string>> Slice::required = {
    { HttpMethod::POST,{ "name" } }
};
std::map<HttpMethod,std::list<std::string>> Slice::optional = {};
std::map<HttpMethod,std::list<std::string>> Slice::disallowed = {
    { HttpMethod::PUT,{ "name" } }
};

void Slice::serialize(rapidjson::Writer<rapidjson::StringBuffer>& writer)
{
    writer.StartObject();
    writer.String("name");
    writer.String(name.c_str());
    if (allocation_policy) {
	writer.String("allocation_policy");
	allocation_policy->serialize(writer);
    }
    writer.String("ues");
    writer.StartArray();
    for (auto it = ues.begin(); it != ues.end(); ++it) {
    	writer.String(it->first.c_str());
    }
    writer.EndArray();
    writer.EndObject();
};

Slice *Slice::create(rapidjson::Document& d,AppError **ae)
{
    if (!d.IsObject()) {
	if (ae) {
	    if (!*ae)
		*ae = new AppError(400);
	    (*ae)->add(std::string("request is not an object"));
	}
	return NULL;
    }
    
    const rapidjson::Value& obj = d.GetObject();

    if (!Slice::validate_json(HttpMethod::POST,obj,ae))
	return NULL;

    AllocationPolicy *allocation_policy = NULL;
    if (obj.HasMember("allocation_policy")) {
	if (!obj["allocation_policy"].IsObject()
	    || !obj["allocation_policy"].HasMember("type")
	    || !obj["allocation_policy"]["type"].IsString()
	    || strcmp(obj["allocation_policy"]["type"].GetString(),
		      "proportional") != 0
	    || !obj["allocation_policy"].HasMember("share")
	    || !obj["allocation_policy"]["share"].IsInt()
	    || obj["allocation_policy"]["share"].GetInt() < 0
	    || obj["allocation_policy"]["share"].GetInt() > 1024
	    || (obj["allocation_policy"].HasMember("auto_equalize")
		&& !obj["allocation_policy"]["auto_equalize"].IsBool())) {
	    if (ae) {
		if (!*ae)
		    *ae = new AppError(400);
		(*ae)->add(std::string("malformed allocation_policy property"));
	    }
	    return NULL;
	}

	bool auto_equalize = false;
	if (obj["allocation_policy"].HasMember("auto_equalize"))
	    auto_equalize = obj["allocation_policy"]["auto_equalize"].GetBool();
	bool throttle = false;
	int throttle_threshold = -1;
	int throttle_period = 1800;
	int throttle_share = 0;
	int throttle_target = 0;
	if (obj["allocation_policy"].HasMember("throttle"))
	    throttle = obj["allocation_policy"]["throttle"].GetBool();
	if (obj["allocation_policy"].HasMember("throttle_threshold"))
	    throttle_threshold = obj["allocation_policy"]["throttle_threshold"].GetInt();
	if (obj["allocation_policy"].HasMember("throttle_period"))
	    throttle_period = obj["allocation_policy"]["throttle_period"].GetInt();
	if (obj["allocation_policy"].HasMember("throttle_share"))
	    throttle_share = obj["allocation_policy"]["throttle_share"].GetInt();
	if (obj["allocation_policy"].HasMember("throttle_target"))
	    throttle_target = obj["allocation_policy"]["throttle_target"].GetInt();

	if (!throttle_share && !throttle_target)
	    throttle_share = 128;

	allocation_policy = new ProportionalAllocationPolicy(
	    obj["allocation_policy"]["share"].GetInt(),auto_equalize,
	    throttle,throttle_threshold,throttle_period,throttle_share,
	    throttle_target);
    }
    if (allocation_policy)
	return new Slice(std::string(obj["name"].GetString()),
			 allocation_policy);
    else
	return new Slice(std::string(obj["name"].GetString()));
}

bool Slice::update(rapidjson::Document& d,AppError **ae)
{
    if (!d.IsObject()) {
	if (ae) {
	    if (!*ae)
		*ae = new AppError(400);
	    (*ae)->add(std::string("request is not an object"));
	}
	return NULL;
    }

    const rapidjson::Value& obj = d.GetObject();

    if (!Slice::validate_json(HttpMethod::PUT,obj,ae))
	return false;

    if (obj.HasMember("allocation_policy")) {
	if (!obj["allocation_policy"].IsObject()) {
	    if (ae) {
		if (!*ae)
		    *ae = new AppError(400);
		(*ae)->add(std::string("malformed allocation_policy property"));
	    }
	    return NULL;
	}

	if (!allocation_policy->update(obj["allocation_policy"],ae))
	    return false;
    }

    return true;
}

}
