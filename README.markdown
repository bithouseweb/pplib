PPLib
=====

Provides `PPLib\Compressor` class which allows compression of data and 
intermediate flushes. At each intermediate flush, the receiver can still decode
data and have a partial document. This isn't possible using the existing zlib
extension since the default zlib.output_compression buffers the entire webpage
and outputs everyting in one go and, if you're willing to implement that using
zlib streams, you cannot create a read/write stream.

This is especially useful to allow a chunked encoding, useful if you want to
flush part of your content early, while still compressing output. The full
solution for this is implemented with the C extension and the PHP class
`PPLib\ChunkedCompressedResponse` (included in the repository).

This package is in its' first iteration. Our goal is to extensively test it
on [PensieriParole](www.pensieriparole.it), our main website, which handles
300k pages each day. Preliminary testing shows that, by using this solution,
we can save ~10ms on TTFB.


Installation
------------

The first thing to do is compile and install the extension:

    $ git clone https://github.com/bithouseweb/pplib.git pplib
    $ cd pplib
    $ phpize
    $ ./configure --enable-pplib
    $ make
    $ make install

Then restart your webserver / php-fpm and check `phpinfo()` to see the
extension is loaded. At this point, require the PHP class, and use it like
this:

```php
require(PATH_TO_CLASS . 'ChunkedCompressedResponse.class.php');

$resp = new ChunkedCompressedResponse();

// ... YOUR HEAD CODE ...

$resp->flush_chunk();

// ... YOUR CONTENT CODE ...

$resp->flush_chunk();

// ... YOUR FOOTER CODE ...

$resp->end_flush(); // optional

````


LICENSE
-------

The MIT License (MIT)

Copyright (c) 2015 bitHOUSEweb S.r.l.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
