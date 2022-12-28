
#include "nexran.h"

namespace nexran {

std::map<std::string,JsonTypeMap> Ue::propertyTypes = {
    { "imsi",JsonTypeMap::STRING },
    { "tmsi",JsonTypeMap::STRING },
    { "crnti",JsonTypeMap::STRING },
    { "status",JsonTypeMap::OBJECT }
};
std::map<std::string,std::list<std::string>> Ue::propertyEnums;
std::map<HttpMethod,std::list<std::string>> Ue::required = {
    { HttpMethod::POST,{ "imsi" } }
};
std::map<HttpMethod,std::list<std::string>> Ue::optional = {
    { HttpMethod::POST,{ "tmsi" } }
};
std::map<HttpMethod,std::list<std::string>> Ue::disallowed = {
    { HttpMethod::POST,{ "crnti","status" } },
    { HttpMethod::PUT,{ "crnti","status" } }
};

void Ue::serialize(rapidjson::Writer<rapidjson::StringBuffer>& writer)
{
    writer.StartObject();
    writer.String("imsi");
    writer.String(imsi.c_str());
    writer.String("tmsi");
    writer.String(tmsi.c_str());
    writer.String("crnti");
    writer.String(crnti.c_str());
    writer.String("status");
    writer.StartObject();
    writer.String("connected");
    writer.Bool(connected);
    writer.EndObject();
    writer.EndObject();
};

Ue *Ue::create(rapidjson::Document& d,AppError **ae)
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

    if (!Ue::validate_json(HttpMethod::POST,obj,ae))
	return NULL;

    Ue *ue;
    if (obj.HasMember("tmsi"))
	ue = new Ue(std::string(obj["imsi"].GetString()),
		    std::string(obj["tmsi"].GetString()),
		    std::string());
    else
	ue = new Ue(std::string(obj["imsi"].GetString()));

    return ue;
}

bool Ue::update(rapidjson::Document& d,AppError **ae)
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

    if (!Ue::validate_json(HttpMethod::PUT,obj,ae))
	return false;

    if (obj.HasMember("imsi")
	&& strcmp(imsi.c_str(),obj["imsi"].GetString()) != 0) {
	if (ae) {
	    if (!*ae)
		*ae = new AppError(403);
	    (*ae)->add(std::string("cannot modify immutable property: imsi"));
	    return false;
	}
    }

    if (obj.HasMember("tmsi"))
	tmsi = std::string(obj["tmsi"].GetString());

    return true;
}

}
