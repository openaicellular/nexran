
#include <cstring>
#include <sstream>

#include "mdclog/mdclog.h"

#include "e2ap.h"
#include "e2sm.h"
#include "e2sm_internal.h"
#include "e2sm_zylinium.h"

#include "E2SM_ZYLINIUM_RANfunction-Description.h"
#include "E2SM_ZYLINIUM_E2SM-Zylinium-ControlHeader.h"
#include "E2SM_ZYLINIUM_E2SM-Zylinium-ControlMessage.h"
#include "E2SM_ZYLINIUM_MaskConfigRequest.h"
#include "E2SM_ZYLINIUM_MaskStatusReport.h"
#include "E2SM_ZYLINIUM_E2SM-Zylinium-EventTriggerDefinition.h"
#include "E2SM_ZYLINIUM_E2SM-Zylinium-ControlOutcome.h"
#include "E2SM_ZYLINIUM_E2SM-Zylinium-IndicationHeader.h"
#include "E2SM_ZYLINIUM_E2SM-Zylinium-IndicationMessage.h"
#include "E2SM_ZYLINIUM_BlockedMask.h"

namespace e2sm
{
namespace zylinium
{

static inline const char* hex_char_to_bin(char c)
{
    switch(toupper(c))
    {
    case '0': return "0000";
    case '1': return "0001";
    case '2': return "0010";
    case '3': return "0011";
    case '4': return "0100";
    case '5': return "0101";
    case '6': return "0110";
    case '7': return "0111";
    case '8': return "1000";
    case '9': return "1001";
    case 'A': return "1010";
    case 'B': return "1011";
    case 'C': return "1100";
    case 'D': return "1101";
    case 'E': return "1110";
    case 'F': return "1111";
    }
    return "XXXX";
}

bool BlockedMask::hex_str_to_bitset(
    const std::string& hex, std::bitset<MAXMASKLEN>& bitset)
{
    if (hex[0] != '0' || hex[1] != 'x') {
	mdclog_write(MDCLOG_ERR,"hex_str_to_bitset: failed to decode: invalid prefix");
	return false;
    }

    if (((signed)hex.length() - 2) * 4 > MAXMASKLEN + 3) {
	mdclog_write(MDCLOG_ERR,"hex_str_to_bitset: input %s of length %lu > %d bits",
		     hex.c_str(), hex.length(), MAXMASKLEN);
	return false;
    }

    std::string bin_reversed;
    std::string bin;

    // Convert hex chars to binary
    for (unsigned int i = 2; i < hex.length(); ++i)
	bin_reversed += hex_char_to_bin(hex[i]);

    // Reverse the binary string back
    for (int i = (signed)bin_reversed.length() - 1; i >= 0; i--)
	bin += bin_reversed[i];

    // If the leading hex char has leading zeroes in the binary representation
    // that should be cut off because of the MAXMASKLEN, cut them off
    bin = bin.substr(0, MAXMASKLEN);

    // Zero pad the end if need be
    for (int i = bin.length(); i < MAXMASKLEN; i++)
	bin += "0";

    // Update the input bitset.
    bitset.reset();
    for (int i = 0; i < bin.length(); i++) {
	if (bin[i] == '1')
	    bitset.set(i);
    }

    return true;
}

std::string MaskStatusReport::to_string(char group_delim,char item_delim)
{
    std::stringstream ss;

    ss << "MaskStatusReport()" << group_delim;
    ss << "dl_mask={mask=" << dl_mask.mask << item_delim
       << "bitcount=" << dl_mask.mask_bitset.count() << item_delim
       << "bitstring=" << dl_mask.mask_bitset.to_string() << item_delim
       << "start=" << dl_mask.start << item_delim
       << "end=" << dl_mask.end << item_delim
       << "id=" << dl_mask.id << "}" << group_delim;
    ss << "ul_mask={mask=" << ul_mask.mask << item_delim
       << "bitcount=" << ul_mask.mask_bitset.count() << item_delim
       << "bitstring=" << ul_mask.mask_bitset.to_string() << item_delim
       << "start=" << ul_mask.start << item_delim
       << "end=" << ul_mask.end << item_delim
       << "id=" << ul_mask.id << "}" << group_delim;
    ss << "dl_def" << dl_def << group_delim;
    ss << "ul_def" << ul_def << group_delim;
    ss << "dl_sched=[";
    for (auto it = dl_sched.begin(); it != dl_sched.end(); ++it) {
	ss << "{mask=" << it->mask << item_delim
	   << "bitcount=" << it->mask_bitset.count() << item_delim
	   << "bitstring=" << it->mask_bitset.to_string() << item_delim
	   << "start=" << it->start << item_delim
	   << "end=" << it->end << item_delim
	   << "id=" << it->id << "}" << item_delim;
    }
    ss << "]" << group_delim;
    ss << "ul_sched=[";
    for (auto it = ul_sched.begin(); it != ul_sched.end(); ++it) {
	ss << "{mask=" << it->mask << item_delim
	   << "bitcount=" << it->mask_bitset.count() << item_delim
	   << "bitstring=" << it->mask_bitset.to_string() << item_delim
	   << "start=" << it->start << item_delim
	   << "end=" << it->end << item_delim
	   << "id=" << it->id << "}" << item_delim;
    }
    ss << "]";

    return ss.str();
}

static void decode_blocked_mask(E2SM_ZYLINIUM_BlockedMask *em,BlockedMask *m)
{
    if (em->mask.buf && em->mask.size > 0)
	m->set_mask_from_str(std::string((char *)em->mask.buf,0,em->mask.size));
    m->start = em->start;
    m->end = em->end;
    m->id = em->id;
}

static void encode_blocked_mask(BlockedMask& m,E2SM_ZYLINIUM_BlockedMask *em)
{
    em->mask.size = strlen(m.mask.c_str());
    em->mask.buf = (uint8_t *)malloc(em->mask.size);
    memcpy(em->mask.buf,m.mask.c_str(),em->mask.size);
    em->start = m.start;
    em->end = m.end;
    em->id = m.id;
}

static MaskStatusReport *decode_mask_status_report(E2SM_ZYLINIUM_MaskStatusReport *r)
{
    std::string dl_def;
    std::string ul_def;
    BlockedMask dl_mask;
    BlockedMask ul_mask;
    std::list<BlockedMask> dl_sched;
    std::list<BlockedMask> ul_sched;

    if (r->dlDefault.buf && r->dlDefault.size > 0)
	dl_def = std::string((char *)r->dlDefault.buf,0,r->dlDefault.size);
    if (r->ulDefault.buf && r->ulDefault.size > 0)
	ul_def = std::string((char *)r->ulDefault.buf,0,r->ulDefault.size);
    decode_blocked_mask(&r->dlMask,&dl_mask);
    decode_blocked_mask(&r->ulMask,&ul_mask);
    for (int i = 0; i < r->dlSched.list.count; ++i) {
	E2SM_ZYLINIUM_BlockedMask_t *item = \
	    (E2SM_ZYLINIUM_BlockedMask_t *)r->dlSched.list.array[i];
	BlockedMask m;
	decode_blocked_mask(item,&m);
	dl_sched.push_back(m);
    }
    for (int i = 0; i < r->ulSched.list.count; ++i) {
	E2SM_ZYLINIUM_BlockedMask_t *item = \
	    (E2SM_ZYLINIUM_BlockedMask_t *)r->ulSched.list.array[i];
	BlockedMask m;
	decode_blocked_mask(item,&m);
	ul_sched.push_back(m);
    }

    return new MaskStatusReport(dl_mask,ul_mask,dl_def,ul_def,dl_sched,ul_sched);
}

Indication *ZyliniumModel::decode(
    e2ap::Indication *ind,
    unsigned char *header,ssize_t header_len,
    unsigned char *message,ssize_t message_len)
{
    E2SM_ZYLINIUM_E2SM_Zylinium_IndicationHeader_t h;
    E2SM_ZYLINIUM_E2SM_Zylinium_IndicationMessage_t m;
    void *ptr;

    memset(&h,0,sizeof(h));
    memset(&m,0,sizeof(m));
    
    asn_dec_rval_t dres;
    bool have_header = false,have_message = false;

    if (header && header_len > 0) {
	ptr = &h;
	dres = aper_decode(
	    NULL,&asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_IndicationHeader,
	    &ptr,header,header_len,0,0);
	if (dres.code != RC_OK) {
	    mdclog_write(MDCLOG_ERR,"failed to decode zylinium indication header (len %lu, code %d)\n",
			 header_len,dres.code);
	    return NULL;
	}
	have_header = true;
	E2SM_XER_PRINT(NULL,&asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_IndicationHeader,&h);
    }
    if (message && message_len > 0) {
	ptr = &m;
	dres = aper_decode(
	    NULL,&asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_IndicationMessage,
	    &ptr,message,message_len,0,0);
	if (dres.code != RC_OK) {
	    mdclog_write(MDCLOG_ERR,"failed to decode zylinium indication message (len %lu, code %d)\n",
			 message_len,dres.code);
	    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_IndicationHeader,&h);
	    return NULL;
	}
	have_message = true;
	E2SM_XER_PRINT(NULL,&asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_IndicationMessage,&m);
    }
    if (!have_header) {
	mdclog_write(MDCLOG_ERR,"missing zylinium indication header; aborting\n");
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_IndicationHeader,&h);
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_IndicationMessage,&m);
	return NULL;
    }
    if (!have_message) {
	mdclog_write(MDCLOG_ERR,"missing zylinium indication message; aborting\n");
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_IndicationHeader,&h);
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_IndicationMessage,&m);
	return NULL;
    }

    if (m.present != E2SM_ZYLINIUM_E2SM_Zylinium_IndicationMessage_PR_maskStatusReport) {
	mdclog_write(MDCLOG_ERR,"unsupported zylinium indication message (%d); aborting\n",
		     m.present);
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_IndicationHeader,&h);
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_IndicationMessage,&m);
	return NULL;
    }

    // XXX: meid

    MaskStatusReport *report = decode_mask_status_report(&m.choice.maskStatusReport);

    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_IndicationHeader,&h);
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_IndicationMessage,&m);

    return new MaskStatusIndication(this,report);
}

