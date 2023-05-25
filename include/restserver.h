#ifndef __RESTSERVER__H__
#define __RESTSERVER__H__

#include <string>

#include <pistache/endpoint.h>
#include <pistache/router.h>

#include "rapidjson/prettywriter.h"

namespace nexran {

class App;

class RequestError {
 public:
    RequestError(int http_status_,std::list<std::string> messages_)
	: http_status(http_status_),messages(messages_) {};
    RequestError(int http_status_,std::string message_)
	: http_status(http_status_),
	  messages(std::list<std::string>({message_})) {};
    RequestError(int http_status_)
	: http_status(http_status_),
	  messages(std::list<std::string>({})) {};
    virtual ~RequestError() = default;

    virtual void add(std::string message)
    {
	messages.push_back(message);
    };

    virtual void serialize(rapidjson::Writer<rapidjson::StringBuffer>& writer)
    {
	writer.StartObject();
	writer.String("errors");
	writer.StartArray();
	for (auto it = messages.begin(); it != messages.end(); ++it)
	    writer.String(it->c_str());
	writer.EndArray();
	writer.EndObject();
    };

    int http_status;
    std::list<std::string> messages;
};

class RequestContext {
 public:
    RequestContext(const Pistache::Rest::Request &request_,
		   Pistache::Http::ResponseWriter &response_)
	: request(request_),response(std::move(response_)),sb(),writer(sb),
	  code(Pistache::Http::Code::Ok),async(false),sent(false) {};
    virtual ~RequestContext() = default;

    void make_async() { async = true; };
    bool is_async() { return async; };
    bool is_sent() { return sent; };

    Pistache::Http::ResponseWriter& get_response() { return response; };
    rapidjson::Writer<rapidjson::StringBuffer>& get_writer() { return writer; };
    Pistache::Http::Code get_code() { return code; };
    void set_code(Pistache::Http::Code code_) { code = code_; };

    virtual void send() { if (sent) return; response.send(code,sb.GetString()); sent = true; };

 protected:
    Pistache::Http::Code code;
    bool async;
    bool sent;
    const Pistache::Rest::Request request;
    Pistache::Http::ResponseWriter response;
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer;
};

class RestServer {
 public:

    RestServer()
	: running(false), app(NULL), versionJson(buildVersionJson()) {};
    virtual void init(App *app_);
    virtual void start();
    virtual void stop();
    virtual ~RestServer() { stop(); };
    static std::string buildVersionJson();

 private:
    void setupRoutes();

    void getVersion(const Pistache::Rest::Request &request,
		    Pistache::Http::ResponseWriter response);

    void getAppConfig(const Pistache::Rest::Request &request,
		      Pistache::Http::ResponseWriter response);
    void putAppConfig(const Pistache::Rest::Request &request,
		      Pistache::Http::ResponseWriter response);

    void getNodeBs(const Pistache::Rest::Request &request,
		   Pistache::Http::ResponseWriter response);
    void postNodeB(const Pistache::Rest::Request &request,
		   Pistache::Http::ResponseWriter response);
    void putNodeB(const Pistache::Rest::Request &request,
		  Pistache::Http::ResponseWriter response);
    void getNodeB(const Pistache::Rest::Request &request,
		  Pistache::Http::ResponseWriter response);
    void deleteNodeB(const Pistache::Rest::Request &request,
		     Pistache::Http::ResponseWriter response);

    void getSlices(const Pistache::Rest::Request &request,
		   Pistache::Http::ResponseWriter response);
    void postSlice(const Pistache::Rest::Request &request,
		   Pistache::Http::ResponseWriter response);
    void putSlice(const Pistache::Rest::Request &request,
		  Pistache::Http::ResponseWriter response);
    void getSlice(const Pistache::Rest::Request &request,
		  Pistache::Http::ResponseWriter response);
    void deleteSlice(const Pistache::Rest::Request &request,
		     Pistache::Http::ResponseWriter response);

    void getUes(const Pistache::Rest::Request &request,
		Pistache::Http::ResponseWriter response);
    void postUe(const Pistache::Rest::Request &request,
		Pistache::Http::ResponseWriter response);
    void putUe(const Pistache::Rest::Request &request,
	       Pistache::Http::ResponseWriter response);
    void getUe(const Pistache::Rest::Request &request,
	       Pistache::Http::ResponseWriter response);
    void deleteUe(const Pistache::Rest::Request &request,
		  Pistache::Http::ResponseWriter response);

    void postNodeBSliceBinding(const Pistache::Rest::Request &request,
			       Pistache::Http::ResponseWriter response);
    void deleteNodeBSliceBinding(const Pistache::Rest::Request &request,
				 Pistache::Http::ResponseWriter response);

    void postSliceUeBinding(const Pistache::Rest::Request &request,
			    Pistache::Http::ResponseWriter response);
    void deleteSliceUeBinding(const Pistache::Rest::Request &request,
			      Pistache::Http::ResponseWriter response);

    bool running;
    App *app;
    Pistache::Http::Endpoint endpoint;
    Pistache::Rest::Router router;
    const std::string versionJson;
};

}

#endif /* __RESTSERVER__H__ */
