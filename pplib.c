/*
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 bitHOUSEweb S.r.l.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_pplib.h"
  
#include <zlib.h>


/* {{{ Memory management wrappers */
static voidpf php_zlib_alloc(voidpf opaque, uInt items, uInt size) {
	return (voidpf)safe_emalloc(items, size, 0);
}

static void php_zlib_free(voidpf opaque, voidpf address) {
	efree((void*)address);
}
/* }}} */


ZEND_BEGIN_ARG_INFO_EX(arginfo_compressor_append, 0, 0, 1)
	ZEND_ARG_INFO(0, buffer)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_compressor_construct, 0, 0, 0)
	ZEND_ARG_INFO(0, format)
	ZEND_ARG_INFO(0, level)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_compressor_no_params, 0, 0, 0)
ZEND_END_ARG_INFO()

const zend_function_entry compressor_methods[] = {
	PHP_ME(Compressor, __construct, arginfo_compressor_construct, ZEND_ACC_PUBLIC)
	PHP_ME(Compressor, append, arginfo_compressor_append, ZEND_ACC_PUBLIC)
	PHP_ME(Compressor, getFormat, arginfo_compressor_no_params, ZEND_ACC_PUBLIC)
	PHP_ME(Compressor, getOutput, arginfo_compressor_no_params, ZEND_ACC_PUBLIC)
	PHP_ME(Compressor, getOutputSize, arginfo_compressor_no_params, ZEND_ACC_PUBLIC)
	PHP_ME(Compressor, printOutput, arginfo_compressor_no_params, ZEND_ACC_PUBLIC)
	PHP_ME(Compressor, endDocument, arginfo_compressor_no_params, ZEND_ACC_PUBLIC)
	PHP_FE_END
};

zend_class_entry *compressor_entry;

static zend_object_handlers compressor_object_handlers;

struct _compressor_buffer;

typedef struct _compressor_buffer {
	struct _compressor_buffer *next;
	uInt size;
	uInt used;
	Bytef data; // data is allocated directly starting from here for size bytes
} compressor_buffer;

typedef struct _compressor_obj_data {
	z_stream stream;
	int flushMode;
	uint8_t format;
	compressor_buffer *head;
	compressor_buffer *tail;
	compressor_buffer *using;
	zend_object std;
} compressor_obj_data;



static zend_object *compressor_create_object_handler(zend_class_entry *class_type TSRMLS_DC);
static compressor_buffer *add_buffer(compressor_obj_data *intern, uInt size);
static compressor_buffer *get_usable_buffer(compressor_obj_data *intern);
static void empty_buffers(compressor_obj_data *intern);
static void free_buffers(compressor_obj_data *intern);
static void consume_input(compressor_obj_data *intern, z_streamp stream, int flushMode);
static uLong output_size(compressor_obj_data *intern);
static compressor_buffer *get_usable_buffer(compressor_obj_data *intern);


static inline compressor_obj_data *compressor_fetch_object(zend_object *obj) {
      return (compressor_obj_data *)((char *)obj - XtOffsetOf(compressor_obj_data, std));
}
 
#define Z_COMPRESSOR_OBJ_P(zv) compressor_fetch_object(Z_OBJ_P(zv));


static void compressor_free_object_storage_handler(zend_object *obj TSRMLS_DC) {
    zend_object_std_dtor(obj TSRMLS_CC);
    
    compressor_obj_data *intern = compressor_fetch_object(obj);
    deflateEnd(&(intern->stream));
    free_buffers(intern);
    efree(intern);
}

static zend_object *compressor_create_object_handler(zend_class_entry *class_type TSRMLS_DC) {
    compressor_obj_data *intern = ecalloc(1, sizeof(compressor_obj_data) + zend_object_properties_size(class_type));
    memset(intern, 0, sizeof(compressor_obj_data));

    zend_object_std_init(&intern->std, class_type TSRMLS_CC);

    compressor_object_handlers.offset = XtOffsetOf(compressor_obj_data, std);
    compressor_object_handlers.free_obj = compressor_free_object_storage_handler;

    /*object_properties_init(&intern->std, class_type);

    retval.handle = zend_objects_store_put(
        intern,
        (zend_objects_store_dtor_t) zend_objects_destroy_object,
        (zend_objects_free_object_storage_t) compressor_free_object_storage_handler,
        NULL TSRMLS_CC
    );*/

    intern->std.handlers = &compressor_object_handlers;

    return &intern->std;
}


