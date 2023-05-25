
#include "nexran.h"

namespace nexran {

std::map<std::string,JsonTypeMap> NodeB::propertyTypes = {
    { "name",JsonTypeMap::STRING },
    { "type",JsonTypeMap::STRING },
    { "mcc",JsonTypeMap::STRING },
    { "mnc",JsonTypeMap::STRING },
    { "id",JsonTypeMap::INT },
    { "id_len",JsonTypeMap::UINT },
    { "status",JsonTypeMap::OBJECT },
    { "config",JsonTypeMap::OBJECT },
    { "dl_mask",JsonTypeMap::OBJECT },
    { "ul_mask",JsonTypeMap::OBJECT },
    { "dl_mask_def",JsonTypeMap::STRING },
    { "ul_mask_def",JsonTypeMap::STRING },
    { "dl_mask_sched",JsonTypeMap::ARRAY },
    { "ul_mask_sched",JsonTypeMap::ARRAY }
};
std::map<std::string,std::list<std::string>> NodeB::propertyEnums = {
    { "type",{ "gNB","gNB-CU-UP","gNB-DU","en-gNB","eNB","ng-eNB" } },
};
std::map<HttpMethod,std::list<std::string>> NodeB::required = {
    { HttpMethod::POST,{ "type","id","mcc","mnc" } }
};
std::map<HttpMethod,std::list<std::string>> NodeB::optional = {
    { HttpMethod::POST,{ "id_len","dl_mask_def","ul_mask_def",
			 "dl_mask_sched","ul_mask_sched" } },
    { HttpMethod::PUT,{ "dl_mask_def","ul_mask_def",
			"dl_mask_sched","ul_mask_sched" } }
};
std::map<HttpMethod,std::list<std::string>> NodeB::disallowed = {
    { HttpMethod::POST,{ "name","status","config","dl_mask","ul_mask" } },
    { HttpMethod::PUT,{ "type","id","id_len","mcc","mnc","name","status","config","dl_mask","ul_mask" } }
};

const char *NodeB::type_string_map[NodeB::Type::__END__] = {
    "unknown","gNB","gNB-CU-UP","gNB-DU","en-gNB","eNB","ng-eNB"
};

const char *NodeB::type_to_type_string(Type t)
{
    if (t >= Type::UNKNOWN && t < Type::__END__)
	return type_string_map[(int)t];
    return "invalid";
}

NodeB::Type NodeB::type_string_to_type(const char *ts)
{
    int i;
    for (i = 0; i < (int)Type::__END__; ++i)
	if (strcmp(ts,type_string_map[i]) == 0)
	    break;
    return (Type)i;
}

const char *NodeB::get_name_prefix(Type type,uint8_t id_len)
{
    if (type == Type::UNKNOWN)
	return "unknown";
    else if (type >= Type::GNB && type <= GNB_DU)
	return "gnB";
    else if (type == Type::EN_GNB)
	return "en_gnB";
    else if (type == Type::ENB)
	if (id_len == 0 || id_len == 20)
	    return "enB_macro";
	else if (id_len == 18)
	    return "enB_shortmacro";
	else if (id_len == 21)
	    return "enB_longmacro";
	else
	    return "invalid";
    else if (type == Type::NG_ENB)
	if (id_len == 0 || id_len == 20)
	    return "ng_enB_macro";
	else if (id_len == 18)
	    return "ng_enB_shortmacro";
	else if (id_len == 21)
	    return "ng_enB_longmacro";
	else
	    return "invalid";
    else
	return "invalid";
}

std::unique_ptr<std::string> NodeB::build_name(
    Type type,const char *mcc,const char *mnc,
    int32_t id,uint8_t id_len)
{
    char buf[128];
    int rc = 0;
    const char *name_prefix =
	get_name_prefix(type,id_len);
    const char *mnc_prefix = "";
    if (strlen(mnc) == 1)
	mnc_prefix = "00";
    else if (strlen(mnc) == 2)
	mnc_prefix = "0";

    rc = std::snprintf(buf,sizeof(buf),"%s_%s_%s%s_%05x0",
		       name_prefix,mcc,mnc_prefix,mnc,id);
    buf[rc] = '\0';
    return std::make_unique<std::string>(buf);
}

void NodeB::serialize(rapidjson::Writer<rapidjson::StringBuffer>& writer)
{
    writer.StartObject();
    writer.String("name");
    writer.String(name->c_str());
    writer.String("type");
    writer.String(type_to_type_string(type));
    writer.String("id");
    writer.Int(id);
    writer.String("id_len");
    writer.Uint(id_len);
    writer.String("status");
    writer.StartObject();
    writer.String("connected");
    writer.Bool(connected);
    writer.String("last_indication");
    writer.Double(last_indication);
    writer.EndObject();
    writer.String("config");
    writer.StartObject();
    writer.String("total_prbs");
    writer.Int(total_prbs);
    writer.EndObject();
    writer.String("slices");
    writer.StartArray();
    for (auto it = slices.begin(); it != slices.end(); ++it) {
	writer.String(it->first.c_str());
    }
    writer.EndArray();

    writer.String("dl_mask");
    writer.StartObject();
    writer.String("mask");
    writer.String(dl_mask.mask.c_str());
    writer.String("start");
    writer.Double(dl_mask.start);
    writer.String("end");
    writer.Double(dl_mask.end);
    writer.String("id");
    writer.Int(dl_mask.id);
    writer.EndObject();
    writer.String("ul_mask");
    writer.StartObject();
    writer.String("mask");
    writer.String(ul_mask.mask.c_str());
    writer.String("start");
    writer.Double(ul_mask.start);
    writer.String("end");
    writer.Double(ul_mask.end);
    writer.String("id");
    writer.Int(ul_mask.id);
    writer.EndObject();

    writer.String("dl_mask_def");
    writer.String(dl_mask_def.c_str());
    writer.String("ul_mask_def");
    writer.String(ul_mask_def.c_str());

    writer.String("dl_mask_sched");
    writer.StartArray();
    for (auto it = dl_mask_sched.begin(); it != dl_mask_sched.end(); ++it) {
	writer.StartObject();
	writer.String("mask");
	writer.String(it->mask.c_str());
	writer.String("start");
	writer.Double(it->start);
	writer.String("end");
	writer.Double(it->end);
	writer.String("id");
	writer.Double(it->id);
	writer.EndObject();
    }
    writer.EndArray();
    writer.String("ul_mask_sched");
    writer.StartArray();
    for (auto it = ul_mask_sched.begin(); it != ul_mask_sched.end(); ++it) {
	writer.StartObject();
	writer.String("mask");
	writer.String(it->mask.c_str());
	writer.String("start");
	writer.Double(it->start);
	writer.String("end");
	writer.Double(it->end);
	writer.String("id");
	writer.Double(it->id);
	writer.EndObject();
    }
    writer.EndArray();

    writer.EndObject();
};

static bool parse_mask_sched(
    const rapidjson::Value& v,AppError **ae,
    std::list<e2sm::zylinium::BlockedMask>& nl)
{
    for (auto& vi : v.GetArray()) {
	if (!vi.IsObject()) {
	    if (!*ae)
		*ae = new AppError(400);
	    (*ae)->add(std::string("invalid mask sched item; not object"));
	    return false;
	}
	e2sm::zylinium::BlockedMask m;
	if (vi.HasMember("mask")) {
	    if (!vi["mask"].IsString()) {
		if (!*ae)
		    *ae = new AppError(400);
		(*ae)->add(std::string("invalid mask sched item: mask must be a string"));
		return false;
	    }
	    m.mask = std::string(vi["mask"].GetString());
	}
	else {
	    if (!*ae)
		*ae = new AppError(400);
	    (*ae)->add(std::string("invalid mask sched item: mask must be present"));
	    return false;
	}
	if (vi.HasMember("start")) {
	    if (!vi["start"].IsDouble()) {
		if (!*ae)
		    *ae = new AppError(400);
		(*ae)->add(std::string("invalid mask sched item: start must be a float"));
		return false;
	    }
	    m.start = vi["start"].GetDouble();
	}
	if (vi.HasMember("end")) {
	    if (!vi["end"].IsDouble()) {
		if (!*ae)
		    *ae = new AppError(400);
		(*ae)->add(std::string("invalid mask sched item: end must be a float"));
		return false;
	    }
	    m.end = vi["end"].GetDouble();
	}
	if (vi.HasMember("id")) {
	    if (!vi["id"].IsInt()) {
		if (!*ae)
		    *ae = new AppError(400);
		(*ae)->add(std::string("invalid mask sched item: id must be an integer"));
		return false;
	    }
	    m.id = vi["id"].GetInt();
	}

	nl.push_back(m);
    }

    return true;
}

NodeB *NodeB::create(rapidjson::Document& d,AppError **ae)
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
    if (!NodeB::validate_json(HttpMethod::POST,obj,ae))
	return NULL;

