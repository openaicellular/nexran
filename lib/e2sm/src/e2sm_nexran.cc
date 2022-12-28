
#include <cstring>

#include "mdclog/mdclog.h"

#include "e2ap.h"
#include "e2sm.h"
#include "e2sm_internal.h"
#include "e2sm_nexran.h"

#include "E2SM_NEXRAN_RANfunction-Description.h"
#include "E2SM_NEXRAN_E2SM-NexRAN-ControlHeader.h"
#include "E2SM_NEXRAN_E2SM-NexRAN-ControlMessage.h"
#include "E2SM_NEXRAN_SliceConfigRequest.h"
#include "E2SM_NEXRAN_SliceConfig.h"
#include "E2SM_NEXRAN_SchedPolicy.h"
#include "E2SM_NEXRAN_SliceDeleteRequest.h"
#include "E2SM_NEXRAN_E2SM-NexRAN-EventTriggerDefinition.h"
#include "E2SM_NEXRAN_E2SM-NexRAN-ControlOutcome.h"
#include "E2SM_NEXRAN_E2SM-NexRAN-IndicationHeader.h"
#include "E2SM_NEXRAN_E2SM-NexRAN-IndicationMessage.h"
#include "E2SM_NEXRAN_SliceStatus.h"
#include "E2SM_NEXRAN_UeStatus.h"

namespace e2sm
{
namespace nexran
{

static std::list<SliceStatus *> decode_slice_status_report(E2SM_NEXRAN_SliceStatusReport *status)
{
    std::list<SliceStatus *> status_list;

    for (auto ptr = status->sliceStatusList.list.array;
	 ptr < &status->sliceStatusList.list.array[status->sliceStatusList.list.count];
	 ptr++) {
	E2SM_NEXRAN_SliceStatus *sstatus = (E2SM_NEXRAN_SliceStatus *)*ptr;
	std::list<UeStatus *> ue_list;
	for (auto ptr2 = sstatus->ueList.list.array;
	     ptr2 < &sstatus->ueList.list.array[sstatus->ueList.list.count];
	     ptr2++) {
	    E2SM_NEXRAN_UeStatus *ustatus = (E2SM_NEXRAN_UeStatus *)*ptr2;
	    std::string imsi,crnti;
	    if (ustatus->imsi.buf && ustatus->imsi.size > 0)
		imsi = std::string((char *)ustatus->imsi.buf,0,ustatus->imsi.size);
	    if (ustatus->crnti && ustatus->crnti->buf && ustatus->crnti->size > 0)
		crnti = std::string((char *)ustatus->crnti->buf,0,ustatus->crnti->size);
	    ue_list.push_back(new UeStatus(imsi,ustatus->connected,crnti));
	}
	std::string slice_name;
	if (sstatus->sliceName.buf && sstatus->sliceName.size > 0)
	    slice_name = std::string((char *)sstatus->sliceName.buf,0,sstatus->sliceName.size);
	auto policy = new ProportionalAllocationPolicy(sstatus->schedPolicy.choice.proportionalAllocationPolicy.share);
	status_list.push_back(new SliceStatus(slice_name,policy,ue_list));
    }

    return status_list;
}

Indication *NexRANModel::decode(e2ap::Indication *ind,
				unsigned char *header,ssize_t header_len,
				unsigned char *message,ssize_t message_len)
{
    E2SM_NEXRAN_E2SM_NexRAN_IndicationHeader_t h;
    E2SM_NEXRAN_E2SM_NexRAN_IndicationMessage_t m;
    void *ptr;

    memset(&h,0,sizeof(h));
    memset(&m,0,sizeof(m));
    
    asn_dec_rval_t dres;
    bool have_header = false,have_message = false;

    if (header && header_len > 0) {
	ptr = &h;
	dres = aper_decode(
	    NULL,&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_IndicationHeader,
	    &ptr,header,header_len,0,0);
	if (dres.code != RC_OK) {
	    mdclog_write(MDCLOG_ERR,"failed to decode nexran indication header (len %lu, code %d)\n",
			 header_len,dres.code);
	    return NULL;
	}
	have_header = true;
	E2SM_XER_PRINT(NULL,&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_IndicationHeader,&h);
    }
    if (message && message_len > 0) {
	ptr = &m;
	dres = aper_decode(
	    NULL,&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_IndicationMessage,
	    &ptr,message,message_len,0,0);
	if (dres.code != RC_OK) {
	    mdclog_write(MDCLOG_ERR,"failed to decode nexran indication message (len %lu, code %d)\n",
			 message_len,dres.code);
	    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_IndicationHeader,&h);
	    return NULL;
	}
	have_message = true;
	E2SM_XER_PRINT(NULL,&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_IndicationMessage,&m);
    }
    if (!have_header) {
	mdclog_write(MDCLOG_ERR,"missing nexran indication header; aborting\n");
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_IndicationHeader,&h);
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_IndicationMessage,&m);
	return NULL;
    }
    if (!have_message) {
	mdclog_write(MDCLOG_ERR,"missing nexran indication message; aborting\n");
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_IndicationHeader,&h);
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_IndicationMessage,&m);
	return NULL;
    }

    if (m.present != E2SM_NEXRAN_E2SM_NexRAN_IndicationMessage_PR_sliceStatusReport) {
	mdclog_write(MDCLOG_ERR,"unsupported nexran indication message (%d); aborting\n",
		     m.present);
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_IndicationHeader,&h);
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_IndicationMessage,&m);
	return NULL;
    }

    // XXX: meid

    std::list<SliceStatus *> status_list = decode_slice_status_report(&m.choice.sliceStatusReport);

    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_IndicationHeader,&h);
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_IndicationMessage,&m);

    return new SliceStatusReport(this,status_list);
}

