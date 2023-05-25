
#include <cassert>
#include <string>
#include <functional>

#include "mdclog/mdclog.h"
#include "rmr/RIC_message_types.h"

#include "e2ap.h"
#include "e2sm.h"

#include "E2AP_E2AP-PDU.h"
#include "E2AP_InitiatingMessage.h"
#include "E2AP_SuccessfulOutcome.h"
#include "E2AP_UnsuccessfulOutcome.h"
#include "E2AP_ProtocolIE-ID.h"
#include "E2AP_ProtocolIE-Field.h"
#include "E2AP_Cause.h"

namespace e2ap
{

bool xer_print = true;

static RanFunctionId next_ran_function_id = 0;

RanFunctionId get_next_ran_function_id()
{
    return next_ran_function_id++;
}

ssize_t encode(
    const struct asn_TYPE_descriptor_s *td,
    const asn_per_constraints_t *constraints,void *sptr,
    unsigned char **buf)
{
    ssize_t encoded;

    encoded = aper_encode_to_new_buffer(td,constraints,sptr,(void **)buf);
    if (encoded < 0)
	return -1;

    return encoded;
}

ssize_t encode_pdu(E2AP_E2AP_PDU_t *pdu,unsigned char **buf,ssize_t *len)
{
    ssize_t encoded;

    encoded = encode(&asn_DEF_E2AP_E2AP_PDU,0,pdu,buf);
    if (encoded < 0)
	return -1;

    *len = encoded;

    return encoded;
}

int decode(
    const struct asn_TYPE_descriptor_s *td,
    void *ptr,const unsigned char *buf,const size_t len)
{
    asn_dec_rval_t dres;

    dres = aper_decode(NULL,td,&ptr,buf,len,0,0);
    if (dres.code != RC_OK) {
	mdclog_write(MDCLOG_ERR,
		     "failed to decode type (%d)\n",dres.code);
	return -1;
    }

    return 0;
}

#define CASE_E2AP_I(id,name)						\
    case id:								\
    mdclog_write(MDCLOG_INFO,"decoded initiating " #name " (%ld)\n",id); \
    break

#define CASE_E2AP_S(id,name)						\
    case id:								\
    mdclog_write(MDCLOG_INFO,"decoded successful outcome " #name " (%ld)\n",id); \
    break

#define CASE_E2AP_U(id,name)						\
    case id:								\
    mdclog_write(MDCLOG_INFO,"decoded unsuccessful outcome " #name " (%ld)\n",id); \
    break

int decode_pdu(E2AP_E2AP_PDU_t *pdu,
	       const unsigned char* const buf,const size_t len)
{
    asn_dec_rval_t dres;

    dres = aper_decode(NULL,&asn_DEF_E2AP_E2AP_PDU,(void **)&pdu,buf,len,0,0);
    if (dres.code != RC_OK) {
	mdclog_write(MDCLOG_ERR,
		     "failed to decode PDU len %lu (%d)\n",len,dres.code);
	return -1;
    }

    E2AP_XER_PRINT(NULL,&asn_DEF_E2AP_E2AP_PDU,pdu);

    switch (pdu->present) {
    case E2AP_E2AP_PDU_PR_initiatingMessage:
	switch (pdu->choice.initiatingMessage.procedureCode) {
	    CASE_E2AP_I(E2AP_ProcedureCode_id_Reset,Reset);
	    CASE_E2AP_I(E2AP_ProcedureCode_id_RICsubscription,
			RICsubscription);
	    CASE_E2AP_I(E2AP_ProcedureCode_id_RICsubscriptionDelete,
			RICsubscriptionDelete);
	    CASE_E2AP_I(E2AP_ProcedureCode_id_RICcontrol,RICcontrol);
	    CASE_E2AP_I(E2AP_ProcedureCode_id_RICserviceQuery,RICserviceQuery);
	    CASE_E2AP_I(E2AP_ProcedureCode_id_RICindication,RICindication);
	    CASE_E2AP_I(E2AP_ProcedureCode_id_ErrorIndication,ErrorIndication);
	default:
	    mdclog_write(MDCLOG_ERR,"unknown procedure ID (%d) for initiating message\n",
		       (int)pdu->choice.initiatingMessage.procedureCode);
	    return -1;
	}
	break;
    case E2AP_E2AP_PDU_PR_successfulOutcome:
	switch (pdu->choice.successfulOutcome.procedureCode) {
	    CASE_E2AP_S(E2AP_ProcedureCode_id_E2setup,E2SetupResponse);
	    CASE_E2AP_S(E2AP_ProcedureCode_id_Reset,Reset);
	    CASE_E2AP_S(E2AP_ProcedureCode_id_RICserviceUpdate,RICserviceUpdate);
	    CASE_E2AP_S(E2AP_ProcedureCode_id_RICsubscription,
			RICsubscription);
	    CASE_E2AP_S(E2AP_ProcedureCode_id_RICsubscriptionDelete,
			RICsubscriptionDelete);
	    CASE_E2AP_S(E2AP_ProcedureCode_id_RICcontrol,RICcontrol);
	default:
	    mdclog_write(MDCLOG_ERR,"unknown procedure ID (%d) for successful outcome\n",
		       (int)pdu->choice.successfulOutcome.procedureCode);
	    return -1;
	}
	break;
    case E2AP_E2AP_PDU_PR_unsuccessfulOutcome:
	switch (pdu->choice.unsuccessfulOutcome.procedureCode) {
	    CASE_E2AP_U(E2AP_ProcedureCode_id_E2setup,E2setupFailure);
	    CASE_E2AP_U(E2AP_ProcedureCode_id_RICserviceUpdate,RICserviceUpdate);
	    CASE_E2AP_U(E2AP_ProcedureCode_id_RICsubscription,
			RICsubscription);
	    CASE_E2AP_U(E2AP_ProcedureCode_id_RICsubscriptionDelete,
			RICsubscriptionDelete);
	    CASE_E2AP_U(E2AP_ProcedureCode_id_RICcontrol,RICcontrol);
	default:
	    mdclog_write(MDCLOG_ERR,"unknown procedure ID (%d) for unsuccessful outcome\n",
		       (int)pdu->choice.unsuccessfulOutcome.procedureCode);
	    return -1;
	}
	break;
    default:
	mdclog_write(MDCLOG_ERR,"unknown presence (%d)\n",(int)pdu->present);
	return -1;
    }

    return 0;
}

bool E2AP::init()
{
    return true;
}

/**
 * We don't want to expose any asn1c goo in the public library header,
 * so in lieu of exposing per-message class decode() functions (where we
 * would have to use the pimpl pattern or an annoying factory style),
 * just hack it for now.
 */
static ControlAck *decode_control_ack(E2AP *e2ap,E2AP_E2AP_PDU_t *pdu)
{
    assert(pdu->present == E2AP_E2AP_PDU_PR_successfulOutcome
	   && pdu->choice.successfulOutcome.procedureCode \
	      == E2AP_ProcedureCode_id_RICcontrol);

    E2AP_RICcontrolAcknowledge_t *msg;
    E2AP_RICcontrolAcknowledge_IEs_t *ie,**ptr;
    unsigned char *outcome = NULL;
    size_t outcome_len = 0;

    ControlAck *ret = new ControlAck();

    msg = &pdu->choice.successfulOutcome.value.choice.RICcontrolAcknowledge;

    for (ptr = msg->protocolIEs.list.array;
	 ptr < &msg->protocolIEs.list.array[msg->protocolIEs.list.count];
	 ptr++) {
	ie = (E2AP_RICcontrolAcknowledge_IEs_t *)*ptr;
	if (ie->id == E2AP_ProtocolIE_ID_id_RICrequestID) {
	    ret->requestor_id = ie->value.choice.RICrequestID.ricRequestorID;
	    ret->instance_id = ie->value.choice.RICrequestID.ricInstanceID;
	}
	else if (ie->id == E2AP_ProtocolIE_ID_id_RANfunctionID) {
	    ret->function_id = ie->value.choice.RANfunctionID;
	}
	else if (ie->id == E2AP_ProtocolIE_ID_id_RICcallProcessID) {
	}
	else if (ie->id == E2AP_ProtocolIE_ID_id_RICcontrolStatus) {
	    ret->status = ie->value.choice.RICcontrolStatus;
	}
	else if (ie->id == E2AP_ProtocolIE_ID_id_RICcontrolOutcome
		 && ie->value.choice.RICcontrolOutcome.size > 0) {
	    outcome_len = ie->value.choice.RICcontrolOutcome.size;
	    outcome = ie->value.choice.RICcontrolOutcome.buf;
	}
    }

    std::shared_ptr<ControlRequest> req =
	e2ap->lookup_control(ret->requestor_id,ret->instance_id);
    if (!req) {
	mdclog_write(MDCLOG_ERR,"control requestID not found (%ld,%ld); ignoring",
		     ret->requestor_id,ret->instance_id);
	delete ret;
	return NULL;
    }

    if (req->control && outcome && outcome_len > 0) {
	e2sm::Model *model = req->control->get_model();
	assert(model != NULL);
	ret->outcome = model->decode(
            ret,outcome,outcome_len);
	if (!ret->outcome) {
	    mdclog_write(MDCLOG_ERR,"error decoding model control outcome");
	    delete ret;
	    return NULL;
	}
    }

    return ret;
}

static ControlFailure *decode_control_failure(E2AP *e2ap,E2AP_E2AP_PDU_t *pdu)
{
    assert(pdu->present == E2AP_E2AP_PDU_PR_unsuccessfulOutcome
	   && pdu->choice.unsuccessfulOutcome.procedureCode \
	      == E2AP_ProcedureCode_id_RICcontrol);

    E2AP_RICcontrolFailure_t *msg;
    E2AP_RICcontrolFailure_IEs_t *ie,**ptr;
    unsigned char *outcome = NULL;
    size_t outcome_len = 0;

    ControlFailure *ret = new ControlFailure();

    msg = &pdu->choice.unsuccessfulOutcome.value.choice.RICcontrolFailure;

    for (ptr = msg->protocolIEs.list.array;
	 ptr < &msg->protocolIEs.list.array[msg->protocolIEs.list.count];
	 ptr++) {
	ie = (E2AP_RICcontrolFailure_IEs_t *)*ptr;
	if (ie->id == E2AP_ProtocolIE_ID_id_RICrequestID) {
	    ret->requestor_id = ie->value.choice.RICrequestID.ricRequestorID;
	    ret->instance_id = ie->value.choice.RICrequestID.ricInstanceID;
	}
	else if (ie->id == E2AP_ProtocolIE_ID_id_RANfunctionID) {
	    ret->function_id = ie->value.choice.RANfunctionID;
	}
	else if (ie->id == E2AP_ProtocolIE_ID_id_RICcallProcessID) {
	}
	else if (ie->id == E2AP_ProtocolIE_ID_id_Cause) {
	    ret->cause = ie->value.choice.Cause.present;
	    ret->cause_detail = ie->value.choice.Cause.choice.misc;
	}
	else if (ie->id == E2AP_ProtocolIE_ID_id_RICcontrolOutcome
		 && ie->value.choice.RICcontrolOutcome.size > 0) {
	    outcome_len = ie->value.choice.RICcontrolOutcome.size;
	    outcome = ie->value.choice.RICcontrolOutcome.buf;
	}
    }

    std::shared_ptr<ControlRequest> req =
	e2ap->lookup_control(ret->requestor_id,ret->instance_id);
    if (!req) {
	mdclog_write(MDCLOG_ERR,"control requestID not found (%ld,%ld); ignoring",
		     ret->requestor_id,ret->instance_id);
	delete ret;
	return NULL;
    }

    if (req->control && outcome && outcome_len > 0) {
	e2sm::Model *model = req->control->get_model();
	assert(model != NULL);
	e2sm::ControlOutcome *sm_outcome = model->decode(
            ret,outcome,outcome_len);
	if (!sm_outcome) {
	    mdclog_write(MDCLOG_ERR,"error decoding model control outcome");
	    delete ret;
	    return NULL;
	}
	ret->outcome = sm_outcome;
    }

    return ret;
}

static Indication *decode_indication(E2AP *e2ap,E2AP_E2AP_PDU_t *pdu,int subid)
{
    assert(pdu->present == E2AP_E2AP_PDU_PR_initiatingMessage
	   && pdu->choice.initiatingMessage.procedureCode \
	      == E2AP_ProcedureCode_id_RICindication);

    E2AP_RICindication_t *msg;
    E2AP_RICindication_IEs_t *ie,**ptr;
    unsigned char *header = NULL,*message = NULL;
    size_t header_len = 0,message_len = 0;

    Indication *ret = new Indication();

    msg = &pdu->choice.initiatingMessage.value.choice.RICindication;

    for (ptr = msg->protocolIEs.list.array;
	 ptr < &msg->protocolIEs.list.array[msg->protocolIEs.list.count];
	 ptr++) {
	ie = (E2AP_RICindication_IEs_t *)*ptr;
	if (ie->id == E2AP_ProtocolIE_ID_id_RICrequestID) {
	    ret->requestor_id = ie->value.choice.RICrequestID.ricRequestorID;
	    ret->instance_id = ie->value.choice.RICrequestID.ricInstanceID;
	}
	else if (ie->id == E2AP_ProtocolIE_ID_id_RANfunctionID) {
	    ret->function_id = ie->value.choice.RANfunctionID;
	}
	else if (ie->id == E2AP_ProtocolIE_ID_id_RICactionID) {
	    ret->action_id = ie->value.choice.RICactionID;
	}
	else if (ie->id == E2AP_ProtocolIE_ID_id_RICindicationSN) {
	    ret->serial_number = ie->value.choice.RICindicationSN;
	}
	else if (ie->id == E2AP_ProtocolIE_ID_id_RICindicationType) {
	    ret->type = ie->value.choice.RICindicationType;
	}
	else if (ie->id == E2AP_ProtocolIE_ID_id_RICindicationHeader
		 && ie->value.choice.RICindicationHeader.size > 0) {
	    header = ie->value.choice.RICindicationHeader.buf;
	    header_len = ie->value.choice.RICindicationHeader.size;
	}
	else if (ie->id == E2AP_ProtocolIE_ID_id_RICindicationMessage
		 && ie->value.choice.RICindicationMessage.size > 0) {
	    message = ie->value.choice.RICindicationMessage.buf;
	    message_len = ie->value.choice.RICindicationMessage.size;
	}
    }

    if (subid < 0) {
	mdclog_write(MDCLOG_WARN,"indication has invalid subscriptionID (%d); aborting processing",
		     subid);
	delete ret;
	return NULL;
    }

    /*
     * Lookup service model via RMR subscriptionID, and have the
     * model->decode_indication do its thing on header/message.  The
     * indication should only be a response to a SubscriptionRequest,
     * due to the required action_id.
     *
     * (We could later move to a model where we lookup the model via RMR
     * meid (NodeB) and its RanFunctionID, but this is a little more
     * cumbersome.)
     */
    std::shared_ptr<SubscriptionResponse> resp = e2ap->lookup_subscription(subid);
    if (!resp) {
	mdclog_write(MDCLOG_ERR,"indication subscriptionID not found (%d); ignoring",
		     subid);
	delete ret;
	return NULL;
    }
    ret->subscription_request = resp->req;

    if (resp->req->trigger) {
	e2sm::Model *model = resp->req->trigger->get_model();
	assert(model != NULL);
	e2sm::Indication *sm_ind = model->decode(
	    ret,header,header_len,message,message_len);
	if (!sm_ind) {
	    mdclog_write(MDCLOG_ERR,"error decoding model indication header/message");
	    delete ret;
	    return NULL;
	}
	ret->model = sm_ind;
	sm_ind->parent = ret;
    }
    return ret;
}

static ErrorIndication *decode_error_indication(E2AP *e2ap,E2AP_E2AP_PDU_t *pdu)
{
    assert(pdu->present == E2AP_E2AP_PDU_PR_initiatingMessage
	   && pdu->choice.initiatingMessage.procedureCode \
	      == E2AP_ProcedureCode_id_ErrorIndication);

    E2AP_ErrorIndication_t *msg;
    E2AP_ErrorIndication_IEs_t *ie,**ptr;

    ErrorIndication *ret = new ErrorIndication();

    msg = &pdu->choice.initiatingMessage.value.choice.ErrorIndication;

    for (ptr = msg->protocolIEs.list.array;
	 ptr < &msg->protocolIEs.list.array[msg->protocolIEs.list.count];
	 ptr++) {
	ie = (E2AP_ErrorIndication_IEs_t *)*ptr;
	if (ie->id == E2AP_ProtocolIE_ID_id_RICrequestID) {
	    ret->requestor_id = ie->value.choice.RICrequestID.ricRequestorID;
	    ret->instance_id = ie->value.choice.RICrequestID.ricInstanceID;
	}
	else if (ie->id == E2AP_ProtocolIE_ID_id_RANfunctionID) {
	    ret->function_id = ie->value.choice.RANfunctionID;
	}
	else if (ie->id == E2AP_ProtocolIE_ID_id_Cause) {
	    ret->cause = ie->value.choice.Cause.present;
	    ret->cause_detail = ie->value.choice.Cause.choice.misc;
	}
	else if (ie->id == E2AP_ProtocolIE_ID_id_CriticalityDiagnostics) {
	}
    }

    return ret;
}

static SubscriptionResponse *decode_subscription_response(E2AP *e2ap,E2AP_E2AP_PDU_t *pdu,std::string& xid)
{
    assert(pdu->present == E2AP_E2AP_PDU_PR_successfulOutcome
	   && pdu->choice.successfulOutcome.procedureCode \
	      == E2AP_ProcedureCode_id_RICsubscription);

    E2AP_RICsubscriptionResponse_t *msg;
    E2AP_RICsubscriptionResponse_IEs_t *ie,**ptr;

    SubscriptionResponse *ret = new SubscriptionResponse();

    msg = &pdu->choice.successfulOutcome.value.choice.RICsubscriptionResponse;

    for (ptr = msg->protocolIEs.list.array;
	 ptr < &msg->protocolIEs.list.array[msg->protocolIEs.list.count];
	 ptr++) {
	ie = (E2AP_RICsubscriptionResponse_IEs_t *)*ptr;
	if (ie->id == E2AP_ProtocolIE_ID_id_RICrequestID) {
	    ret->requestor_id = ie->value.choice.RICrequestID.ricRequestorID;
	    ret->instance_id = ie->value.choice.RICrequestID.ricInstanceID;
	}
	else if (ie->id == E2AP_ProtocolIE_ID_id_RANfunctionID) {
	    ret->function_id = ie->value.choice.RANfunctionID;
	}
	else if (ie->id == E2AP_ProtocolIE_ID_id_RICactions_Admitted) {
	    for (int i = 0; i < ie->value.choice.RICaction_Admitted_List.list.count; ++i) {
		E2AP_RICaction_Admitted_Item_t *aa_item = \
		    (E2AP_RICaction_Admitted_Item_t *)ie->value.choice.RICaction_Admitted_List.list.array[i];
		ret->actions_admitted.push_back(aa_item->ricActionID);
	    }
	}
	else if (ie->id == E2AP_ProtocolIE_ID_id_RICactions_NotAdmitted) {
	    for (int i = 0; i < ie->value.choice.RICaction_NotAdmitted_List.list.count; ++i) {
		E2AP_RICaction_NotAdmitted_Item_t *ana_item = \
		    (E2AP_RICaction_NotAdmitted_Item_t *)ie->value.choice.RICaction_NotAdmitted_List.list.array[i];
		ret->actions_not_admitted.push_back(
		     std::make_tuple(ana_item->ricActionID,
				     (long)ana_item->cause.present,
				     (long)ana_item->cause.choice.misc));
	    }
	}
    }

    /*
     * Lookup original request.
     */
    std::shared_ptr<SubscriptionRequest> req = e2ap->lookup_pending_subscription(xid);
    if (!req) {
	mdclog_write(MDCLOG_ERR,"subscription xid (%s) not found; ignoring",
		     xid.c_str());
	delete ret;
	return NULL;
    }

    ret->req = req;

    return ret;
}

static SubscriptionFailure *decode_subscription_failure(E2AP *e2ap,E2AP_E2AP_PDU_t *pdu,std::string& xid)
{
    assert(pdu->present == E2AP_E2AP_PDU_PR_unsuccessfulOutcome
	   && pdu->choice.unsuccessfulOutcome.procedureCode \
	      == E2AP_ProcedureCode_id_RICsubscription);

    E2AP_RICsubscriptionFailure_t *msg;
    E2AP_RICsubscriptionFailure_IEs_t *ie,**ptr;

    SubscriptionFailure *ret = new SubscriptionFailure();

    msg = &pdu->choice.unsuccessfulOutcome.value.choice.RICsubscriptionFailure;

    for (ptr = msg->protocolIEs.list.array;
	 ptr < &msg->protocolIEs.list.array[msg->protocolIEs.list.count];
	 ptr++) {
	ie = (E2AP_RICsubscriptionFailure_IEs_t *)*ptr;
	if (ie->id == E2AP_ProtocolIE_ID_id_RICrequestID) {
	    ret->requestor_id = ie->value.choice.RICrequestID.ricRequestorID;
	    ret->instance_id = ie->value.choice.RICrequestID.ricInstanceID;
	}
	else if (ie->id == E2AP_ProtocolIE_ID_id_RANfunctionID) {
	    ret->function_id = ie->value.choice.RANfunctionID;
	}
	else if (ie->id == E2AP_ProtocolIE_ID_id_RICactions_NotAdmitted) {
	    for (int i = 0; i < ie->value.choice.RICaction_NotAdmitted_List.list.count; ++i) {
		E2AP_RICaction_NotAdmitted_Item_t *ana_item = \
		    (E2AP_RICaction_NotAdmitted_Item_t *)ie->value.choice.RICaction_NotAdmitted_List.list.array[i];
		ret->actions_not_admitted.push_back(
		     std::make_tuple(ana_item->ricActionID,
				     (long)ana_item->cause.present,
				     (long)ana_item->cause.choice.misc));
	    }
	}
    }

    /*
     * Lookup original request.
     */
    std::shared_ptr<SubscriptionRequest> req = e2ap->lookup_pending_subscription(xid);
    if (!req) {
	mdclog_write(MDCLOG_ERR,"subscription xid (%s) not found; ignoring",
		     xid.c_str());
	delete ret;
	return NULL;
    }

    ret->req = req;

    return ret;
}

static SubscriptionDeleteResponse *decode_subscription_delete_response(E2AP *e2ap,E2AP_E2AP_PDU_t *pdu,std::string& xid)
{
    assert(pdu->present == E2AP_E2AP_PDU_PR_successfulOutcome
	   && pdu->choice.successfulOutcome.procedureCode \
	      == E2AP_ProcedureCode_id_RICsubscriptionDelete);

    E2AP_RICsubscriptionDeleteResponse_t *msg;
    E2AP_RICsubscriptionDeleteResponse_IEs_t *ie,**ptr;

    SubscriptionDeleteResponse *ret = new SubscriptionDeleteResponse();

    msg = &pdu->choice.successfulOutcome.value.choice.RICsubscriptionDeleteResponse;

    for (ptr = msg->protocolIEs.list.array;
	 ptr < &msg->protocolIEs.list.array[msg->protocolIEs.list.count];
	 ptr++) {
	ie = (E2AP_RICsubscriptionDeleteResponse_IEs_t *)*ptr;
	if (ie->id == E2AP_ProtocolIE_ID_id_RICrequestID) {
	    ret->requestor_id = ie->value.choice.RICrequestID.ricRequestorID;
	    ret->instance_id = ie->value.choice.RICrequestID.ricInstanceID;
	}
	else if (ie->id == E2AP_ProtocolIE_ID_id_RANfunctionID) {
	    ret->function_id = ie->value.choice.RANfunctionID;
	}
    }

    /*
     * Lookup original request.
     */
    std::shared_ptr<SubscriptionDeleteRequest> req = e2ap->lookup_pending_subscription_delete(xid);
    if (!req) {
	mdclog_write(MDCLOG_ERR,"subscription delete xid (%s) not found; ignoring",
		     xid.c_str());
	delete ret;
	return NULL;
    }

    ret->req = req;

    return ret;
}

static SubscriptionDeleteFailure *decode_subscription_delete_failure(E2AP *e2ap,E2AP_E2AP_PDU_t *pdu,std::string& xid)
{
    assert(pdu->present == E2AP_E2AP_PDU_PR_unsuccessfulOutcome
	   && pdu->choice.unsuccessfulOutcome.procedureCode \
	      == E2AP_ProcedureCode_id_RICsubscriptionDelete);

    E2AP_RICsubscriptionDeleteFailure_t *msg;
    E2AP_RICsubscriptionDeleteFailure_IEs_t *ie,**ptr;

    SubscriptionDeleteFailure *ret = new SubscriptionDeleteFailure();

    msg = &pdu->choice.unsuccessfulOutcome.value.choice.RICsubscriptionDeleteFailure;

    for (ptr = msg->protocolIEs.list.array;
	 ptr < &msg->protocolIEs.list.array[msg->protocolIEs.list.count];
	 ptr++) {
	ie = (E2AP_RICsubscriptionDeleteFailure_IEs_t *)*ptr;
	if (ie->id == E2AP_ProtocolIE_ID_id_RICrequestID) {
	    ret->requestor_id = ie->value.choice.RICrequestID.ricRequestorID;
	    ret->instance_id = ie->value.choice.RICrequestID.ricInstanceID;
	}
	else if (ie->id == E2AP_ProtocolIE_ID_id_RANfunctionID) {
	    ret->function_id = ie->value.choice.RANfunctionID;
	}
	else if (ie->id == E2AP_ProtocolIE_ID_id_Cause) {
	    ret->cause = ie->value.choice.Cause.present;
	    ret->cause_detail = ie->value.choice.Cause.choice.misc;
	}
	else if (ie->id == E2AP_ProtocolIE_ID_id_CriticalityDiagnostics) {
	}
    }

    /*
     * Lookup original request.
     */
    std::shared_ptr<SubscriptionDeleteRequest> req = e2ap->lookup_pending_subscription_delete(xid);
    if (!req) {
	mdclog_write(MDCLOG_ERR,"subscription delete xid (%s) not found; ignoring",xid.c_str());
	delete ret;
	return NULL;
    }

    ret->req = req;

    return ret;
}

bool E2AP::handle_message(const unsigned char *buf,ssize_t len,int subid,
			  std::string meid,std::string xid)
{
    E2AP_E2AP_PDU_t pdu;
    int ret;
    bool bret = false;

    memset(&pdu,0,sizeof(pdu));
    ret = decode_pdu(&pdu,buf,len);
    if (ret < 0) {
	mdclog_write(MDCLOG_ERR,"failed to decode E2AP PDU from %s\n",
		     meid.c_str());
	return false;
    }

    switch (pdu.present) {
    case E2AP_E2AP_PDU_PR_initiatingMessage:
	switch (pdu.choice.initiatingMessage.procedureCode) {
	case E2AP_ProcedureCode_id_RICindication:
	    {
		Indication *ind = decode_indication(this,&pdu,subid);
		if (ind)
		    bret = agent_if->handle(ind);
	    }
	    break;
	case E2AP_ProcedureCode_id_ErrorIndication:
	    {
		ErrorIndication *ind = decode_error_indication(this,&pdu);
		if (ind)
		    bret = agent_if->handle(ind);
	    }
	    break;
	default:
	    mdclog_write(MDCLOG_WARN,
			 "unsupported initiatingMessage procedure %ld from %s\n",
			 pdu.choice.initiatingMessage.procedureCode,meid.c_str());
	    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2AP_E2AP_PDU,&pdu);
	    return false;
	};
	break;
    case E2AP_E2AP_PDU_PR_successfulOutcome:
	switch (pdu.choice.successfulOutcome.procedureCode) {
	case E2AP_ProcedureCode_id_RICsubscription:
	    {
		SubscriptionResponse *resp = decode_subscription_response(this,&pdu,xid);
		if (resp) {
		    mdclog_write(MDCLOG_DEBUG,"subscription request succeeded (xid=%s,subid=%d)\n",
				 xid.c_str(),subid);
		    mutex.lock();
		    subscriptions[subid] = std::shared_ptr<SubscriptionResponse>(resp);
		    pending_subscriptions.erase(xid);
		    mutex.unlock();
		    bret = agent_if->handle(resp);
		}
	    }
	    break;
	case E2AP_ProcedureCode_id_RICsubscriptionDelete:
	    {
		SubscriptionDeleteResponse *resp = decode_subscription_delete_response(this,&pdu,xid);
		if (resp) {
		    mdclog_write(MDCLOG_DEBUG,"subscription delete request succeeded (xid=%s,subid=%d)\n",
				 xid.c_str(),subid);
		    mutex.lock();
		    subscriptions.erase(subid);
		    pending_deletes.erase(xid);
		    mutex.unlock();
		    bret = agent_if->handle(resp);
		}
	    }
	    break;
	case E2AP_ProcedureCode_id_RICcontrol:
	    {
		ControlAck *ack = decode_control_ack(this,&pdu);
		if (ack) {
		    bret = agent_if->handle(ack);
		    if (ack->req != NULL)
			controls.erase(ack->req->instance_id);
		    delete ack;
		}
	    }
	    break;
	default:
	    mdclog_write(MDCLOG_WARN,
			 "unsupported successfulOutcome procedure %ld from %s\n",
			 pdu.choice.initiatingMessage.procedureCode,meid.c_str());
	    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2AP_E2AP_PDU,&pdu);
	    return false;
	};
	break;
    case E2AP_E2AP_PDU_PR_unsuccessfulOutcome:
	switch (pdu.choice.unsuccessfulOutcome.procedureCode) {
	case E2AP_ProcedureCode_id_RICsubscription:
	    {
		SubscriptionFailure *resp = decode_subscription_failure(this,&pdu,xid);
		if (resp)
		    bret = agent_if->handle(resp);
	    }
	    break;
	case E2AP_ProcedureCode_id_RICsubscriptionDelete:
	    {
		SubscriptionDeleteFailure *resp = decode_subscription_delete_failure(this,&pdu,xid);
		if (resp)
		    bret = agent_if->handle(resp);
	    }
	    break;
	case E2AP_ProcedureCode_id_RICcontrol:
	    {
		ControlFailure *failure = decode_control_failure(this,&pdu);
		if (failure) {
		    bret = agent_if->handle(failure);
		    if (failure->req != NULL)
			controls.erase(failure->req->instance_id);
		    delete failure;
		}
	    }
	    break;
	default:
	    mdclog_write(MDCLOG_WARN,
			 "unsupported unsuccessfulOutcome procedure %ld from %s\n",
			 pdu.choice.initiatingMessage.procedureCode,meid.c_str());
	    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2AP_E2AP_PDU,&pdu);
	    return false;
	};
	break;
    default:
	mdclog_write(MDCLOG_ERR,"unsupported presence %u from %s\n",
		     pdu.present,meid.c_str());
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2AP_E2AP_PDU,&pdu);
	return false;
    }

    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2AP_E2AP_PDU,&pdu);

    return bret;
}