ControlOutcome *ZyliniumModel::decode(
    e2ap::ControlAck *ack,
    unsigned char *outcome,ssize_t outcome_len)
{
    if (!outcome || outcome_len <= 0)
	return NULL;

    E2SM_ZYLINIUM_E2SM_Zylinium_ControlOutcome_t o;
    memset(&o,0,sizeof(o));

    void *ptr = &o;
    asn_dec_rval_t dres = aper_decode(
        NULL,&asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlOutcome,
	&ptr,outcome,outcome_len,0,0);
    if (dres.code != RC_OK) {
	mdclog_write(MDCLOG_ERR,"failed to decode zylinium control outcome (len %lu, code %d)\n",
		     outcome_len,dres.code);
	return NULL;
    }
    E2SM_XER_PRINT(NULL,&asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlOutcome,&o);
    if (o.present != E2SM_ZYLINIUM_E2SM_Zylinium_ControlOutcome_PR_controlOutcomeFormat1) {
	mdclog_write(MDCLOG_ERR,"unsupported zylinium control outcome (%d); aborting\n",
		     o.present);
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlOutcome,&o);
	return NULL;
    }
    if (o.choice.controlOutcomeFormat1.present != E2SM_ZYLINIUM_E2SM_Zylinium_ControlOutcome_Format1_PR_maskStatusReport) {
	mdclog_write(MDCLOG_ERR,"unsupported zylinium control outcome format1 (%d); aborting\n",
		     o.choice.controlOutcomeFormat1.present);
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlOutcome,&o);
	return NULL;
    }

    MaskStatusReport *report = decode_mask_status_report(&o.choice.controlOutcomeFormat1.choice.maskStatusReport);

    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlOutcome,&o);

    return new MaskStatusControlOutcome(this,report);
}