/* {{{ internal function
   adds a new buffer to the linked list */
static compressor_buffer *add_buffer(compressor_obj_data *intern, uInt size) {
	compressor_buffer *buffer = (compressor_buffer *) emalloc(sizeof(compressor_buffer) + sizeof(Bytef) *(size - 1));
	buffer->next = NULL;
	buffer->size = size;
	buffer->used = 0;
	
	if (intern->head == NULL) {
		intern->head = buffer;
		intern->tail = buffer;
	}
	else {
		intern->tail->next = buffer;
		intern->tail = buffer;
	}
	
	return buffer;
}
/* }}} */

/* {{{ internal function
   finds the first buffer that has some space in it to be used */
static compressor_buffer *get_usable_buffer(compressor_obj_data *intern) {
	compressor_buffer *buffer = intern->head, *next;
	
	while(buffer != NULL) {
		next = buffer->next;
		
		if (buffer->used < buffer->size && (next == NULL || next->used == 0)) {
			break;
		}
		
		buffer = next;
	}
	
	return buffer;
}
/* }}} */

/* {{{ internal function
   frees all buffers */
static void free_buffers(compressor_obj_data *intern) {
	compressor_buffer *buffer = intern->head, *next;
	
	intern->stream.next_out = NULL;
	intern->stream.avail_out = 0;
	
	intern->head = NULL;
	intern->tail = NULL;
	intern->using = NULL;
	
	while(buffer != NULL) {
		next = buffer->next;
		buffer->used = 0;
		efree(buffer);
		buffer = next;
	}
}
/* }}} */

/* {{{ internal function
   frees all buffers */
static void empty_buffers(compressor_obj_data *intern) {
	compressor_buffer *buffer = intern->head;
	
	intern->stream.next_out = NULL;
	intern->stream.avail_out = 0;
	
	intern->using = NULL;
	
	while(buffer != NULL) {
		buffer->used = 0;
		buffer = buffer->next;
	}
}
/* }}} */

/* {{{ internal debug function
   finds out the index of a buffer in the list */
#ifdef PPLIB_DEBUG
static int buffer_index(compressor_obj_data *intern, compressor_buffer *src) {
	int id = 0;
	
	compressor_buffer *buffer = intern->head;
	
	while(buffer != NULL && buffer != src) {
		buffer = buffer->next;
		++id;
	}
	
	return id;
}
#endif
/* }}} */

/* {{{ internal function
   consume input until avail_in is zero and adds buffers as needed to the linked list */
static void consume_input(compressor_obj_data *intern, z_streamp stream, int flushMode) {
	int state;
	uInt avail_out = 0, needed;
	compressor_buffer *buffer = intern->using;
	
	do {
		if (stream->avail_out == 0 || stream->next_out == NULL) {
			buffer = get_usable_buffer(intern);
			if (buffer == NULL) {
				if (stream->avail_in != 0) {
					needed = deflateBound(stream, stream->avail_in);
				}
				else {
					needed = deflateBound(stream, stream->total_in) - stream->total_out;
				}
				
				buffer = add_buffer(intern, needed);
			}
			
			intern->using = buffer;
			
			stream->next_out = &buffer->data;
			stream->avail_out = buffer->size;
		}
		
		avail_out = stream->avail_out;
		
#ifdef PPLIB_DEBUG
		zend_error(E_USER_NOTICE, "flushMode %d, avail_in %u, avail_out %u, buffer_used %u, buffer #%d, next_in %lu, next_out %lu", flushMode, stream->avail_in, stream->avail_out, buffer->used, buffer_index(intern, buffer), (unsigned long) stream->next_in, (unsigned long) stream->next_out);
#endif
		
		state = deflate(stream, flushMode);
		
		if (buffer != NULL && avail_out != 0) {
			buffer->used += avail_out - stream->avail_out;
			avail_out = stream->avail_out;
		}

#ifdef PPLIB_DEBUG
		zend_error(E_USER_NOTICE, "flushMode %d, avail_in %u, avail_out %u, buffer_used %u, state %d", flushMode, stream->avail_in, stream->avail_out, buffer->used, state);
#endif
	} while (
		(flushMode == Z_NO_FLUSH && stream->avail_in != 0) ||
		(flushMode != Z_NO_FLUSH && 
			(
				state == Z_OK ||
				(state == Z_BUF_ERROR && stream->avail_out == 0)
			)
		)
	);
}
/* }}} */

