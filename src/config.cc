
#include <cstdlib>
#include <cstdio>
#include <iostream>

#include "config.h"

nexran::Config::Config()
{
    config[XAPP_NAME] = new Item(
	STRING,'n',"xapp-name","XAPP_NAME",true,(ItemValue*)NULL,
	"The name of the xApp.");
    config[XAPP_ID] = new Item(
	STRING,'x',"xapp-id","XAPP_ID",true,(ItemValue*)NULL,
	"The ID of the xApp.");
    config[HOST] = new Item(
	STRING,'H',"host","HOST",false,new ItemValue("0.0.0.0"),
	"The local IP on which the RMR service listens.");
    config[PORT] = new Item(
	INTEGER,'p',"port","PORT",false,new ItemValue(4560),
	"The RMR listen port (default 4560)");
    config[LOG_LEVEL] = new Item(
	STRING,'l',"log-level","LOG_LEVEL",false,new ItemValue("warn"),
	"The name of the xApp.");
    config[ADMIN_HOST] = new Item(
	STRING,'A',"admin-host","ADMIN_HOST",false,new ItemValue("0.0.0.0"),
	"The local IP on which the northbound interface listens.");
    config[ADMIN_PORT] = new Item(
	INTEGER,'a',"admin-port","ADMIN_PORT",false,new ItemValue(8000),
	"The northbound interface listen port.");
    config[RMR_NOWAIT] = new Item(
	BOOL,'R',"rmr-nowait","RMR_NOWAIT",false,new ItemValue(false),
	"Do not wait for RMR route established (waits by default).");
    config[KPM_INTERVAL_INDEX] = new Item(
	INTEGER,'K',"kpm-interval-index","KPM_INTERVAL_INDEX",false,new ItemValue(18),
	"Set the default KPM subscription interval; defaults to 18 (5120ms) (0-19 -> 10-10240ms).");
    config[INFLUXDB_URL] = new Item(
	STRING,'I',"influxdb-url","INFLUXDB_URL",false,new ItemValue(""),
	"Set the InfluxDB URL to this value.");

    optstr = (char *)calloc(config.size() + 2 + 1,2);
    long_options = (struct option *)calloc(config.size() + 2,
					   sizeof(struct option));
    int i = 0;
    char *optstr_ptr = optstr;
    for (auto it = config.begin(); it != config.end(); ++it) {
	auto name = it->first;
	auto item = it->second;

	sprintf(optstr_ptr,"%c",item->short_option_name);
	++optstr_ptr;
	if (item->type != BOOL) {
	    sprintf(optstr_ptr,":");
	    ++optstr_ptr;
	}

	if (item->long_option_name == NULL)
	    continue;
	long_options[i].name = item->long_option_name;
	if (item->type == BOOL)
	    long_options[i].has_arg = 0;
	else
	    long_options[i].has_arg = 2;
	long_options[i].val = item->short_option_name;
	++i;
    }
    long_options[i].name = "help";
    long_options[i].has_arg = 0;
    long_options[i].val = 'h';
}

nexran::Config::~Config()
{
    free(optstr);
    free(long_options);
}

bool nexran::Config::parseEnv()
{
    for (auto it = config.begin(); it != config.end(); ++it) {
	auto item = it->second;

	if (!item->env_name)
	    continue;

	char *val = getenv(item->env_name);
	if (!val)
	    continue;

	if (item->type == BOOL)
	    item->value.b = not item->value.b;
	else if (item->type == STRING)
	    item->value.s = val;
	else if (item->type == INTEGER)
	    item->value.i = std::atoi(val);
	else
	    return false;

	item->value_set = true;
    }

    return true;
}

bool nexran::Config::parseArgv(int argc,char **argv)
{
    while (true) {
	int optindex = 0;
	char c = getopt_long(argc,argv,optstr,long_options,&optindex);
	if (c == -1)
	    return true;
	if (c == 'h')
	    return false;
	bool found = false;
	for (auto it = config.begin(); it != config.end(); ++it) {
	    auto name = it->first;
	    auto item = it->second;

	    if (c != item->short_option_name)
		continue;

	    if (item->type == BOOL)
		item->value.b = true;
	    else if (item->type == STRING)
		item->value.s = optarg;
	    else if (item->type == INTEGER)
		item->value.i = std::atoi(optarg);
	    else
		return false;

	    item->value_set = true;
	    found = true;
	    break;
	}
	if (!found)
	    return false;
    }

    return true;
}

bool nexran::Config::validate()
{
    for (auto it = config.begin(); it != config.end(); ++it) {
	auto item = it->second;

	if (item->required && !(item->value_set || item->default_value))
	    return false;
    }

    return true;
}

void nexran::Config::usage(const char *progname)
{
    std::printf("Usage: %s [OPTION]\n",progname);
    std::printf("\n");
    std::printf("Mandatory arguments:\n");
    for (auto it = config.begin(); it != config.end(); ++it) {
	auto item = it->second;

	if (!item->required)
	    continue;

	std::printf("  -%c",item->short_option_name);
	if (item->long_option_name)
	    std::printf(", --%s",item->long_option_name);
	std::printf("\t\t%s\n",item->help);
    }
    std::printf("\n");
    std::printf("Optional arguments:\n");
    for (auto it = config.begin(); it != config.end(); ++it) {
	auto item = it->second;

	if (item->required)
	    continue;

	std::printf("  -%c",item->short_option_name);
	if (item->long_option_name)
	    std::printf(", --%s",item->long_option_name);
	std::printf("\t\t%s\n",item->help);
    }
}
