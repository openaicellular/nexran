
#include "nexran.h"

namespace nexran {

std::map<AllocationPolicy::Type,const char *> AllocationPolicy::type_to_string = {
    { Proportional, "proportional" },
};

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

    share = obj["share"].GetInt();

    return true;
}

}
