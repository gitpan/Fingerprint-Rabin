#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "rabin64.h"

MODULE = Fingerprint::Rabin::Internal PACKAGE = Fingerprint::Rabin::Internal

void 
fp_init()
	PPCODE:
{
	fingerprint_init();
}

fingerprint_t *
fp_buffer(buffer)
	SV *buffer
	CODE:
{
	fingerprint_t *f; 
	char          *text;
	fingerprint_t  tmp;
	STRLEN         text_len;

	New(0, f, 1, fingerprint_t);

	text = (char *) SvPV(buffer, text_len);
	
	tmp = fingerprint_from_buffer(text, (int) text_len);
	memcpy(f, &tmp, sizeof(fingerprint_t));
	
	RETVAL = f;
}
	OUTPUT:
	RETVAL

int
fp_compare(f1, f2)
	fingerprint_t *f1
	fingerprint_t *f2
	CODE:
{
#if FINGERPRINT_USE_INTEGRAL_TYPE
	RETVAL = fingerprint_equal_f(*f1, *f2);
#else
	RETVAL = fingerprint_equal(*f1, *f2);
#endif
}
	OUTPUT:
	RETVAL

fingerprint_t *
fp_combine(f1, f2)
	fingerprint_t *f1
	fingerprint_t *f2
	CODE:
{
	fingerprint_t *f;
	fingerprint_t  tmp;

	New(0, f, 1, fingerprint_t);
	
	tmp = fingerprint_combine(*f1, *f2);
	memcpy(f, &tmp, sizeof(fingerprint_t));

	RETVAL = f;
}
	OUTPUT:
	RETVAL

unsigned int
fp_hash(fp)
	fingerprint_t *fp
	CODE:
{
	RETVAL = fingerprint_hash(*fp);
}
	OUTPUT:
	RETVAL

void
fp_free(fp)
	fingerprint_t *fp
	CODE:
{
	Safefree(fp);
}