ControlOutcome *NexRANModel::decode(e2ap::ControlAck *ack,
				    unsigned char *outcome,ssize_t outcome_len)
{
    if (!outcome || outcome_len <= 0)
	return NULL;

    E2SM_NEXRAN_E2SM_NexRAN_ControlOutcome_t o;
    memset(&o,0,sizeof(o));

    void *ptr = &o;
    asn_dec_rval_t dres = aper_decode(
        NULL,&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlOutcome,
	&ptr,outcome,outcome_len,0,0);
    if (dres.code != RC_OK) {
	mdclog_write(MDCLOG_ERR,"failed to decode nexran control outcome (len %lu, code %d)\n",
		     outcome_len,dres.code);
	return NULL;
    }
    E2SM_XER_PRINT(NULL,&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlOutcome,&o);
    if (o.present != E2SM_NEXRAN_E2SM_NexRAN_ControlOutcome_PR_controlOutcomeFormat1) {
	mdclog_write(MDCLOG_ERR,"unsupported nexran control outcome (%d); aborting\n",
		     o.present);
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlOutcome,&o);
	return NULL;
    }
    if (o.choice.controlOutcomeFormat1.present != E2SM_NEXRAN_E2SM_NexRAN_ControlOutcome_Format1_PR_sliceStatusReport) {
	mdclog_write(MDCLOG_ERR,"unsupported nexran control outcome format1 (%d); aborting\n",
		     o.choice.controlOutcomeFormat1.present);
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlOutcome,&o);
	return NULL;
    }

    std::list<SliceStatus *> status_list = decode_slice_status_report(&o.choice.controlOutcomeFormat1.choice.sliceStatusReport);

    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlOutcome,&o);

    return new SliceStatusControlOutcome(this,status_list);
}

ControlOutcome *NexRANModel::decode(e2ap::ControlFailure *failure,
				    unsigned char *outcome,ssize_t outcome_len)
{
    if (!outcome || outcome_len <= 0)
	return NULL;

    E2SM_NEXRAN_E2SM_NexRAN_ControlOutcome_t o;
    memset(&o,0,sizeof(o));

    void *ptr = &o;
    asn_dec_rval_t dres = aper_decode(
        NULL,&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlOutcome,
	&ptr,outcome,outcome_len,0,0);
    if (dres.code != RC_OK) {
	mdclog_write(MDCLOG_ERR,"failed to decode nexran control outcome (len %lu, code %d)\n",
		     outcome_len,dres.code);
	return NULL;
    }
    E2SM_XER_PRINT(NULL,&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlOutcome,&o);
    if (o.present != E2SM_NEXRAN_E2SM_NexRAN_ControlOutcome_PR_controlOutcomeFormat1) {
	mdclog_write(MDCLOG_ERR,"unsupported nexran control outcome (%d); aborting\n",
		     o.present);
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlOutcome,&o);
	return NULL;
    }
    if (o.choice.controlOutcomeFormat1.present != E2SM_NEXRAN_E2SM_NexRAN_ControlOutcome_Format1_PR_sliceStatusReport) {
	mdclog_write(MDCLOG_ERR,"unsupported nexran control outcome format1 (%d); aborting\n",
		     o.choice.controlOutcomeFormat1.present);
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlOutcome,&o);
	return NULL;
    }

    std::list<SliceStatus *> status_list = decode_slice_status_report(
        &o.choice.controlOutcomeFormat1.choice.sliceStatusReport);

    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlOutcome,&o);

    return new SliceStatusControlOutcome(this,status_list);
}

