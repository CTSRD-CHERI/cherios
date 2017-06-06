#include "lib.h"
#include "zlib.h"

static void * zlib_ref = NULL;

int ZEXPORT deflateInit_ OF((z_streamp strm, int level,
                                     const char *version, int stream_size)) {
	if(zlib_ref == NULL) {
		zlib_ref = namespace_get_ref(namespace_num_zlib);
	}
	return (int)message_send(level, stream_size, 0, 0, strm, (char *)version, NULL, NULL, zlib_ref, SYNC_CALL, 0);
}

int ZEXPORT deflate OF((z_streamp strm, int flush)) {
	return (int)message_send(flush, 0, 0, 0, strm, NULL, NULL, NULL, zlib_ref, SYNC_CALL, 1);
}

int ZEXPORT deflateEnd OF((z_streamp strm)) {
	return (int)message_send(0, 0, 0, 0, strm, NULL, NULL, NULL, zlib_ref, SYNC_CALL, 2);
}
