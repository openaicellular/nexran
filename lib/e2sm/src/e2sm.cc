
#include "e2sm.h"
#include "e2sm_internal.h"

namespace e2sm
{

bool xer_print = true;

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

}