/*
 * NB: we create RMR subscription IDs by combining our requestor_id and
 * instance_id.  We read the E2AP spec to indicate that requestor is the
 * source xApp id, that is random per app instance; and instance_id can
 * be determined by the app and is local to the per-app requestor_id.
 * But the e2submgr currently tracks control requests based solely on
 * instance_id, so we have to send something randomish.  So, we do this
 * generally for any subid; e2 doesn't care; and we don't care what the
 * subids are; we just react to whatever message we get.
 */
static int req_to_subid(long req_id,long inst_id)
{
    std::size_t rh = std::hash<std::string>{}(std::to_string(req_id));
    std::size_t ih = std::hash<std::string>{}(std::to_string(inst_id));
    return (rh ^ ih);
}

static std::string req_to_xid(long req_id,long inst_id)
{
    return std::string(std::to_string(req_id) + "." + std::to_string(inst_id));
}

bool E2AP::send_control_request(std::shared_ptr<ControlRequest> req,
				const std::string& meid)
{
    if (!req->encode())
	return false;

    if (req->ack_request == CONTROL_REQUEST_ACK) {
	mutex.lock();
	if (controls.count(req->instance_id) > 0) {
	    mutex.unlock();
	    return false;
	}

	controls[req->instance_id] = req;
	mutex.unlock();
    }

    int subid = req_to_subid(req->requestor_id,req->instance_id);
    bool ret = agent_if->send_message(req->get_buf(),req->get_len(),
				      RIC_CONTROL_REQ,subid,meid,
				      std::string());
    if (!ret && req->ack_request == CONTROL_REQUEST_ACK) {
	mutex.lock();
	controls.erase(req->instance_id);
	mutex.unlock();
    }
    else
	mdclog_write(MDCLOG_DEBUG,
		     "sent control request" \
		     " (subid=%d,requestor_id=%ld,instance_id=%ld,meid=%s)\n",
		     subid,req->requestor_id,req->instance_id,meid.c_str());

    return ret;
}

