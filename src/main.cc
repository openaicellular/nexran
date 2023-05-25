
#include <thread>
#include <csignal>
#include <memory>

#include "mdclog/mdclog.h"

#include "nexran.h"
#include "restserver.h"

static int signaled = -1;
static std::unique_ptr<nexran::App> app;

void sigh(int signo) {
    signaled = signo;
    app->stop();
    exit(0);
}

int main(int argc,char **argv) {
    std::thread::id this_id;
    std::unique_ptr<nexran::Config> config;

    this_id = std::this_thread::get_id();

    mdclog_level_set(MDCLOG_DEBUG);

    config = std::make_unique<nexran::Config>();
    if (!config || !config->parseArgv(argc,argv) || !config->parseEnv()
	|| !config->validate()) {
	mdclog_write(MDCLOG_ERR,"Failed to load config");
	config->usage(argv[0]);
	exit(1);
    }

    mdclog_write(MDCLOG_DEBUG,"Creating app and attaching to RMR");
    app = std::make_unique<nexran::App>(*config);

    signal(SIGINT,sigh);
    signal(SIGHUP,sigh);
    signal(SIGTERM,sigh);

    mdclog_write(MDCLOG_DEBUG,"Starting app");
    app->start();

    while (true)
	sleep(1);

    exit(0);
}