bool SliceConfigRequest::encode()
{
    if (encoded)
	return true;

    E2SM_NEXRAN_E2SM_NexRAN_ControlHeader_t h;
    E2SM_NEXRAN_E2SM_NexRAN_ControlMessage_t m;

    memset(&h,0,sizeof(h));
    memset(&m,0,sizeof(m));

    h.present = E2SM_NEXRAN_E2SM_NexRAN_ControlHeader_PR_controlHeaderFormat1;
    h.choice.controlHeaderFormat1.controlMessageId = \
	E2SM_NEXRAN_E2SM_NexRAN_ControlMessage_Id_sliceConfigRequest;

    m.present = E2SM_NEXRAN_E2SM_NexRAN_ControlMessage_PR_controlMessageFormat1;
    m.choice.controlMessageFormat1.present = \
	E2SM_NEXRAN_E2SM_NexRAN_ControlMessage_Format1_PR_sliceConfigRequest;
    for (auto it = configs.begin(); it != configs.end(); ++it) {
	E2SM_NEXRAN_SliceConfig_t *ie = (E2SM_NEXRAN_SliceConfig_t *)calloc(1,sizeof(*ie));
	ie->sliceName.size = strlen((*it)->name.c_str());
	ie->sliceName.buf = (uint8_t *)malloc(ie->sliceName.size + 1);
	strcpy((char *)ie->sliceName.buf,(*it)->name.c_str());

	ie->schedPolicy.present = E2SM_NEXRAN_SchedPolicy_PR_proportionalAllocationPolicy;
	ie->schedPolicy.choice.proportionalAllocationPolicy.share = (*it)->policy->share;
	ASN_SEQUENCE_ADD(&m.choice.controlMessageFormat1.choice.sliceConfigRequest.sliceConfigList.list,ie);
    }

    E2SM_XER_PRINT(NULL,&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlHeader,&h);

    ssize_t len = e2sm::encode(&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlHeader,NULL,&h,&header);
    if (len < 0) {
	header = NULL;
	header_len = 0;
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlHeader,&h);
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlMessage,&m);
	return 1;
    }
    header_len = len;
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlHeader,&h);

    E2SM_XER_PRINT(NULL,&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlMessage,&m);

    len = e2sm::encode(&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlMessage,NULL,&m,&message);
    if (len < 0) {
	free(header);
	header = NULL;
	header_len = 0;
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlMessage,&m);
	return 1;
    }
    message_len = len;
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlMessage,&m);

    encoded = true;
    return true;
}

bool SliceDeleteRequest::encode()
{
    if (encoded)
	return true;

    E2SM_NEXRAN_E2SM_NexRAN_ControlHeader_t h;
    E2SM_NEXRAN_E2SM_NexRAN_ControlMessage_t m;

    memset(&h,0,sizeof(h));
    memset(&m,0,sizeof(m));

    h.present = E2SM_NEXRAN_E2SM_NexRAN_ControlHeader_PR_controlHeaderFormat1;
    h.choice.controlHeaderFormat1.controlMessageId = \
	E2SM_NEXRAN_E2SM_NexRAN_ControlMessage_Id_sliceDeleteRequest;

    m.present = E2SM_NEXRAN_E2SM_NexRAN_ControlMessage_PR_controlMessageFormat1;
    m.choice.controlMessageFormat1.present = \
	E2SM_NEXRAN_E2SM_NexRAN_ControlMessage_Format1_PR_sliceDeleteRequest;
    for (auto it = names.begin(); it != names.end(); ++it) {
	E2SM_NEXRAN_SliceName_t *ie = (E2SM_NEXRAN_SliceName_t *)calloc(1,sizeof(*ie));
	ie->size = strlen((*it).c_str());
	ie->buf = (uint8_t *)malloc(ie->size + 1);
	strcpy((char *)ie->buf,(*it).c_str());
	ASN_SEQUENCE_ADD(&m.choice.controlMessageFormat1.choice.sliceDeleteRequest.sliceNameList.list,ie);
    }

    E2SM_XER_PRINT(NULL,&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlHeader,&h);

    ssize_t len = e2sm::encode(&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlHeader,NULL,&h,&header);
    if (len < 0) {
	header = NULL;
	header_len = 0;
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlHeader,&h);
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlMessage,&m);
	return 1;
    }
    header_len = len;
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlHeader,&h);

    E2SM_XER_PRINT(NULL,&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlMessage,&m);

    len = e2sm::encode(&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlMessage,NULL,&m,&message);
    if (len < 0) {
	free(header);
	header = NULL;
	header_len = 0;
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlMessage,&m);
	return 1;
    }
    message_len = len;
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlMessage,&m);

    encoded = true;
    return true;
}

