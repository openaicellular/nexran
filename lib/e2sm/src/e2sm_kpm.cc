
#include <cstring>

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

namespace e2sm
{
namespace kpm
{

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

    // XXX: meid

    //std::list<SliceStatus *> status_list = decode_slice_status_report(&m.choice.sliceStatusReport);

    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_KPM_E2SM_KPM_IndicationHeader,&h);
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_KPM_E2SM_KPM_IndicationMessage,&m);

    //return new SliceStatusReport(this,status_list);
    return NULL;
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
	calloc(1,sizeof(E2SM_KPM_E2SM_KPM_EventTriggerDefinition_Format1::E2SM_KPM_E2SM_KPM_EventTriggerDefinition_Format1__policyTest_List));
    E2SM_KPM_Trigger_ConditionIE_Item *item = \
	(E2SM_KPM_Trigger_ConditionIE_Item *)calloc(1,sizeof(E2SM_KPM_Trigger_ConditionIE_Item));
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
