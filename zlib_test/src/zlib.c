#include "lib.h"
#include "zlib.h"

static void * zlib_ref = NULL;
static void * zlib_id  = NULL;

int ZEXPORT deflateInit_ OF((z_streamp strm, int level,
                                     const char *version, int stream_size)) {
	if(zlib_ref == NULL) {
		zlib_ref = namespace_get_ref(namespace_num_zlib);
		zlib_id  = namespace_get_id(namespace_num_zlib);
	}
	return ccall_rrcc_r(zlib_ref, zlib_id, 0, level, stream_size, strm, (char *)version);
}

int ZEXPORT deflate OF((z_streamp strm, int flush)) {
	return ccall_rc_r(zlib_ref, zlib_id, 1, flush, strm);
}

int ZEXPORT deflateEnd OF((z_streamp strm)) {
	return ccall_c_r(zlib_ref, zlib_id, 2, strm);
}
