#include "lib.h"
#include "zlib.h"

static void * zlib_ref = NULL;

int ZEXPORT deflateInit_ OF((z_streamp strm, int level,
                                     const char *version, int stream_size)) {
	if(zlib_ref == NULL) {
		zlib_ref = namespace_get_ref(namespace_num_zlib);
	}
	return MESSAGE_SYNC_SEND_r(zlib_ref, level, stream_size, 0, 0, strm, (char *)version, NULL, NULL, 0);
}

int ZEXPORT deflate OF((z_streamp strm, int flush)) {
	return MESSAGE_SYNC_SEND_r(zlib_ref, flush, 0, 0, 0, strm, NULL, NULL, NULL, 1);
}

int ZEXPORT deflateEnd OF((z_streamp strm)) {
	return MESSAGE_SYNC_SEND_r(zlib_ref, 0, 0, 0, 0, strm, NULL, NULL, NULL, 2);
}