ControlOutcome *ZyliniumModel::decode(
    e2ap::ControlFailure *failure,
    unsigned char *outcome,ssize_t outcome_len)
{
    if (!outcome || outcome_len <= 0)
	return NULL;

    E2SM_ZYLINIUM_E2SM_Zylinium_ControlOutcome_t o;
    memset(&o,0,sizeof(o));

    void *ptr = &o;
    asn_dec_rval_t dres = aper_decode(
        NULL,&asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlOutcome,
	&ptr,outcome,outcome_len,0,0);
    if (dres.code != RC_OK) {
	mdclog_write(MDCLOG_ERR,"failed to decode zylinium control outcome (len %lu, code %d)\n",
		     outcome_len,dres.code);
	return NULL;
    }
    E2SM_XER_PRINT(NULL,&asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlOutcome,&o);
    if (o.present != E2SM_ZYLINIUM_E2SM_Zylinium_ControlOutcome_PR_controlOutcomeFormat1) {
	mdclog_write(MDCLOG_ERR,"unsupported zylinium control outcome (%d); aborting\n",
		     o.present);
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlOutcome,&o);
	return NULL;
    }
    if (o.choice.controlOutcomeFormat1.present != E2SM_ZYLINIUM_E2SM_Zylinium_ControlOutcome_Format1_PR_maskStatusReport) {
	mdclog_write(MDCLOG_ERR,"unsupported zylinium control outcome format1 (%d); aborting\n",
		     o.choice.controlOutcomeFormat1.present);
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlOutcome,&o);
	return NULL;
    }

    MaskStatusReport *report = decode_mask_status_report(
        &o.choice.controlOutcomeFormat1.choice.maskStatusReport);

    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlOutcome,&o);

    return new MaskStatusControlOutcome(this,report);
}

