#!/usr/bin/env python3
"""
OCR Service Python Client Example
"""

import socket
import json
import base64
import struct
import sys

class OcrClient:
    MAGIC_REQUEST = 0x4F435251
    MAGIC_RESPONSE = 0x4F435253

    def __init__(self, host='127.0.0.1', port=8080):
        self.host = host
        self.port = port
        self.sock = None

    def connect(self):
        """Connect to OCR service"""
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((self.host, self.port))
        print(f"Connected to {self.host}:{self.port}")

    def disconnect(self):
        """Disconnect from OCR service"""
        if self.sock:
            self.sock.close()
            self.sock = None
            print("Disconnected")

    def ocr(self, image_path, params=None):
        """
        Send OCR request

        Args:
            image_path: Path to image file
            params: Optional OCR parameters

        Returns:
            OCR result dictionary
        """
        # Read image
        with open(image_path, 'rb') as f:
            image_data = base64.b64encode(f.read()).decode()

        # Build request
        request = {
            'id': 1,
            'method': 'ocr',
            'params': params or {},
            'image_data': image_data
        }

        return self._send_request(request)

    def status(self):
        """Get service status"""
        request = {
            'id': 2,
            'method': 'status',
            'params': {},
            'image_data': ''
        }
        return self._send_request(request)

    def reload(self):
        """Reload models"""
        request = {
            'id': 3,
            'method': 'reload',
            'params': {},
            'image_data': ''
        }
        return self._send_request(request)

    def _send_request(self, request):
        """Send request and receive response"""
        # Encode request
        json_data = json.dumps(request).encode()
        header = struct.pack('II', self.MAGIC_REQUEST, len(json_data))

        # Send request
        self.sock.sendall(header + json_data)

        # Receive response header
        header = self._recv_all(8)
        magic, length = struct.unpack('II', header)

        if magic != self.MAGIC_RESPONSE:
            raise Exception(f'Invalid response magic: 0x{magic:08X}')

        # Receive response body
        data = self._recv_all(length)
        response = json.loads(data)

        return response

    def _recv_all(self, length):
        """Receive exactly length bytes"""
        data = b''
        while len(data) < length:
            chunk = self.sock.recv(length - len(data))
            if not chunk:
                raise Exception('Connection closed')
            data += chunk
        return data


def main():
    if len(sys.argv) < 2:
        print("Usage: python client_example.py <image_path> [host] [port]")
        sys.exit(1)

    image_path = sys.argv[1]
    host = sys.argv[2] if len(sys.argv) > 2 else '127.0.0.1'
    port = int(sys.argv[3]) if len(sys.argv) > 3 else 8080

    # Create client
    client = OcrClient(host, port)
    client.connect()

    try:
        # Send OCR request
        print(f"\nSending OCR request for: {image_path}")
        result = client.ocr(image_path, {
            'use_doc_orientation_classify': True,
            'use_doc_unwarping': False,
            'use_textline_orientation': False
        })

        print("\nOCR Result:")
        print(json.dumps(result, indent=2, ensure_ascii=False))

        # Get status
        print("\nService Status:")
        status = client.status()
        print(json.dumps(status, indent=2, ensure_ascii=False))

    finally:
        client.disconnect()


if __name__ == '__main__':
    main()
