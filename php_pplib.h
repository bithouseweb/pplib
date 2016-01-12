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

#ifndef PHP_PPLIB_H
#define PHP_PPLIB_H

#define PPLIB_FORMAT_ZLIB 0
#define PPLIB_FORMAT_DEFLATE 1
#define PPLIB_FORMAT_GZIP 2
 
#define PPLIB_NAMESPACE "PPLib"

extern zend_module_entry pplib_module_entry;
#define phpext_pplib_ptr &pplib_module_entry

extern zend_class_entry *compressor_entry;

PHP_METHOD(Compressor, __construct);
PHP_METHOD(Compressor, append);
PHP_METHOD(Compressor, endDocument);
PHP_METHOD(Compressor, getFormat);
PHP_METHOD(Compressor, getOutputSize);
PHP_METHOD(Compressor, getOutput);
PHP_METHOD(Compressor, printOutput);

#define PHP_PPLIB_VERSION "0.0.2"

#ifdef PHP_WIN32
#	define PHP_PPLIB_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_PPLIB_API __attribute__ ((visibility("default")))
#else
#	define PHP_PPLIB_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

#ifdef ZTS
#define PPLIB_G(v) TSRMG(pplib_globals_id, zend_pplib_globals *, v)
#else
#define PPLIB_G(v) (pplib_globals.v)
#endif
   

#endif	/* PHP_PPLIB_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