bool SliceStatusRequest::encode()
{
    if (encoded)
	return true;

    E2SM_NEXRAN_E2SM_NexRAN_ControlHeader_t h;
    E2SM_NEXRAN_E2SM_NexRAN_ControlMessage_t m;

    memset(&h,0,sizeof(h));
    memset(&m,0,sizeof(m));

    h.present = E2SM_NEXRAN_E2SM_NexRAN_ControlHeader_PR_controlHeaderFormat1;
    h.choice.controlHeaderFormat1.controlMessageId = \
	E2SM_NEXRAN_E2SM_NexRAN_ControlMessage_Id_sliceStatusRequest;

    m.present = E2SM_NEXRAN_E2SM_NexRAN_ControlMessage_PR_controlMessageFormat1;
    m.choice.controlMessageFormat1.present = \
	E2SM_NEXRAN_E2SM_NexRAN_ControlMessage_Format1_PR_sliceStatusRequest;
    for (auto it = names.begin(); it != names.end(); ++it) {
	E2SM_NEXRAN_SliceName_t *ie = (E2SM_NEXRAN_SliceName_t *)calloc(1,sizeof(*ie));
	ie->size = strlen(it->c_str());
	ie->buf = (uint8_t *)malloc(ie->size + 1);
	strcpy((char *)ie->buf,it->c_str());
	ASN_SEQUENCE_ADD(&m.choice.controlMessageFormat1.choice.sliceStatusRequest.sliceNameList.list,ie);
    }

    E2SM_XER_PRINT(NULL,&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlHeader,&h);

    ssize_t len = e2sm::encode(&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlHeader,NULL,&h,&header);
    if (len < 0) {
	header = NULL;
	header_len = 0;
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlHeader,&h);
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlMessage,&m);
	return 1;
    }
    header_len = len;
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlHeader,&h);

    E2SM_XER_PRINT(NULL,&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlMessage,&m);

    len = e2sm::encode(&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlMessage,NULL,&m,&message);
    if (len < 0) {
	free(header);
	header = NULL;
	header_len = 0;
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlMessage,&m);
	return 1;
    }
    message_len = len;
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlMessage,&m);

    encoded = true;
    return true;
}

bool SliceUeBindRequest::encode()
{
    if (encoded)
	return true;

    E2SM_NEXRAN_E2SM_NexRAN_ControlHeader_t h;
    E2SM_NEXRAN_E2SM_NexRAN_ControlMessage_t m;

    memset(&h,0,sizeof(h));
    memset(&m,0,sizeof(m));

    h.present = E2SM_NEXRAN_E2SM_NexRAN_ControlHeader_PR_controlHeaderFormat1;
    h.choice.controlHeaderFormat1.controlMessageId = \
	E2SM_NEXRAN_E2SM_NexRAN_ControlMessage_Id_sliceUeBindRequest;

    m.present = E2SM_NEXRAN_E2SM_NexRAN_ControlMessage_PR_controlMessageFormat1;
    m.choice.controlMessageFormat1.present = \
	E2SM_NEXRAN_E2SM_NexRAN_ControlMessage_Format1_PR_sliceUeBindRequest;
    m.choice.controlMessageFormat1.choice.sliceUeBindRequest.sliceName.size = \
	strlen(slice.c_str());
    m.choice.controlMessageFormat1.choice.sliceUeBindRequest.sliceName.buf = \
	(uint8_t *)malloc(strlen(slice.c_str()) + 1);
    strcpy((char *)m.choice.controlMessageFormat1.choice.sliceUeBindRequest.sliceName.buf,
	   slice.c_str());
    for (auto it = imsis.begin(); it != imsis.end(); ++it) {
	E2SM_NEXRAN_IMSI_t *ie = (E2SM_NEXRAN_IMSI_t *)calloc(1,sizeof(*ie));
	ie->size = strlen(it->c_str());
	ie->buf = (uint8_t *)malloc(ie->size + 1);
	strcpy((char *)ie->buf,it->c_str());
	ASN_SEQUENCE_ADD(&m.choice.controlMessageFormat1.choice.sliceUeBindRequest.imsiList.list,ie);
    }

    E2SM_XER_PRINT(NULL,&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlHeader,&h);

    ssize_t len = e2sm::encode(&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlHeader,NULL,&h,&header);
    if (len < 0) {
	mdclog_write(MDCLOG_ERR,"failed to encode sliceUeBindRequest header\n");
	header = NULL;
	header_len = 0;
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlHeader,&h);
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlMessage,&m);
	return 1;
    }
    header_len = len;
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlHeader,&h);

    E2SM_XER_PRINT(NULL,&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlMessage,&m);

    len = e2sm::encode(&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlMessage,NULL,&m,&message);
    if (len < 0) {
	mdclog_write(MDCLOG_ERR,"failed to encode sliceUeBindRequest message\n");
	free(header);
	header = NULL;
	header_len = 0;
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlMessage,&m);
	return 1;
    }
    message_len = len;
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlMessage,&m);

    encoded = true;
    return true;
}

