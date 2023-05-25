
#include <cstring>
#include <sstream>
#include <ctime>

#include "mdclog/mdclog.h"

#include "e2ap.h"
#include "e2sm.h"
#include "e2sm_internal.h"
#include "e2sm_kpm.h"

#include "E2SM_KPM_RANfunction-Name.h"
#include "E2SM_KPM_E2SM-KPM-EventTriggerDefinition.h"
#include "E2SM_KPM_Trigger-ConditionIE-Item.h"
#include "E2SM_KPM_E2SM-KPM-IndicationHeader.h"
#include "E2SM_KPM_E2SM-KPM-IndicationMessage.h"
#include "E2SM_KPM_PM-Containers-List.h"
#include "E2SM_KPM_PF-Container.h"
#include "E2SM_KPM_PF-ContainerListItem.h"
#include "E2SM_KPM_PlmnID-List.h"
#include "E2SM_KPM_EPC-CUUP-PM-Format.h"
#include "E2SM_KPM_PerUEReportListItemFormat.h"
#include "E2SM_KPM_PerUEReportListItem.h"
#include "E2SM_KPM_PerSliceReportListItemFormat.h"
#include "E2SM_KPM_CellResourceReportListItem.h"
#include "E2SM_KPM_ServedPlmnPerCellListItem.h"
#include "E2SM_KPM_EPC-DU-PM-Container.h"
#include "E2SM_KPM_PerSliceReportListItemFormat.h"
#include "E2SM_KPM_PerSliceReportListItem.h"