bool E2AP::send_subscription_request(std::shared_ptr<SubscriptionRequest> req,
				     const std::string& meid)
{
    if (!req->encode())
	return false;

    std::string xid = req_to_xid(req->requestor_id,req->instance_id);
    mutex.lock();
    if (pending_subscriptions.count(xid) > 0) {
	mutex.unlock();
	mdclog_write(MDCLOG_WARN,
		     "subscription request already exists; not sending" \
		     " (xid=%s,requestor_id=%ld,instance_id=%ld,meid=%s)\n",
		     xid.c_str(),req->requestor_id,req->instance_id,meid.c_str());
	return false;
    }
    pending_subscriptions[xid] = req;
    mutex.unlock();

    bool ret = agent_if->send_message(req->get_buf(),req->get_len(),
				      RIC_SUB_REQ,-1,meid,xid);
    if (!ret) {
	mutex.lock();
	pending_subscriptions.erase(xid);
	mutex.unlock();
    }
    else
	mdclog_write(MDCLOG_DEBUG,
		     "sent subscription request" \
		     " (xid=%s,requestor_id=%ld,instance_id=%ld,meid=%s)\n",
		     xid.c_str(),req->requestor_id,req->instance_id,meid.c_str());

    return ret;
}

bool E2AP::_send_subscription_delete_request(std::shared_ptr<SubscriptionDeleteRequest> req,
					     const std::string& meid, bool locked)
{
    if (!req->encode())
	return false;

    std::string xid = req_to_xid(req->requestor_id,req->instance_id);
    int subid = req_to_subid(req->requestor_id,req->instance_id);

    if (!locked)
	mutex.lock();
    if (pending_deletes.count(xid) > 0) {
	if (!locked)
	    mutex.unlock();
	mdclog_write(MDCLOG_WARN,
		     "subscription delete request already exists; not sending" \
		     " (subid=%d,requestor_id=%ld,instance_id=%ld,meid=%s)\n",
		     subid,req->requestor_id,req->instance_id,meid.c_str());
	return false;
    }
    pending_deletes[xid] = req;
    if (!locked)
	mutex.unlock();

    bool ret = agent_if->send_message(req->get_buf(),req->get_len(),
				      RIC_SUB_DEL_REQ,subid,meid,xid);
    if (!ret) {
	if (!locked)
	    mutex.lock();
	pending_deletes.erase(xid);
	if (!locked)
	    mutex.unlock();
    }
    else
	mdclog_write(MDCLOG_DEBUG,
		     "sent subscription delete request" \
		     " (subid=%d,requestor_id=%ld,instance_id=%ld,meid=%s)\n",
		     subid,req->requestor_id,req->instance_id,meid.c_str());

    return ret;
}

