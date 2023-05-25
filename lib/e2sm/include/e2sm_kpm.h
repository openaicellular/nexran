#ifndef _E2SM_KPM_H_
#define _E2SM_KPM_H_

#include <list>
#include <map>
#include <queue>
#include <string>
#include <cstdint>
#include <ctime>

#include "e2ap.h"
#include "e2sm.h"

namespace e2sm
{
namespace kpm
{

typedef struct entity_metrics
{
  time_t time;
  uint64_t dl_bytes;
  uint64_t ul_bytes;
  uint64_t dl_prbs;
  uint64_t ul_prbs;
  int64_t  tx_pkts;
  int64_t  tx_errors;
  int64_t  tx_brate;
  int64_t  rx_pkts;
  int64_t  rx_errors;
  int64_t  rx_brate;
  double   dl_cqi;
  double   dl_ri;
  double   dl_pmi;
  double   ul_phr;
  double   ul_sinr;
  double   ul_mcs;
  int64_t  ul_samples;
  double   dl_mcs;
  int64_t  dl_samples;
} entity_metrics_t;

class MetricsIndex
{
 public:
    MetricsIndex(int period_)
	: period(period_),queue() {};

    void add(entity_metrics_t m);
    entity_metrics_t *current(void);
    entity_metrics_t& get_totals() { return totals; };
    uint64_t get_total_bytes() { return totals.dl_bytes + totals.ul_bytes; };
    int size() { return queue.size(); };
    void flush();
    void reset(int period_) { period = period_; flush(); };

 private:
    int period;
    std::queue<entity_metrics_t> queue;
    entity_metrics_t totals = { };
};

class KpmReport
{
 public:
    KpmReport()
	: period_ms(0),available_dl_prbs(0),available_ul_prbs(0),active_ues(0),
	  ues(),slices() {};
    virtual ~KpmReport() = default;
    std::string to_string(char group_delim = ' ',char item_delim = ',');

    long period_ms;
    int available_dl_prbs;
    int available_ul_prbs;
    long active_ues;
    std::map<long,entity_metrics_t> ues;
    std::map<std::string,entity_metrics_t> slices;
};

class KpmIndication : public e2sm::Indication
{
 public:
    KpmIndication(e2sm::Model *model_)
	: e2sm::Indication(model_) {};
    KpmIndication(e2sm::Model *model_,KpmReport *report_)
	: report(report_),e2sm::Indication(model_) {};
    virtual ~KpmIndication() { delete report; };
    virtual bool encode() { return false; };

    KpmReport *report;
};

typedef enum KpmPeriod {
    MS10	= 0,
    MS20	= 1,
    MS32	= 2,
    MS40	= 3,
    MS60	= 4,
    MS64	= 5,
    MS70	= 6,
    MS80	= 7,
    MS128	= 8,
    MS160	= 9,
    MS256	= 10,
    MS320	= 11,
    MS512	= 12,
    MS640	= 13,
    MS1024	= 14,
    MS1280	= 15,
    MS2048	= 16,
    MS2560	= 17,
    MS5120	= 18,
    MS10240	= 19
} KpmPeriod_t;

long kpm_period_to_ms(KpmPeriod_t period);

class EventTrigger : public e2sm::EventTrigger
{
 public:
    EventTrigger(e2sm::Model *model_,KpmPeriod_t period_)
	: period(period_),e2sm::EventTrigger(model_) {};
    EventTrigger(e2sm::Model *model_)
	: period(KpmPeriod::MS5120),e2sm::EventTrigger(model_) {};
    virtual ~EventTrigger() = default;

    virtual bool encode();

    KpmPeriod_t period;
};

class AgentInterface
{
 public:
    virtual bool handle(e2sm::kpm::KpmIndication *ind) = 0;
};

class KpmModel : public e2sm::Model
{
 public:
    KpmModel(AgentInterface *agent_if_)
	: agent_if(agent_if_),e2sm::Model("ORAN-E2SM-KPM","1.3.6.1.4.1.1.1.2.2") {};
    virtual ~KpmModel() = default;
    virtual int init() { return 0; };
    virtual void stop() {};

    Indication *decode(e2ap::Indication *ind,
		       unsigned char *header,ssize_t header_len,
		       unsigned char *message,ssize_t message_len);
    ControlOutcome *decode(e2ap::ControlAck *ack,
			   unsigned char *outcome,ssize_t outcome_len);
    ControlOutcome *decode(e2ap::ControlFailure *failure,
			   unsigned char *outcome,ssize_t outcome_len);

 protected:
    AgentInterface *agent_if;
};

}
}

#endif /* _E2SM_KPM_H_ */