namespace e2sm
{
namespace kpm
{

void MetricsIndex::add(entity_metrics_t m)
{
    queue.push(m);
    totals.dl_bytes += m.dl_bytes;
    totals.ul_bytes += m.ul_bytes;
    totals.dl_prbs += m.dl_prbs;
    totals.ul_prbs += m.ul_prbs;
    totals.tx_pkts += m.tx_pkts;
    totals.tx_errors += m.tx_errors;
    totals.rx_pkts += m.rx_pkts;
    totals.rx_errors += m.rx_errors;
    flush();
}

entity_metrics_t *MetricsIndex::current(void)
{
    if (queue.size() > 0)
	return &queue.back();
    return nullptr;
}

void MetricsIndex::flush()
{
    time_t now = std::time(nullptr);

    while (queue.size() > 0) {
	entity_metrics_t& m = queue.front();
	if (m.time < (now - period)) {
	    totals.dl_bytes -= m.dl_bytes;
	    totals.ul_bytes -= m.ul_bytes;
	    totals.dl_prbs -= m.dl_prbs;
	    totals.ul_prbs -= m.ul_prbs;
	    totals.tx_pkts -= m.tx_pkts;
	    totals.tx_errors -= m.tx_errors;
	    totals.rx_pkts -= m.rx_pkts;
	    totals.rx_errors -= m.rx_errors;
	    queue.pop();
	}
	else
	    break;
    }
}

long kpm_period_to_ms(KpmPeriod_t period)
{
    switch(period) {
    case MS10: return 10;
    case MS20: return 20;
    case MS32: return 32;
    case MS40: return 40;
    case MS60: return 60;
    case MS64: return 64;
    case MS70: return 70;
    case MS80: return 80;
    case MS128: return 128;
    case MS160: return 160;
    case MS256: return 256;
    case MS320: return 320;
    case MS512: return 512;
    case MS640: return 640;
    case MS1024: return 1024;
    case MS1280: return 1280;
    case MS2048: return 2048;
    case MS2560: return 2560;
    case MS5120: return 5120;
    case MS10240: return 10240;
    default: return 0;
    }
}

static KpmReport *decode_kpm_indication(
    E2SM_KPM_E2SM_KPM_IndicationHeader_t& h,
    E2SM_KPM_E2SM_KPM_IndicationMessage_t& m)
{
    if (m.indicationMessage.present
	!= E2SM_KPM_E2SM_KPM_IndicationMessage__indicationMessage_PR_indicationMessage_Format1)
	return NULL;

    KpmReport *report = new KpmReport();
    time_t now = std::time(nullptr);

    E2SM_KPM_E2SM_KPM_IndicationMessage_Format1_t *imf = \
	&m.indicationMessage.choice.indicationMessage_Format1;

    for (int i = 0; i < imf->pm_Containers.list.count; ++i) {
	E2SM_KPM_PM_Containers_List_t *item = \
	    (E2SM_KPM_PM_Containers_List_t *)imf->pm_Containers.list.array[i];
	if (!item->performanceContainer)
	    continue;
	E2SM_KPM_PF_Container_t *pfc = item->performanceContainer;
	if (pfc->present == E2SM_KPM_PF_Container_PR_oDU) {
	    E2SM_KPM_ODU_PF_Container_t *du = &pfc->choice.oDU;
	    if (du->cellResourceReportList.list.count == 1) {
		E2SM_KPM_CellResourceReportListItem_t *cell_item = \
		    (E2SM_KPM_CellResourceReportListItem_t *)du->cellResourceReportList.list.array[0];
		if (cell_item->dl_TotalofAvailablePRBs)
		    report->available_dl_prbs = (int)*cell_item->dl_TotalofAvailablePRBs;
		if (cell_item->ul_TotalofAvailablePRBs)
		    report->available_ul_prbs = (int)*cell_item->ul_TotalofAvailablePRBs;
		for (int j = 0; j < cell_item->servedPlmnPerCellList.list.count; ++j) {
		    E2SM_KPM_ServedPlmnPerCellListItem_t *plmn_cell_item = \
			(E2SM_KPM_ServedPlmnPerCellListItem_t *)cell_item->servedPlmnPerCellList.list.array[j];
		    if (!plmn_cell_item->du_PM_EPC)
			continue;
		    
		    if (plmn_cell_item->du_PM_EPC->perUEReportList) {
			for (int x = 0; x < plmn_cell_item->du_PM_EPC->perUEReportList->list.count; ++x) {
			    E2SM_KPM_PerUEReportListItem_t *pui = \
				(E2SM_KPM_PerUEReportListItem_t *)plmn_cell_item->du_PM_EPC->perUEReportList->list.array[x];
			    if (pui->rnti < 1)
				continue;
			    unsigned long dl_prbs = 0,ul_prbs = 0;
			    asn_INTEGER2ulong(&pui->dl_PRBUsage,&dl_prbs);
			    asn_INTEGER2ulong(&pui->ul_PRBUsage,&ul_prbs);
			    if (report->ues.count(pui->rnti) < 1) {
				report->ues[pui->rnti] = {
				    now,0,0,dl_prbs,ul_prbs
				};
			    }
			    else {
				report->ues[pui->rnti].dl_prbs = dl_prbs;
				report->ues[pui->rnti].ul_prbs = ul_prbs;
			    }
			    report->ues[pui->rnti].tx_pkts = pui->tx_pkts;
			    report->ues[pui->rnti].tx_errors = pui->tx_errors;
			    report->ues[pui->rnti].tx_brate = pui->tx_brate;
			    report->ues[pui->rnti].rx_pkts = pui->rx_pkts;
			    report->ues[pui->rnti].rx_errors = pui->rx_errors;
			    report->ues[pui->rnti].rx_brate = pui->rx_brate;
			    report->ues[pui->rnti].dl_cqi = pui->dl_cqi;
			    report->ues[pui->rnti].dl_ri = pui->dl_ri;
			    report->ues[pui->rnti].dl_pmi = pui->dl_pmi;
			    report->ues[pui->rnti].ul_phr = pui->ul_phr;
			    report->ues[pui->rnti].ul_sinr = pui->ul_sinr;
			    report->ues[pui->rnti].ul_mcs = pui->ul_mcs;
			    report->ues[pui->rnti].ul_samples = pui->ul_samples;
			    report->ues[pui->rnti].dl_mcs = pui->dl_mcs;
			    report->ues[pui->rnti].dl_samples = pui->dl_samples;
			}
		    }
		    if (plmn_cell_item->du_PM_EPC->perSliceReportList) {
			for (int x = 0; x < plmn_cell_item->du_PM_EPC->perSliceReportList->list.count; ++x) {
			    E2SM_KPM_PerSliceReportListItem_t *psi = \
				(E2SM_KPM_PerSliceReportListItem_t *)plmn_cell_item->du_PM_EPC->perSliceReportList->list.array[x];
			    if (psi->sliceName.size < 1)
				continue;
			    std::string slice_name = std::string((char *)psi->sliceName.buf,psi->sliceName.size);
			    unsigned long dl_prbs = 0,ul_prbs = 0;
			    asn_INTEGER2ulong(&psi->dl_PRBUsage,&dl_prbs);
			    asn_INTEGER2ulong(&psi->ul_PRBUsage,&ul_prbs);
			    if (report->slices.count(slice_name) < 1) {
				report->slices[slice_name] = {
				    now,0,0,dl_prbs,ul_prbs
				};
			    }
			    else {
				report->slices[slice_name].dl_prbs = dl_prbs;
				report->slices[slice_name].ul_prbs = ul_prbs;
			    }
			    report->slices[slice_name].tx_pkts = psi->tx_pkts;
			    report->slices[slice_name].tx_errors = psi->tx_errors;
			    report->slices[slice_name].tx_brate = psi->tx_brate;
			    report->slices[slice_name].rx_pkts = psi->rx_pkts;
			    report->slices[slice_name].rx_errors = psi->rx_errors;
			    report->slices[slice_name].rx_brate = psi->rx_brate;
			    report->slices[slice_name].dl_cqi = psi->dl_cqi;
			    report->slices[slice_name].dl_ri = psi->dl_ri;
			    report->slices[slice_name].dl_pmi = psi->dl_pmi;
			    report->slices[slice_name].ul_phr = psi->ul_phr;
			    report->slices[slice_name].ul_sinr = psi->ul_sinr;
			    report->slices[slice_name].ul_mcs = psi->ul_mcs;
			    report->slices[slice_name].ul_samples = psi->ul_samples;
			    report->slices[slice_name].dl_mcs = psi->dl_mcs;
			    report->slices[slice_name].dl_samples = psi->dl_samples;
			}
		    }
		}
	    }
	}
	else if (pfc->present == E2SM_KPM_PF_Container_PR_oCU_CP) {
	    E2SM_KPM_OCUCP_PF_Container_t *cucp = &pfc->choice.oCU_CP;
	    if (cucp->cu_CP_Resource_Status.numberOfActive_UEs)
		report->active_ues = *cucp->cu_CP_Resource_Status.numberOfActive_UEs;
	}
	else if (pfc->present == E2SM_KPM_PF_Container_PR_oCU_UP) {
	    E2SM_KPM_OCUUP_PF_Container_t *cuup = &pfc->choice.oCU_UP;
	    for (int j = 0; j < cuup->pf_ContainerList.list.count; ++j) {
		E2SM_KPM_PF_ContainerListItem_t *cuup_item = \
		    (E2SM_KPM_PF_ContainerListItem_t *)cuup->pf_ContainerList.list.array[j];

		for (int k = 0; k < cuup_item->o_CU_UP_PM_Container.plmnList.list.count; ++k) {
		    E2SM_KPM_PlmnID_List_t *cuup_plmn_item = \
			(E2SM_KPM_PlmnID_List_t *)cuup_item->o_CU_UP_PM_Container.plmnList.list.array[k];
		    if (!cuup_plmn_item->cu_UP_PM_EPC)
			continue;

		    if (cuup_plmn_item->cu_UP_PM_EPC->perUEReportList) {
			for (int x = 0; x < cuup_plmn_item->cu_UP_PM_EPC->perUEReportList->list.count; ++x) {
			    E2SM_KPM_PerUEReportListItemFormat_t *pui = \
				(E2SM_KPM_PerUEReportListItemFormat_t *)cuup_plmn_item->cu_UP_PM_EPC->perUEReportList->list.array[x];
			    if (pui->rnti < 1)
				continue;
			    unsigned long dl_bytes = 0,ul_bytes = 0;
			    asn_INTEGER2ulong(&pui->bytesDL,&dl_bytes);
			    asn_INTEGER2ulong(&pui->bytesUL,&ul_bytes);
			    if (report->ues.count(pui->rnti) < 1) {
				report->ues[pui->rnti] = {
				    now,dl_bytes,ul_bytes,0,0
				};
			    }
			    else {
				report->ues[pui->rnti].dl_bytes = dl_bytes;
				report->ues[pui->rnti].ul_bytes = ul_bytes;
			    }
			}
		    }
		    if (cuup_plmn_item->cu_UP_PM_EPC->perSliceReportList) {
			for (int x = 0; x < cuup_plmn_item->cu_UP_PM_EPC->perSliceReportList->list.count; ++x) {
			    E2SM_KPM_PerSliceReportListItemFormat_t *psi = \
				(E2SM_KPM_PerSliceReportListItemFormat_t *)cuup_plmn_item->cu_UP_PM_EPC->perSliceReportList->list.array[x];
			    if (psi->sliceName.size < 1)
				continue;
			    std::string slice_name = std::string((char *)psi->sliceName.buf,psi->sliceName.size);
			    unsigned long dl_bytes = 0,ul_bytes = 0;
			    asn_INTEGER2ulong(&psi->bytesDL,&dl_bytes);
			    asn_INTEGER2ulong(&psi->bytesUL,&ul_bytes);
			    if (report->slices.count(slice_name) < 1) {
				report->slices[slice_name] = {
				    now,dl_bytes,ul_bytes,0,0
				};
			    }
			    else {
				report->slices[slice_name].dl_bytes = dl_bytes;
				report->slices[slice_name].ul_bytes = ul_bytes;
			    }
			}
		    }
		}
	    }
	}
    }

    return report;
}

std::string KpmReport::to_string(char group_delim,char item_delim)
{
    std::stringstream ss;

    ss << "KpmReport(period=" << period_ms << " ms)" << group_delim;
    ss << "available_dl_prbs=" << available_dl_prbs << group_delim;
    ss << "available_ul_prbs=" << available_ul_prbs << group_delim;
    for (auto it = ues.begin(); it != ues.end(); ++it)
	ss << "ue[" << it->first << "]={"
	   << "dl_bytes=" << it->second.dl_bytes << item_delim
	   << "ul_bytes=" << it->second.ul_bytes << item_delim
	   << "dl_prbs=" << it->second.dl_prbs << item_delim
	   << "ul_prbs=" << it->second.ul_prbs << item_delim
	   << "tx_pkts=" << it->second.tx_pkts << item_delim
	   << "tx_errors=" << it->second.tx_errors << item_delim
	   << "tx_brate=" << it->second.tx_brate << item_delim
	   << "rx_pkts=" << it->second.rx_pkts << item_delim
	   << "rx_errors=" << it->second.rx_errors << item_delim
	   << "rx_brate=" << it->second.rx_brate << item_delim
	   << "dl_cqi=" << it->second.dl_cqi << item_delim
	   << "dl_ri=" << it->second.dl_ri << item_delim
	   << "dl_pmi=" << it->second.dl_pmi << item_delim
	   << "ul_phr=" << it->second.ul_phr << item_delim
	   << "ul_sinr=" << it->second.ul_sinr << item_delim
	   << "ul_mcs=" << it->second.ul_mcs << item_delim
	   << "ul_samples=" << it->second.ul_samples << item_delim
	   << "dl_mcs=" << it->second.dl_mcs << item_delim
	   << "dl_samples=" << it->second.dl_samples << item_delim
	   << "}" << group_delim;
    for (auto it = slices.begin(); it != slices.end(); ++it)
	ss << "slice[" << it->first << "]={"
	   << "dl_bytes=" << it->second.dl_bytes << item_delim
	   << "ul_bytes=" << it->second.ul_bytes << item_delim
	   << "dl_prbs=" << it->second.dl_prbs << item_delim
	   << "ul_prbs=" << it->second.ul_prbs << item_delim
	   << "tx_pkts=" << it->second.tx_pkts << item_delim
	   << "tx_errors=" << it->second.tx_errors << item_delim
	   << "tx_brate=" << it->second.tx_brate << item_delim
	   << "rx_pkts=" << it->second.rx_pkts << item_delim
	   << "rx_errors=" << it->second.rx_errors << item_delim
	   << "rx_brate=" << it->second.rx_brate << item_delim
	   << "dl_cqi=" << it->second.dl_cqi << item_delim
	   << "dl_ri=" << it->second.dl_ri << item_delim
	   << "dl_pmi=" << it->second.dl_pmi << item_delim
	   << "ul_phr=" << it->second.ul_phr << item_delim
	   << "ul_sinr=" << it->second.ul_sinr << item_delim
	   << "ul_mcs=" << it->second.ul_mcs << item_delim
	   << "ul_samples=" << it->second.ul_samples << item_delim
	   << "dl_mcs=" << it->second.dl_mcs << item_delim
	   << "dl_samples=" << it->second.dl_samples << item_delim
	   << "}" << group_delim;

    return ss.str();
}

Indication *KpmModel::decode(e2ap::Indication *ind,
			     unsigned char *header,ssize_t header_len,
			     unsigned char *message,ssize_t message_len)
{
    E2SM_KPM_E2SM_KPM_IndicationHeader_t h;
    E2SM_KPM_E2SM_KPM_IndicationMessage_t m;
    void *ptr;

    memset(&h,0,sizeof(h));
    memset(&m,0,sizeof(m));
    
    asn_dec_rval_t dres;
    bool have_header = false,have_message = false;

    if (header && header_len > 0) {
	ptr = &h;
	dres = aper_decode(
	    NULL,&asn_DEF_E2SM_KPM_E2SM_KPM_IndicationHeader,
	    &ptr,header,header_len,0,0);
	if (dres.code != RC_OK) {
	    mdclog_write(MDCLOG_ERR,"failed to decode kpm indication header (len %lu, code %d)\n",
			 header_len,dres.code);
	    return NULL;
	}
	have_header = true;
	E2SM_XER_PRINT(NULL,&asn_DEF_E2SM_KPM_E2SM_KPM_IndicationHeader,&h);
    }
    if (message && message_len > 0) {
	ptr = &m;
	dres = aper_decode(
	    NULL,&asn_DEF_E2SM_KPM_E2SM_KPM_IndicationMessage,
	    &ptr,message,message_len,0,0);
	if (dres.code != RC_OK) {
	    mdclog_write(MDCLOG_ERR,"failed to decode kpm indication message (len %lu, code %d)\n",
			 message_len,dres.code);
	    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_KPM_E2SM_KPM_IndicationHeader,&h);
	    return NULL;
	}
	have_message = true;
	E2SM_XER_PRINT(NULL,&asn_DEF_E2SM_KPM_E2SM_KPM_IndicationMessage,&m);
    }
    if (!have_header) {
	mdclog_write(MDCLOG_ERR,"missing kpm indication header; aborting\n");
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_KPM_E2SM_KPM_IndicationHeader,&h);
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_KPM_E2SM_KPM_IndicationMessage,&m);
	return NULL;
    }
    if (!have_message) {
	mdclog_write(MDCLOG_ERR,"missing kpm indication message; aborting\n");
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_KPM_E2SM_KPM_IndicationHeader,&h);
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_KPM_E2SM_KPM_IndicationMessage,&m);
	return NULL;
    }

    mdclog_write(MDCLOG_DEBUG,"kpm indication report style %ld\n",
		 m.ric_Style_Type);

    KpmReport *report = decode_kpm_indication(h,m);
    if (ind->subscription_request
	&& ind->subscription_request->trigger
	&& dynamic_cast<e2sm::kpm::EventTrigger *>(ind->subscription_request->trigger)) {
	report->period_ms = e2sm::kpm::kpm_period_to_ms(
	    dynamic_cast<e2sm::kpm::EventTrigger *>(ind->subscription_request->trigger)->period);
    }

    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_KPM_E2SM_KPM_IndicationHeader,&h);
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_KPM_E2SM_KPM_IndicationMessage,&m);

    return new KpmIndication(this,report);
}