bool SliceUeUnbindRequest::encode()
{
    if (encoded)
	return true;

    E2SM_NEXRAN_E2SM_NexRAN_ControlHeader_t h;
    E2SM_NEXRAN_E2SM_NexRAN_ControlMessage_t m;

    memset(&h,0,sizeof(h));
    memset(&m,0,sizeof(m));

    h.present = E2SM_NEXRAN_E2SM_NexRAN_ControlHeader_PR_controlHeaderFormat1;
    h.choice.controlHeaderFormat1.controlMessageId = \
	E2SM_NEXRAN_E2SM_NexRAN_ControlMessage_Id_sliceUeUnbindRequest;

    m.present = E2SM_NEXRAN_E2SM_NexRAN_ControlMessage_PR_controlMessageFormat1;
    m.choice.controlMessageFormat1.present = \
	E2SM_NEXRAN_E2SM_NexRAN_ControlMessage_Format1_PR_sliceUeUnbindRequest;
    m.choice.controlMessageFormat1.choice.sliceUeUnbindRequest.sliceName.size = \
	strlen(slice.c_str());
    m.choice.controlMessageFormat1.choice.sliceUeUnbindRequest.sliceName.buf = \
	(uint8_t *)malloc(strlen(slice.c_str()) + 1);
    strcpy((char *)m.choice.controlMessageFormat1.choice.sliceUeUnbindRequest.sliceName.buf,
	   slice.c_str());
    for (auto it = imsis.begin(); it != imsis.end(); ++it) {
	E2SM_NEXRAN_IMSI_t *ie = (E2SM_NEXRAN_IMSI_t *)calloc(1,sizeof(*ie));
	ie->size = strlen(it->c_str());
	ie->buf = (uint8_t *)malloc(ie->size + 1);
	strcpy((char *)ie->buf,it->c_str());
	ASN_SEQUENCE_ADD(&m.choice.controlMessageFormat1.choice.sliceUeUnbindRequest.imsiList.list,ie);
    }

    E2SM_XER_PRINT(NULL,&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlHeader,&h);

    ssize_t len = e2sm::encode(&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlHeader,NULL,&h,&header);
    if (len < 0) {
	header = NULL;
	header_len = 0;
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlHeader,&h);
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlMessage,&m);
	return 1;
    }
    header_len = len;
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlHeader,&h);

    E2SM_XER_PRINT(NULL,&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlMessage,&m);

    len = e2sm::encode(&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlMessage,NULL,&m,&message);
    if (len < 0) {
	free(header);
	header = NULL;
	header_len = 0;
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlMessage,&m);
	return 1;
    }
    message_len = len;
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_ControlMessage,&m);

    encoded = true;
    return true;
}

bool EventTrigger::encode()
{
    if (encoded)
	return true;

    E2SM_NEXRAN_E2SM_NexRAN_EventTriggerDefinition_t td;

    memset(&td,0,sizeof(td));

    if (on_events) {
	td.present = E2SM_NEXRAN_E2SM_NexRAN_EventTriggerDefinition_PR_ranEventDefinition;
    }
    else {
	td.present = E2SM_NEXRAN_E2SM_NexRAN_EventTriggerDefinition_PR_periodDefinition;
	td.choice.periodDefinition.period = period;
    }

    E2SM_XER_PRINT(NULL,&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_EventTriggerDefinition,&td);

    ssize_t len = e2sm::encode(&asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_EventTriggerDefinition,NULL,&td,&buf);
    if (len < 0) {
	buf = NULL;
	buf_len = 0;
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_EventTriggerDefinition,&td);
	return false;
    }
    buf_len = len;
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_NEXRAN_E2SM_NexRAN_EventTriggerDefinition,&td);

    encoded = true;
    return true;
}

}
}