bool E2AP::send_subscription_delete_request(std::shared_ptr<SubscriptionDeleteRequest> req,
					    const std::string& meid)
{
    const std::lock_guard<std::mutex> lock(mutex);

    return _send_subscription_delete_request(req, meid, true);
}

bool E2AP::delete_all_subscriptions
    (std::string& meid)
{
    const std::lock_guard<std::mutex> lock(mutex);

    for (auto it = subscriptions.begin(); it != subscriptions.end(); ++it) {
	if (it->second->req->meid != meid)
	    continue;

	std::shared_ptr<e2ap::SubscriptionDeleteRequest> req = \
	    std::make_shared<e2ap::SubscriptionDeleteRequest>(
		it->second->requestor_id,it->second->instance_id,
		it->second->function_id);
	req->set_meid(meid);
	_send_subscription_delete_request(req, meid, true);
    }

    return true;
}

std::shared_ptr<SubscriptionRequest> E2AP::lookup_pending_subscription
    (std::string& xid)
{
    const std::lock_guard<std::mutex> lock(mutex);

    if (pending_subscriptions.count(xid) > 0)
	return pending_subscriptions[xid];
    return NULL;
}

std::shared_ptr<SubscriptionResponse> E2AP::lookup_subscription
    (int subid)
{
    const std::lock_guard<std::mutex> lock(mutex);

    if (subscriptions.count(subid) > 0)
	return subscriptions[subid];
    return NULL;
}