ControlOutcome *KpmModel::decode(e2ap::ControlAck *ack,
				 unsigned char *outcome,ssize_t outcome_len)
{
    return NULL;
}

ControlOutcome *KpmModel::decode(e2ap::ControlFailure *failure,
				 unsigned char *outcome,ssize_t outcome_len)
{
    return NULL;
}

bool EventTrigger::encode()
{
    if (encoded)
	return true;

    E2SM_KPM_E2SM_KPM_EventTriggerDefinition_t td;

    memset(&td,0,sizeof(td));

    td.present = E2SM_KPM_E2SM_KPM_EventTriggerDefinition_PR_eventDefinition_Format1;
    td.choice.eventDefinition_Format1.policyTest_List = \
	(E2SM_KPM_E2SM_KPM_EventTriggerDefinition_Format1::E2SM_KPM_E2SM_KPM_EventTriggerDefinition_Format1__policyTest_List *) \
	calloc(1,sizeof(*td.choice.eventDefinition_Format1.policyTest_List));

    E2SM_KPM_Trigger_ConditionIE_Item_t *item = \
	(E2SM_KPM_Trigger_ConditionIE_Item *)calloc(1,sizeof(*item));
    item->report_Period_IE = (enum E2SM_KPM_RT_Period_IE)period;
    ASN_SEQUENCE_ADD(&td.choice.eventDefinition_Format1.policyTest_List->list,item);

    E2SM_XER_PRINT(NULL,&asn_DEF_E2SM_KPM_E2SM_KPM_EventTriggerDefinition,&td);

    ssize_t len = e2sm::encode(&asn_DEF_E2SM_KPM_E2SM_KPM_EventTriggerDefinition,NULL,&td,&buf);
    if (len < 0) {
	buf = NULL;
	buf_len = 0;
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_KPM_E2SM_KPM_EventTriggerDefinition,&td);
	return false;
    }
    buf_len = len;
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_KPM_E2SM_KPM_EventTriggerDefinition,&td);

    encoded = true;
    return true;
}

}
}