/* {{{ internal function
   returns the output size till now */
static uLong output_size(compressor_obj_data *intern) {
	uLong size;
	compressor_buffer *buffer;

	size = 0;
	buffer = intern->head;
	
	while (buffer != NULL && buffer->used != 0) {
		size += buffer->used;
		buffer = buffer->next;
	}
	
	return size;
}
/* }}} */

/* {{{ proto string __construct(int format = FORMAT_GZIP, int level = -1)
   construct the Compressor class, with the given compression level (defaults to -1 = determined by zlib) */
PHP_METHOD(Compressor, __construct) {
	long level, format;
	compressor_obj_data *intern;

	switch(ZEND_NUM_ARGS()) {
		case 2:
			if (zend_parse_parameters(2 TSRMLS_CC, "ll", &format, &level) == FAILURE) {
				return;
			}
			break;
		
		case 1:
			if (zend_parse_parameters(1 TSRMLS_CC, "l", &format) == FAILURE) {
				return;
			}
			level = Z_DEFAULT_COMPRESSION;
			break;
		
		case 0:
			format = PPLIB_FORMAT_GZIP;
			level = Z_DEFAULT_COMPRESSION;
	}
	
	intern = Z_COMPRESSOR_OBJ_P(getThis());
	
	intern->stream.zalloc = php_zlib_alloc;
	intern->stream.zfree = php_zlib_free;
	intern->stream.opaque = Z_NULL;
	
	intern->flushMode = Z_SYNC_FLUSH;
	
	intern->format = format;
	
	if ( Z_OK != deflateInit2( &(intern->stream),
			level,
			Z_DEFLATED,
			format == PPLIB_FORMAT_GZIP ? 31 : (format == PPLIB_FORMAT_ZLIB ? 15 : -15),
			8,
			Z_DEFAULT_STRATEGY ) ) {
		RETURN_FALSE;
	}
}
/* }}} */

/* {{{ proto string append(string data)
   adds the specified data to the compressed stream */
PHP_METHOD(Compressor, append) {
	char *data;
	size_t len;
	compressor_obj_data *intern;

	if (ZEND_NUM_ARGS() == 1) {
		if (zend_parse_parameters(1 TSRMLS_CC, "s", &data, &len) == FAILURE) {
			return;
		}
	}
	else {
		WRONG_PARAM_COUNT;
	}
	
	intern = Z_COMPRESSOR_OBJ_P(getThis());
	
	z_streamp stream = &(intern->stream);

#ifdef PPLIB_DEBUG
	zend_error(E_USER_NOTICE, "data %s, length %d", data, len);
#endif
	
	stream->next_in = (Bytef *) data;
	stream->avail_in = len;
	
	consume_input(intern, stream, Z_NO_FLUSH);
}
/* }}} */

/* {{{ proto string endDocument()
   marks the end of the document requesting for a finish flush */
PHP_METHOD(Compressor, endDocument) {
	compressor_obj_data *intern;

	if (ZEND_NUM_ARGS() != 0) {
		WRONG_PARAM_COUNT;
	}
	
	intern = Z_COMPRESSOR_OBJ_P(getThis());
	
	intern->flushMode = Z_FINISH;
}
/* }}} */

/* {{{ proto string getOutputSize()
   flushes the output and returns its size */
PHP_METHOD(Compressor, getOutputSize) {
	compressor_obj_data *intern;

	if (ZEND_NUM_ARGS() != 0) {
		WRONG_PARAM_COUNT;
	}
	
	intern = Z_COMPRESSOR_OBJ_P(getThis());
	
	consume_input(intern, &(intern->stream), intern->flushMode);
	
	RETURN_LONG(output_size(intern));
}
/* }}} */

/* {{{ proto string getFormat()
   returns the format passed when constructing the object */
