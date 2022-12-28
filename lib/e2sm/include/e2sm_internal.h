#ifndef _E2SM_INTERNAL_H_
#define _E2SM_INTERNAL_H_

#include "asn_application.h"
#include "per_encoder.h"

#define E2SM_XER_PRINT(stream,type,pdu)					\
    do {								\
	if (e2sm::xer_print)						\
	    xer_fprint((stream == NULL) ? stderr : stream,type,pdu);				\
    } while (0);

namespace e2sm
{

extern bool xer_print;

ssize_t encode(
    const struct asn_TYPE_descriptor_s *td,
    const asn_per_constraints_t *constraints,void *sptr,
    unsigned char **buf);

}

#endif /* _E2SM_INTERNAL_H_ */