bool MaskConfigRequest::encode()
{
    if (encoded)
	return true;

    E2SM_ZYLINIUM_E2SM_Zylinium_ControlHeader_t h;
    E2SM_ZYLINIUM_E2SM_Zylinium_ControlMessage_t m;

    memset(&h,0,sizeof(h));
    memset(&m,0,sizeof(m));

    h.present = E2SM_ZYLINIUM_E2SM_Zylinium_ControlHeader_PR_controlHeaderFormat1;
    h.choice.controlHeaderFormat1.controlMessageId = \
	E2SM_ZYLINIUM_E2SM_Zylinium_ControlMessage_Id_maskConfigRequest;

    m.present = E2SM_ZYLINIUM_E2SM_Zylinium_ControlMessage_PR_controlMessageFormat1;
    m.choice.controlMessageFormat1.present = \
	E2SM_ZYLINIUM_E2SM_Zylinium_ControlMessage_Format1_PR_maskConfigRequest;

    E2SM_ZYLINIUM_MaskConfigRequest *req = \
	&m.choice.controlMessageFormat1.choice.maskConfigRequest;
    req->dlDefault.size = strlen(dl_def.c_str());
    req->dlDefault.buf = (uint8_t *)malloc(req->dlDefault.size);
    memcpy(req->dlDefault.buf,dl_def.c_str(),req->dlDefault.size);
    req->ulDefault.size = strlen(ul_def.c_str());
    req->ulDefault.buf = (uint8_t *)malloc(req->ulDefault.size);
    memcpy(req->ulDefault.buf,ul_def.c_str(),req->ulDefault.size);

    E2SM_ZYLINIUM_BlockedMask_t *bm;
    for (auto it = dl_sched.begin(); it != dl_sched.end(); ++it) {
	bm = (E2SM_ZYLINIUM_BlockedMask_t *)calloc(sizeof(*bm),1);
	encode_blocked_mask(*it,bm);
	ASN_SEQUENCE_ADD(&req->dlSched.list,bm);
    }

    for (auto it = ul_sched.begin(); it != ul_sched.end(); ++it) {
	bm = (E2SM_ZYLINIUM_BlockedMask_t *)calloc(sizeof(*bm),1);
	encode_blocked_mask(*it,bm);
	ASN_SEQUENCE_ADD(&req->ulSched.list,bm);
    }

    E2SM_XER_PRINT(NULL,&asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlHeader,&h);

    ssize_t len = e2sm::encode(&asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlHeader,NULL,&h,&header);
    if (len < 0) {
	header = NULL;
	header_len = 0;
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlHeader,&h);
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlMessage,&m);
	return false;
    }
    header_len = len;
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlHeader,&h);

    E2SM_XER_PRINT(NULL,&asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlMessage,&m);

    len = e2sm::encode(&asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlMessage,NULL,&m,&message);
    if (len < 0) {
	free(header);
	header = NULL;
	header_len = 0;
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlMessage,&m);
	return false;
    }
    message_len = len;
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlMessage,&m);

    encoded = true;

    return true;
}