    uint8_t id_len = 20;
    if (obj.HasMember("id_len"))
	id_len = (uint8_t)obj["id_len"].GetUint();
    NodeB *nb = new NodeB(
	type_string_to_type(obj["type"].GetString()),
	obj["mcc"].GetString(),obj["mnc"].GetString(),
	obj["id"].GetInt(),id_len);

    if (obj.HasMember("dl_mask_def"))
	nb->dl_mask_def = obj["dl_mask_def"].GetString();
    if (obj.HasMember("ul_mask_def"))
	nb->ul_mask_def = obj["ul_mask_def"].GetString();
    if (obj.HasMember("dl_mask_sched")) {
	std::list<e2sm::zylinium::BlockedMask> nl;
	if (!parse_mask_sched(obj["dl_mask_sched"],ae,nl))
	    goto errout;
	nb->dl_mask_sched = nl;
    }
    if (obj.HasMember("ul_mask_sched")) {
	std::list<e2sm::zylinium::BlockedMask> nl;
	if (!parse_mask_sched(obj["ul_mask_sched"],ae,nl))
	    goto errout;
	nb->ul_mask_sched = nl;
    }

    return nb;

 errout:
    delete nb;
    return NULL;
}

bool NodeB::update(rapidjson::Document& d,AppError **ae)
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
    if (!NodeB::validate_json(HttpMethod::PUT,obj,ae))
	return false;

    std::list<e2sm::zylinium::BlockedMask> new_dl_mask_sched;
    std::list<e2sm::zylinium::BlockedMask> new_ul_mask_sched;
    if (obj.HasMember("dl_mask_sched")) {
	if (!parse_mask_sched(obj["dl_mask_sched"],ae,new_dl_mask_sched))
	    return false;
    }
    if (obj.HasMember("ul_mask_sched")) {
	std::list<e2sm::zylinium::BlockedMask> nl;
	if (!parse_mask_sched(obj["ul_mask_sched"],ae,new_ul_mask_sched))
	    return false;
    }
    // Only update once we parse successfully; needs a refactor.
    if (obj.HasMember("dl_mask_sched"))
	dl_mask_sched = new_dl_mask_sched;
    if (obj.HasMember("ul_mask_sched"))
	ul_mask_sched = new_ul_mask_sched;
    if (obj.HasMember("dl_mask_def"))
	dl_mask_def = obj["dl_mask_def"].GetString();
    if (obj.HasMember("ul_mask_def"))
	ul_mask_def = obj["ul_mask_def"].GetString();

    return true;
}

}