std::shared_ptr<SubscriptionDeleteRequest> E2AP::lookup_pending_subscription_delete
    (std::string& xid)
{
    const std::lock_guard<std::mutex> lock(mutex);

    if (pending_deletes.count(xid) > 0)
	return pending_deletes[xid];
    return NULL;
}

std::shared_ptr<ControlRequest> E2AP::lookup_control
    (long requestor_id,long instance_id)
{
    const std::lock_guard<std::mutex> lock(mutex);

    if (controls.count(instance_id) > 0)
	return controls[instance_id];
    return NULL;
}

bool ControlRequest::encode()
{
    E2AP_E2AP_PDU_t pdu;
    E2AP_RICcontrolRequest_t *req;
    E2AP_RICcontrolRequest_IEs_t *ie;
    unsigned char *iebuf;
    size_t iebuflen;

    if (encoded)
	return true;

    memset(&pdu,0,sizeof(pdu));
    pdu.present = E2AP_E2AP_PDU_PR_initiatingMessage;
    pdu.choice.initiatingMessage.procedureCode = E2AP_ProcedureCode_id_RICcontrol;
    pdu.choice.initiatingMessage.criticality = E2AP_Criticality_reject;
    pdu.choice.initiatingMessage.value.present = E2AP_InitiatingMessage__value_PR_RICcontrolRequest;
    req = &pdu.choice.initiatingMessage.value.choice.RICcontrolRequest;

    ie = (E2AP_RICcontrolRequest_IEs_t *)calloc(1,sizeof(*ie));
    ie->id = E2AP_ProtocolIE_ID_id_RICrequestID;
    ie->criticality = E2AP_Criticality_reject;
    ie->value.present = E2AP_RICcontrolRequest_IEs__value_PR_RICrequestID;
    ie->value.choice.RICrequestID.ricRequestorID = requestor_id;
    ie->value.choice.RICrequestID.ricInstanceID = instance_id;
    ASN_SEQUENCE_ADD(&req->protocolIEs.list,ie);

    ie = (E2AP_RICcontrolRequest_IEs_t *)calloc(1,sizeof(*ie));
    ie->id = E2AP_ProtocolIE_ID_id_RANfunctionID;
    ie->criticality = E2AP_Criticality_reject;
    ie->value.present = E2AP_RICcontrolRequest_IEs__value_PR_RANfunctionID;
    ie->value.choice.RANfunctionID = function_id;
    ASN_SEQUENCE_ADD(&req->protocolIEs.list,ie);

    iebuf = control->get_call_process_id();
    iebuflen = control->get_call_process_id_len();
    if (iebuf && iebuflen > 0) {
	ie = (E2AP_RICcontrolRequest_IEs_t *)calloc(1,sizeof(*ie));
	ie->id = E2AP_ProtocolIE_ID_id_RICcallProcessID;
	ie->criticality = E2AP_Criticality_reject;
	ie->value.present = E2AP_RICcontrolRequest_IEs__value_PR_RICcallProcessID;
	ie->value.choice.RICcallProcessID.buf = (uint8_t *)malloc(iebuflen);
	memcpy(ie->value.choice.RICcallProcessID.buf,iebuf,iebuflen);
	ie->value.choice.RICcallProcessID.size = iebuflen;
	ASN_SEQUENCE_ADD(&req->protocolIEs.list,ie);
    }

    iebuf = control->get_header();
    iebuflen = control->get_header_len();
    ie = (E2AP_RICcontrolRequest_IEs_t *)calloc(1,sizeof(*ie));
    ie->id = E2AP_ProtocolIE_ID_id_RICcontrolHeader;
    ie->criticality = E2AP_Criticality_reject;
    ie->value.present = E2AP_RICcontrolRequest_IEs__value_PR_RICcontrolHeader;
    ie->value.choice.RICcontrolHeader.buf = (uint8_t *)malloc(iebuflen);
    memcpy(ie->value.choice.RICcontrolHeader.buf,iebuf,iebuflen);
    ie->value.choice.RICcontrolHeader.size = iebuflen;
    ASN_SEQUENCE_ADD(&req->protocolIEs.list,ie);

    iebuf = control->get_message();
    iebuflen = control->get_message_len();
    ie = (E2AP_RICcontrolRequest_IEs_t *)calloc(1,sizeof(*ie));
    ie->id = E2AP_ProtocolIE_ID_id_RICcontrolMessage;
    ie->criticality = E2AP_Criticality_reject;
    ie->value.present = E2AP_RICcontrolRequest_IEs__value_PR_RICcontrolMessage;
    ie->value.choice.RICcontrolMessage.buf = (uint8_t *)malloc(iebuflen);
    memcpy(ie->value.choice.RICcontrolMessage.buf,iebuf,iebuflen);
    ie->value.choice.RICcontrolMessage.size = iebuflen;
    ASN_SEQUENCE_ADD(&req->protocolIEs.list,ie);

    ie = (E2AP_RICcontrolRequest_IEs_t *)calloc(1,sizeof(*ie));
    ie->id = E2AP_ProtocolIE_ID_id_RICcontrolAckRequest;
    ie->criticality = E2AP_Criticality_reject;
    ie->value.present = E2AP_RICcontrolRequest_IEs__value_PR_RICcontrolAckRequest;
    ie->value.choice.RICcontrolAckRequest = (E2AP_RICcontrolAckRequest_t)ack_request;
    ASN_SEQUENCE_ADD(&req->protocolIEs.list,ie);

    E2AP_XER_PRINT(NULL,&asn_DEF_E2AP_E2AP_PDU,&pdu);

    if (encode_pdu(&pdu,&buf,&len) < 0) {
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2AP_E2AP_PDU,&pdu);
	return false;
    }

    /*
    E2AP_E2AP_PDU_t pdud;
    memset(&pdud,0,sizeof(pdud));
    if (e2ap_decode_pdu(&pdud,*buffer,*len) < 0) {
        E2AP_WARN("Failed to encode E2setupRequest\n");
    }
    */

    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2AP_E2AP_PDU,&pdu);

    encoded = true;
    return true;
}