bool MaskStatusRequest::encode()
{
    if (encoded)
	return true;

    E2SM_ZYLINIUM_E2SM_Zylinium_ControlHeader_t h;
    E2SM_ZYLINIUM_E2SM_Zylinium_ControlMessage_t m;

    memset(&h,0,sizeof(h));
    memset(&m,0,sizeof(m));

    h.present = E2SM_ZYLINIUM_E2SM_Zylinium_ControlHeader_PR_controlHeaderFormat1;
    h.choice.controlHeaderFormat1.controlMessageId = \
	E2SM_ZYLINIUM_E2SM_Zylinium_ControlMessage_Id_maskStatusRequest;

    m.present = E2SM_ZYLINIUM_E2SM_Zylinium_ControlMessage_PR_controlMessageFormat1;
    m.choice.controlMessageFormat1.present = \
	E2SM_ZYLINIUM_E2SM_Zylinium_ControlMessage_Format1_PR_maskStatusRequest;

    E2SM_XER_PRINT(NULL,&asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlHeader,&h);

    ssize_t len = e2sm::encode(&asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlHeader,NULL,&h,&header);
    if (len < 0) {
	header = NULL;
	header_len = 0;
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlHeader,&h);
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlMessage,&m);
	return 1;
    }
    header_len = len;
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlHeader,&h);

    E2SM_XER_PRINT(NULL,&asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlMessage,&m);

    len = e2sm::encode(&asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlMessage,NULL,&m,&message);
    if (len < 0) {
	free(header);
	header = NULL;
	header_len = 0;
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlMessage,&m);
	return 1;
    }
    message_len = len;
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_ControlMessage,&m);

    encoded = true;
    return true;
}

bool EventTrigger::encode()
{
    if (encoded)
	return true;

    E2SM_ZYLINIUM_E2SM_Zylinium_EventTriggerDefinition_t td;

    memset(&td,0,sizeof(td));

    td.present = E2SM_ZYLINIUM_E2SM_Zylinium_EventTriggerDefinition_PR_ranEventDefinition;

    E2SM_XER_PRINT(NULL,&asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_EventTriggerDefinition,&td);

    ssize_t len = e2sm::encode(&asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_EventTriggerDefinition,NULL,&td,&buf);
    if (len < 0) {
	buf = NULL;
	buf_len = 0;
	ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_EventTriggerDefinition,&td);
	return false;
    }
    buf_len = len;
    ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_E2SM_ZYLINIUM_E2SM_Zylinium_EventTriggerDefinition,&td);

    encoded = true;
    return true;
}

}
}
