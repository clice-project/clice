import os
import json
import asyncio
import logging
from typing import Any, Callable


class LSPTransport:
    def __init__(self, commands: list[str], mode="stdio", host="127.0.0.1", port=2087):
        self.commands = commands
        self.mode = mode
        self.host = host
        self.port = port

        self.process: asyncio.subprocess.Process = None
        self.reader: asyncio.StreamReader = None
        self.writer: asyncio.StreamWriter = None

        self.request_id = 0
        self.pending_requests: dict[int, asyncio.Future] = {}
        self.notification_handlers: dict[str,
                                         Callable[[dict[str, Any]], Any]] = {}
        self.message_queue: asyncio.Queue = asyncio.Queue()

        logging.basicConfig(level=logging.INFO,
                            format='%(asctime)s - %(levelname)s - %(message)s')

    async def start(self):
        if self.mode == "stdio":
            logging.info(f"Starting LSP server via stdio: {self.commands}")
            self.process = await asyncio.create_subprocess_exec(
                *self.commands,
                stdin=asyncio.subprocess.PIPE,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE
            )
            self.reader = self.process.stdout
            self.writer = self.process.stdin
            logging.info("LSP server started via stdio.")
        elif self.mode == "socket":
            logging.info(
                f"Connecting to LSP server via socket: {self.host}:{self.port}")
            # Note: For socket mode, you usually need to start the LSP server externally
            # or have it run as a daemon process already. This client will just connect.
            try:
                self.reader, self.writer = await asyncio.open_connection(self.host, self.port)
                logging.info("Connected to LSP server via socket.")
            except ConnectionRefusedError:
                logging.error(
                    f"Connection refused: No LSP server listening on {self.host}:{self.port}")
                raise
            except Exception as e:
                logging.error(f"Error connecting via socket: {e}")
                raise
        else:
            raise ValueError(
                "Invalid connection mode. Use 'stdio' or 'socket'.")

        asyncio.create_task(self._read_messages())
        asyncio.create_task(self._process_messages())
        if self.process and self.process.stderr:
            asyncio.create_task(self._read_stderr())

    async def stop(self):
        if self.mode == "stdio" and self.process:
            logging.info("Terminating LSP server process.")
            self.process.terminate()
            await self.process.wait()
            logging.info("LSP server process terminated.")
        elif self.mode == "socket" and self.writer:
            logging.info("Closing socket connection to LSP server.")
            self.writer.close()
            await self.writer.wait_closed()
            logging.info("Socket connection closed.")

    async def _read_stderr(self):
        if not self.process or not self.process.stderr:
            return
        while True:
            line = await self.process.stderr.readline()
            if not line:
                break
            logging.error(f"LSP Server STDERR: {line.decode().strip()}")

    async def _parse_header_line(self, header_line: bytes) -> int | None:
        header_line = header_line.strip()
        if not header_line:
            return None

        if header_line.startswith(b'Content-Length:'):
            try:
                return int(header_line.split(b':')[1].strip())
            except ValueError:
                logging.error(
                    f"Invalid Content-Length header: {header_line.decode()}")
                return 0
        if header_line.startswith(b'Content-Type:'):
            return 0

        logging.warning(f"Unknown header: {header_line.decode()}")
        return 0

    async def _read_messages(self):
        content_length = 0
        content_bytes = b''

        try:
            while True:
                if not self.reader:
                    logging.info(
                        "LSP client reader is not available. Exiting _read_messages.")
                    break

                header_line = await self.reader.readline()
                if not header_line:
                    logging.info(
                        "LSP server output stream closed. Exiting _read_messages.")
                    break

                parsed_length = await self._parse_header_line(header_line)

                # Empty line means headers end
                if parsed_length is None:
                    if content_length > 0:
                        content_bytes = await self.reader.readexactly(content_length)
                        message = json.loads(content_bytes.decode('utf-8'))
                        await self.message_queue.put(message)
                        content_length = 0
                    continue

                if parsed_length > 0:
                    content_length = parsed_length

        except asyncio.IncompleteReadError as e:
            logging.error(f"Incomplete message read: {e}")
        except json.JSONDecodeError as e:
            decoded_content_attempt = content_bytes.decode(
                'utf-8', errors='ignore') if content_bytes else 'N/A'
            logging.error(
                f"JSON decode error: {e}, Content (attempted): {decoded_content_attempt}")
        except Exception as e:
            logging.error(f"Error reading messages: {e}")
        finally:
            logging.info("_read_messages task finished.")

    async def _handle_response(self, message: dict[str, Any]):
        request_id = message["id"]
        if request_id not in self.pending_requests:
            logging.warning(
                f"Received unknown request/response ID: {request_id}, message: {message}")
            return

        future = self.pending_requests.pop(request_id)
        if "result" in message:
            future.set_result(message["result"])
        elif "error" in message:
            future.set_exception(Exception(f"LSP Error: {message['error']}"))
        else:
            future.set_exception(
                Exception(f"LSP response missing 'result' or 'error': {message}"))

    async def _handle_notification(self, message: dict[str, Any]):
        method = message["method"]
        if method not in self.notification_handlers:
            logging.warning(
                f"Received unhandled notification: {method}, message: {message}")
            return

        try:
            await self.notification_handlers[method](message.get("params"))
        except Exception as e:
            logging.error(f"Error in notification handler for {method}: {e}")

    async def _process_messages(self):
        message: dict[str, Any] = None

        try:
            while True:
                message = await self.message_queue.get()
                logging.debug(f"Received message: {message}")

                if "id" in message:
                    await self._handle_response(message)
                elif "method" in message:
                    await self._handle_notification(message)
                else:
                    logging.warning(
                        f"Received malformed LSP message: {message}")

        except asyncio.CancelledError:
            logging.info("_process_messages task cancelled.")
        except Exception as e:
            logging.error(
                f"Critical error processing message: {e}, Message: {message}")
        finally:
            logging.info("_process_messages task finished.")

    async def _send_message(self, message: dict[str, Any]):
        if not self.writer:
            logging.error("LSP client writer is not available.")
            return

        encoded_message = json.dumps(
            message, ensure_ascii=False).encode('utf-8')
        content_length = len(encoded_message)

        header = (
            f"Content-Length: {content_length}\r\n"
            f"\r\n"
        ).encode('utf-8')

        try:
            self.writer.write(header)
            self.writer.write(encoded_message)
            await self.writer.drain()
            logging.debug(
                f"Sent message: {message.get('method', 'Unknown Method')} (ID: {message.get('id', 'N/A')})")
        except Exception as e:
            logging.error(f"Error sending message: {e}, message: {message}")

    async def send_request(self, method: str, params: dict[str, Any] | None = None) -> Any:
        self.request_id += 1
        current_id = self.request_id
        message = {
            "jsonrpc": "2.0",
            "id": current_id,
            "method": method,
            "params": params if params is not None else {}
        }
        await self._send_message(message)
        future = asyncio.Future()
        self.pending_requests[current_id] = future
        return await future

    async def send_notification(self, method: str, params: dict[str, Any] | None = None):
        message = {
            "jsonrpc": "2.0",
            "method": method,
            "params": params if params is not None else {}
        }
        await self._send_message(message)

    def register_notification_handler(self, method: str, handler: Callable[[dict[str, Any]], Any]):
        self.notification_handlers[method] = handler