bool SubscriptionRequest::encode()
{
    E2AP_E2AP_PDU_t pdu;
    E2AP_RICsubscriptionRequest_t *req;
    E2AP_RICsubscriptionRequest_IEs_t *ie;
    unsigned char *iebuf;
    size_t iebuflen;

    if (encoded)
	return true;

    memset(&pdu,0,sizeof(pdu));
    pdu.present = E2AP_E2AP_PDU_PR_initiatingMessage;
    pdu.choice.initiatingMessage.procedureCode = E2AP_ProcedureCode_id_RICsubscription;
    pdu.choice.initiatingMessage.criticality = E2AP_Criticality_reject;
    pdu.choice.initiatingMessage.value.present = E2AP_InitiatingMessage__value_PR_RICsubscriptionRequest;
    req = &pdu.choice.initiatingMessage.value.choice.RICsubscriptionRequest;

    ie = (E2AP_RICsubscriptionRequest_IEs_t *)calloc(1,sizeof(*ie));
    ie->id = E2AP_ProtocolIE_ID_id_RICrequestID;
    ie->criticality = E2AP_Criticality_reject;
    ie->value.present = E2AP_RICsubscriptionRequest_IEs__value_PR_RICrequestID;
    ie->value.choice.RICrequestID.ricRequestorID = requestor_id;
    ie->value.choice.RICrequestID.ricInstanceID = instance_id;
    ASN_SEQUENCE_ADD(&req->protocolIEs.list,ie);

    ie = (E2AP_RICsubscriptionRequest_IEs_t *)calloc(1,sizeof(*ie));
    ie->id = E2AP_ProtocolIE_ID_id_RANfunctionID;
    ie->criticality = E2AP_Criticality_reject;
    ie->value.present = E2AP_RICsubscriptionRequest_IEs__value_PR_RANfunctionID;
    ie->value.choice.RANfunctionID = function_id;
    ASN_SEQUENCE_ADD(&req->protocolIEs.list,ie);

    ie = (E2AP_RICsubscriptionRequest_IEs_t *)calloc(1,sizeof(*ie));
    ie->id = E2AP_ProtocolIE_ID_id_RICsubscriptionDetails;
    ie->criticality = E2AP_Criticality_reject;
    ie->value.present = E2AP_RICsubscriptionRequest_IEs__value_PR_RICsubscriptionDetails;

    if (trigger && trigger->get_buf_len() > 0) {
	ie->value.choice.RICsubscriptionDetails.ricEventTriggerDefinition.buf = \
	    (uint8_t *)malloc(trigger->get_buf_len());
	memcpy(ie->value.choice.RICsubscriptionDetails.ricEventTriggerDefinition.buf,
	       trigger->get_buf(),trigger->get_buf_len());
	ie->value.choice.RICsubscriptionDetails.ricEventTriggerDefinition.size = \
	    trigger->get_buf_len();
    }
    for (auto it = actions.begin(); it != actions.end(); ++it) {
	Action *action = *it;
	E2AP_RICaction_ToBeSetup_ItemIEs_t *aie = \
	    (E2AP_RICaction_ToBeSetup_ItemIEs_t *)calloc(1,sizeof(*aie));
	aie->id = E2AP_ProtocolIE_ID_id_RICaction_ToBeSetup_Item;
	aie->criticality = E2AP_Criticality_reject;
	aie->value.present = E2AP_RICaction_ToBeSetup_ItemIEs__value_PR_RICaction_ToBeSetup_Item;
	E2AP_RICaction_ToBeSetup_Item_t *item = &aie->value.choice.RICaction_ToBeSetup_Item;
	item->ricActionID = action->id;
	item->ricActionType = (long)action->type;
	if (action->definition && action->definition->get_buf_len()) {
	    item->ricActionDefinition = \
		(E2AP_RICactionDefinition_t *)calloc(1,sizeof(*item->ricActionDefinition));
	    item->ricActionDefinition->buf = (uint8_t *)malloc(action->definition->get_buf_len());
	    memcpy(item->ricActionDefinition->buf,action->definition->get_buf(),
		   action->definition->get_buf_len());
	    item->ricActionDefinition->size = action->definition->get_buf_len();
	}
	// XXX: subsequent_action/time_to_wait
	ASN_SEQUENCE_ADD(&ie->value.choice.RICsubscriptionDetails.ricAction_ToBeSetup_List.list,aie);
    }
    ASN_SEQUENCE_ADD(&req->protocolIEs.list,ie);

    E2AP_XER_PRINT(NULL,&asn_DEF_E2AP_E2AP_PDU,&pdu);

    if (encode_pdu(&pdu,&buf,&len) < 0) {
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2AP_E2AP_PDU,&pdu);
	return false;
    }

    /*
    E2AP_E2AP_PDU_t pdud;
    memset(&pdud,0,sizeof(pdud));
    if (e2ap_decode_pdu(&pdud,*buffer,*len) < 0) {
        E2AP_WARN("Failed to encode E2setupRequest\n");
    }
    */

    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2AP_E2AP_PDU,&pdu);

    encoded = true;
    return true;
}

