
#include "nexran.h"
#include "e2sm_kpm.h"

namespace nexran {

std::map<std::string,JsonTypeMap> AppConfig::propertyTypes = {
    { "kpm_interval_index",JsonTypeMap::INT },
    { "influxdb_url",JsonTypeMap::STRING }
};
std::map<std::string,std::list<std::string>> AppConfig::propertyEnums;
std::map<HttpMethod,std::list<std::string>> AppConfig::required = {
    { HttpMethod::PUT,{ "kpm_interval_index" } }
};
std::map<HttpMethod,std::list<std::string>> AppConfig::optional = {
    { HttpMethod::PUT,{ "influxdb_url" } }
};
std::map<HttpMethod,std::list<std::string>> AppConfig::disallowed = {
};

void AppConfig::serialize(rapidjson::Writer<rapidjson::StringBuffer>& writer)
{
    writer.StartObject();
    writer.String("kpm_interval_index");
    writer.Int((int)kpm_interval_index);
    writer.String("influxdb_url");
    writer.String(influxdb_url.c_str());
    writer.EndObject();
};

AppConfig *AppConfig::create(rapidjson::Document& d,AppError **ae)
{
    *ae = new AppError(404);
    (*ae)->add(std::string("cannot create multiple xApp Config objects; singleton"));
    return NULL;
}

bool AppConfig::update(rapidjson::Document& d,AppError **ae)
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

    if (!AppConfig::validate_json(HttpMethod::PUT,obj,ae))
	return false;

    if (obj.HasMember("kpm_interval_index")
	&& (obj["kpm_interval_index"].GetInt() < 0 || obj["kpm_interval_index"].GetInt() > 19)) {
	if (ae) {
	    if (!*ae)
		*ae = new AppError(401);
	    (*ae)->add(std::string("kpm_interval_index must be between 0 (10ms) and 19 (10240ms)"));
	    return false;
	}
    }

    if (obj.HasMember("kpm_interval_index"))
	kpm_interval_index = (e2sm::kpm::KpmPeriod_t)obj["kpm_interval_index"].GetInt();

    if (obj.HasMember("influxdb_url")
	&& strcmp(obj["influxdb_url"].GetString(),influxdb_url.c_str()) != 0)
	influxdb_url = std::string(obj["influxdb_url"].GetString());

    return true;
}

}