PHP_METHOD(Compressor, getFormat) {
	compressor_obj_data *intern;

	if (ZEND_NUM_ARGS() != 0) {
		WRONG_PARAM_COUNT;
	}
	
	intern = Z_COMPRESSOR_OBJ_P(getThis());
	
	RETURN_LONG(intern->format);
}
/* }}} */

/* {{{ proto string getOutput()
   returns the output as a string and empties buffers */
PHP_METHOD(Compressor, getOutput) {
	compressor_obj_data *intern;
	compressor_buffer *buffer;
	uLong size;
	char *pos;
	zend_string *retstr;

	if (ZEND_NUM_ARGS() != 0) {
		WRONG_PARAM_COUNT;
	}
	
	intern = Z_COMPRESSOR_OBJ_P(getThis());
	
	consume_input(intern, &(intern->stream), intern->flushMode);
	
	size = output_size(intern);
	retstr = zend_string_alloc(size, 0);
	pos = retstr->val;
	
	buffer = intern->head;
	
	while (buffer != NULL && buffer->used != 0) {
		memcpy(pos, &buffer->data, buffer->used);
		
		pos += buffer->used;
		buffer = buffer->next;
	}
	
	buffer = NULL;
	
	empty_buffers(intern);
	
	RETURN_STR(retstr);
}
/* }}} */

/* {{{ proto string printOutput()
   returns the output as a string and empties buffers */
PHP_METHOD(Compressor, printOutput) {
	compressor_obj_data *intern;
	compressor_buffer *buffer;

	if (ZEND_NUM_ARGS() != 0) {
		WRONG_PARAM_COUNT;
	}
	
	intern = Z_COMPRESSOR_OBJ_P(getThis());
	
	consume_input(intern, &(intern->stream), intern->flushMode);
	
	buffer = intern->head;
	uLong printed = 0;
	
	while (buffer != NULL && buffer->used != 0) {
		PHPWRITE((const char *) &buffer->data, buffer->used);
		printed += buffer->used;
		
		buffer = buffer->next;
	}
	
	buffer = NULL;

#ifdef PPLIB_DEBUG
	zend_error(E_USER_NOTICE, "Printed %lu bytes", printed);
#endif
	
	empty_buffers(intern);
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(pplib) {
	zend_class_entry ce;
	INIT_NS_CLASS_ENTRY(ce, PPLIB_NAMESPACE, "Compressor", compressor_methods);
	ce.create_object = compressor_create_object_handler;
	
	compressor_entry = zend_register_internal_class(&ce TSRMLS_CC);
	
	zend_declare_class_constant_long(compressor_entry, "FORMAT_ZLIB", sizeof("FORMAT_ZLIB") - 1, PPLIB_FORMAT_ZLIB TSRMLS_CC);
	zend_declare_class_constant_long(compressor_entry, "FORMAT_DEFLATE", sizeof("FORMAT_DEFLATE") - 1, PPLIB_FORMAT_DEFLATE TSRMLS_CC);
	zend_declare_class_constant_long(compressor_entry, "FORMAT_GZIP", sizeof("FORMAT_GZIP") - 1, PPLIB_FORMAT_GZIP TSRMLS_CC);
	
	memcpy(&compressor_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(pplib) {
	return SUCCESS;
}
/* }}} */


/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(pplib) {
	php_info_print_table_start();
	php_info_print_table_header(2, "PPLib support", "enabled");
	php_info_print_table_header(2, "PPLib version", PHP_PPLIB_VERSION);
	php_info_print_table_header(2, "Linked zlib version", ZLIB_VERSION);
	php_info_print_table_end();
}
/* }}} */

/* {{{ pplib_functions[]
 *
 * Every user visible function must have an entry in pplib_functions[].
 */
const zend_function_entry pplib_functions[] = {
	PHP_FE_END	/* Must be the last line in pplib_functions[] */
};
/* }}} */

/* {{{ pplib_module_entry
 */
zend_module_entry pplib_module_entry = {
	STANDARD_MODULE_HEADER,
	"pplib",
	pplib_functions,
	PHP_MINIT(pplib),
	PHP_MSHUTDOWN(pplib),
	NULL,
	NULL,
	PHP_MINFO(pplib),
	PHP_PPLIB_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_PPLIB
ZEND_GET_MODULE(pplib)
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