bool SubscriptionDeleteRequest::encode()
{
    E2AP_E2AP_PDU_t pdu;
    E2AP_RICsubscriptionDeleteRequest_t *req;
    E2AP_RICsubscriptionDeleteRequest_IEs_t *ie;
    unsigned char *iebuf;
    size_t iebuflen;

    if (encoded)
	return true;

    memset(&pdu,0,sizeof(pdu));
    pdu.present = E2AP_E2AP_PDU_PR_initiatingMessage;
    pdu.choice.initiatingMessage.procedureCode = E2AP_ProcedureCode_id_RICsubscriptionDelete;
    pdu.choice.initiatingMessage.criticality = E2AP_Criticality_reject;
    pdu.choice.initiatingMessage.value.present = E2AP_InitiatingMessage__value_PR_RICsubscriptionDeleteRequest;
    req = &pdu.choice.initiatingMessage.value.choice.RICsubscriptionDeleteRequest;

    ie = (E2AP_RICsubscriptionDeleteRequest_IEs_t *)calloc(1,sizeof(*ie));
    ie->id = E2AP_ProtocolIE_ID_id_RICrequestID;
    ie->criticality = E2AP_Criticality_reject;
    ie->value.present = E2AP_RICsubscriptionDeleteRequest_IEs__value_PR_RICrequestID;
    ie->value.choice.RICrequestID.ricRequestorID = requestor_id;
    ie->value.choice.RICrequestID.ricInstanceID = instance_id;
    ASN_SEQUENCE_ADD(&req->protocolIEs.list,ie);

    ie = (E2AP_RICsubscriptionDeleteRequest_IEs_t *)calloc(1,sizeof(*ie));
    ie->id = E2AP_ProtocolIE_ID_id_RANfunctionID;
    ie->criticality = E2AP_Criticality_reject;
    ie->value.present = E2AP_RICsubscriptionDeleteRequest_IEs__value_PR_RANfunctionID;
    ie->value.choice.RANfunctionID = function_id;
    ASN_SEQUENCE_ADD(&req->protocolIEs.list,ie);

    E2AP_XER_PRINT(NULL,&asn_DEF_E2AP_E2AP_PDU,&pdu);

    if (encode_pdu(&pdu,&buf,&len) < 0) {
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2AP_E2AP_PDU,&pdu);
	return false;
    }

    /*
    E2AP_E2AP_PDU_t pdud;
    memset(&pdud,0,sizeof(pdud));
    if (e2ap_decode_pdu(&pdud,*buffer,*len) < 0) {
        E2AP_WARN("Failed to encode E2setupRequest\n");
    }
    */

    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2AP_E2AP_PDU,&pdu);

    encoded = true;
    return true;
}

bool SubscriptionResponse::encode()
{
    return false;
}

bool SubscriptionFailure::encode()
{
    return false;
}

bool SubscriptionDeleteResponse::encode()
{
    return false;
}

bool SubscriptionDeleteFailure::encode()
{
    return false;
}

bool ControlAck::encode()
{
    return false;
}

bool ControlFailure::encode()
{
    return false;
}

Indication::~Indication()
{
    if (call_process_id)
	free(call_process_id);
    if (model)
	delete model;
}

bool Indication::encode()
{
    return false;
}

}
