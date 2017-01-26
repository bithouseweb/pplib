<?php
	declare(strict_types=1);

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

	namespace PPLib;

	/**
	 * This class provides a mechanism to allow compressed response in chunked encoding;
	 * because of the way it's flushed, the browser is able to decompress part of the
	 * document without waiting for the whole thing
	 *
	 * @author  Andrea <andrea@bhweb.it>
	 */
	class ChunkedCompressedResponse {
		private $compressor;
		private $documentEnd;
		private $determineDone;
		private $buffer;

		/**
		 * Mime in this list gets compressed; the match is performed as a search
		 * so "text/" matches all "text/*"
		 * @var array
		 */
		static $compressibleMime = [
			'text/',
			'application/json',
			'application/xml',
			'application/rss+xml',
			'application/xrds+xml',
			'application/javascript',
			'image/svg+xml',
			'application/octet-stream',
		];

		/**
		 * Content negotiation is done at construction time
		 */
		public function __construct() {
			$this->compressor = null;

			if(isset($_SERVER['TERM'])) {
				$this->determineDone = true;
				return;
			}

			$this->documentEnd = false;
			$this->determineDone = false;

			ini_set('zlib.output_compression', 'Off');
			ini_set('zlib.output_handler', '');
			ini_set('output_buffering', 'Off');
			ini_set('implicit_flush', 'Off');

			$this->start();

			register_shutdown_function([$this, 'end_flush']);
		}

		/**
		 * Allows manually stopping the compression
		 *
		 * @return boolean true if it could stop; false if the first chunk has
		 *                      been already sent by
		 */
		public function stop() : bool {
			if(!$this->determineDone) {
				$this->compressor = null;
				$this->determineDone = true;

				if(ob_get_level() !== 0) {
					ob_end_flush();
				}

				return true;
			}

			return $this->compressor === null;
		}

		/**
		 * Calls the compression determination and eventually stops output
		 * buffering
		 *
		 * @return void
		 */
		protected function startupCompression() : void {
			$this->determineCompression();

			if($this->compressor === null && ob_get_level() !== 0) {
				ob_end_flush();
			}

			$this->determineDone = true;
		}

		/**
		 * Determine which compression to use based on Accept-Encoding header
		 */
		protected function determineCompression() : void {
			$headers = headers_list();

			// Checks headers to see if someone is already doing compression
			// or sending files or sending incompressible data
			foreach ($headers as $header) {
				if(stripos($header, 'Content-Type:') === 0) {
					$compressible = false;
					foreach (self::$compressibleMime as $mime) {
						if(strpos($header, $mime, 14) !== false) {
							$compressible = true;
							break;
						}
					}

					if(!$compressible) {
						return;
					}
				}
				else if(stripos($header, 'Content-Length:') === 0 || stripos($header, 'X-Sendfile:')) {
					return;
				}
				else if(preg_match('/^Content-Encoding: +(.+)$/i', $header, $matches)) {
					if($matches[1] !== 'identity') {
						return;
					}
				}
			}

			// Compression negotiation
			if($this->compressor === null && isset($_SERVER['HTTP_ACCEPT_ENCODING'])) {
				if(stripos($_SERVER['HTTP_ACCEPT_ENCODING'], 'gzip') !== false) {
					header('Content-Encoding: gzip', true);
					$this->compressor = deflate_init(ZLIB_ENCODING_GZIP);
				}
				else if(stripos($_SERVER['HTTP_ACCEPT_ENCODING'], 'deflate') !== false) {
					header('Content-Encoding: deflate', true);
					$this->compressor = deflate_init(ZLIB_ENCODING_DEFLATE);
				}
			}

			if(!isset($_SERVER['HTTP_USER_AGENT']) || strpos($_SERVER['HTTP_USER_AGENT'], 'MSIE') === false || isset($this->compressor)) {
				header('Vary: Accept-Encoding');
			}
		}

		/**
		 * Starts the buffering that allows this to work
		 */
		protected function start() : void {
			$this->buffer = '';
			ob_start([$this, 'ob_callback']);
		}

		/**
		 * Callback for Output Buffering, will receive all the contents from
		 * the buffering of the page; will return an empty string because
		 * the flushing is done in the flush method
		 *
		 * @param  string $buffer the content of the page
		 * @return string
		 */
		protected function ob_callback(string $buffer) : string {
			if(isset($this->compressor)) {
				$this->buffer .= deflate_add($this->compressor, $buffer, ZLIB_NO_FLUSH);

				return '';
			}

			return $buffer;
		}

		/**
		 * Flushes collected output to browser
		 */
		protected function flush() : void {
			if(ob_get_level() !== 0) {
				ob_end_flush();
			}

			if($this->documentEnd) {
				$this->buffer .= deflate_add($this->compressor, "", ZLIB_FINISH);
			}
			else {
				$this->buffer .= deflate_add($this->compressor, "", ZLIB_SYNC_FLUSH);
			}

			echo $this->buffer;
			flush();

			$this->buffer = '';
		}

		/**
		 * Call this function to flush a chunk
		 */
		public function flush_chunk() : void {
			if(!$this->determineDone) {
				$this->startupCompression();
			}

			if(isset($this->compressor)) {
				$this->flush();
				$this->start();
			}
			else {
				flush();
			}
		}

		/**
		 * This is called on shutdown to push remaining of the document
		 * to the browser, or you can call it manually
		 */
		public function end_flush() : void {
			if(!$this->determineDone) {
				$this->startupCompression();
			}

			if(isset($this->compressor) && $this->documentEnd === false) {
				$this->documentEnd = true;

				$this->flush();

				if($this->compressor) {
					unset($this->compressor);
				}
			}
		}
	}
