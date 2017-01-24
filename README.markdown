PPLib
=====

Provides `PPLib\ChunkedCompressedResponse` class which allows a chuncked
encoding with compression, that allows flushing your content early while
compressing it.
At each intermediate flush, the receiver can still decode the sent data and
have a partial document. This isn't possible using the existing zlib
filters, since the default zlib.output_compression buffers the entire webpage
and outputs everyting in one go. Previously this package used a custom C
extension to create a partially-flushable zlib stream, but with PHP 7 this can
be done with stock PHP.

This package is used on a live website:
[PensieriParole](www.pensieriparole.it), our main website, which handles
300k pages each day. Preliminary testing shows that, by using this solution,
we can save ~10ms on TTFB, this mostly depends how much early you can flush
your head and how much things can be postponed.


Installation
------------

Just require the package using composer:

```bash
composer require bithouseweb/pplib
````

At this point you can use the class like this:

```php
use PPLib\ChunkedCompressedResponse;

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
