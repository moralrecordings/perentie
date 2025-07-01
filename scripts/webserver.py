#!/usr/bin/env python3
# Source: https://gist.github.com/pezcode/d51926bdbadcbd4f22f5a5d2fb8e0394
# Based on:
# https://stackoverflow.com/a/21957017
# https://gist.github.com/HaiyangXu/ec88cbdce3cdbac7b8d5

from http.server import SimpleHTTPRequestHandler, HTTPServer
import argparse
import socketserver
import sys
import ssl
import pathlib


class Handler(SimpleHTTPRequestHandler):
    extensions_map = {
        '': 'application/octet-stream',
        '.css':	'text/css',
        '.html': 'text/html',
        '.jpg': 'image/jpg',
        '.js':	'application/x-javascript',
        '.json': 'application/json',
        '.manifest': 'text/cache-manifest',
        '.png': 'image/png',
        '.wasm':	'application/wasm',
        '.xml': 'application/xml',
    }

    def end_headers(self):
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        SimpleHTTPRequestHandler.end_headers(self)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(formatter_class=argparse.RawDescriptionHelpFormatter, description="Simple web server for serving SDL3 Emscripten builds with the SharedMemoryBuffer headers", epilog="Mobile Safari won't work unless the server is HTTPS. Generate a self-signed cert with:\n    openssl req -x509 -newkey rsa:4096 -out cert.pem -sha256 -days 3650 -nodes -subj \"/C=XX/ST=StateName/L=CityName/O=CompanyName/OU=CompanySectionName/CN=CommonNameOrHostname\"")
    parser.add_argument("--port", type=int, help="Port to use, defaults to 8000 for HTTP and 8443 for HTTPS")
    parser.add_argument("--cert", type=pathlib.Path, help="Self-signed certificate.")
    args = parser.parse_args()

    port = args.port if args.port else (8443 if args.cert else 8000)
    with socketserver.TCPServer(("0.0.0.0", port), Handler) as httpd:
        if args.cert:
            sslc = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            sslc.load_cert_chain(str(args.cert), str(args.cert))
            with sslc.wrap_socket(httpd.socket, server_side=True) as sock:
                httpd.socket = sock
                print(f"Emscripten port served at https://localhost:{port}/perentie.html")
                httpd.serve_forever()
        else:
            print(f"Emscripten port served at http://localhost:{port}/perentie.html")
            httpd.serve_forever()
