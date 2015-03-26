<?php
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
		private $compressor, $documentEnd;
		
		/**
		 * Content negotiation is done at construction time
		 */
		public function __construct() {
			$this->compressor = null;
			$this->documentEnd = false;
			
			ini_set('zlib.output_compression', 'Off');
			ini_set('zlib.output_handler', '');
			ini_set('output_buffering', 'Off');
			ini_set('implicit_flush', 'Off');
			
			$this->determineCompression();
			
			if($this->compressor !== null) {
				register_shutdown_function([$this, 'end_flush']);
				
				$this->start();
			}
		}
		
		/**
		 * Determine which compression to use based on Accept-Encoding header
		 */
		protected function determineCompression() {
			$headers = headers_list();
			
			// Allows setting the compression from outside
			foreach ($headers as $header) {
				if(preg_match('/^Content-Encoding: +(.+)$/i', $header, $matches)) {
					if($matches[1] === 'gzip') {
						$this->compressor = new Compressor(Compressor::FORMAT_GZIP);
					}
					else if($matches[1] === 'deflate') {
						$this->compressor = new Compressor(Compressor::FORMAT_ZLIB);
					}
					break;
				}
			}
			
			// Compression negotiation
			if($this->compressor === null && isset($_SERVER['HTTP_ACCEPT_ENCODING'])) {
				if(stripos($_SERVER['HTTP_ACCEPT_ENCODING'], 'gzip') !== false) {
					header('Content-Encoding: gzip', true);
					$this->compressor = new Compressor(Compressor::FORMAT_GZIP);
				}
				else if(stripos($_SERVER['HTTP_ACCEPT_ENCODING'], 'deflate') !== false) {
					header('Content-Encoding: deflate', true);
					$this->compressor = new Compressor(Compressor::FORMAT_ZLIB);
				}
			}
			
			header('Vary: Accept-Encoding');
		}
		
		/**
		 * Starts the buffering that allows this to work
		 */
		protected function start() {
			ob_start();
		}
		
		/**
		 * Flushes collected output to browser
		 */
		protected function flush() {
			$this->compressor->append(ob_get_contents());
			ob_end_clean();
			
			if($this->documentEnd) {
				$this->compressor->endDocument();
			}
			
			//printf("%x\r\n", $this->compressor->getOutputSize());
			$this->compressor->printOutput();
			//echo "\r\n";
			flush();
		}
		
		/**
		 * Call this function to flush a chunk
		 */
		public function flush_chunk() {
			if($this->compressor !== null) {
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
		public function end_flush() {
			if($this->documentEnd === false) {
				$this->documentEnd = true;
				
				$this->flush();
				
				//echo "0\r\n\r\n";
				
				if($this->compressor) {
					unset($this->compressor);
				}
			}
		}
	}
	
	