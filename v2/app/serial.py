import json
import asyncio

from serial import SerialException
from serial_asyncio import open_serial_connection

from .types import make_connection_response


async def connect(app, port=None, reconnect=False):
    serial  = app['ctx'].serial
    port    = serial.port if port is None else port

    if reconnect:
        serial.connected = False
        print("serial: disconnected...")
        await app['ctx'].broadcasts.put(json.dumps(
            make_connection_response(app)
        ))

    # connect with retries
    while not serial.connected:
        try:
            reader, writer = await open_serial_connection(
                url=port,
                baudrate=app['ctx'].config.serial.baud,
            )
            serial.port      = port
            serial.connected = True
            serial.writer    = writer
            serial.reader    = reader
        except SerialException as e:
            print("serial: attempting to reconnect...")
            await asyncio.sleep(0.5)

    if reconnect:
        await app['ctx'].broadcasts.put(json.dumps(
            make_connection_response(app)
        ))
    print("serial: connected")


async def read_handler(app):
    while True:
        while not app['ctx'].serial.connected:
            await asyncio.sleep(0.5)

        retry = True
        while retry:
            reader = app['ctx'].serial.reader
            try:
                line = await reader.readline()
                retry = False
            except SerialException:
                await connect(app, reconnect=True)
            except ValueError:
                # not really sure what to do about this.
                pass

        try:
            await app['ctx'].serial.rx.put(str(line, 'utf-8'))
        except UnicodeDecodeError:
            # similarly, not totally sure how to handle this
            pass


async def write_handler(app):
    while True:
        while not app['ctx'].serial.connected:
            await asyncio.sleep(0.5)

        cmd = await app['ctx'].serial.tx.get()
        writer = app['ctx'].serial.writer

        retry = True
        while retry:
            try:
                writer.write(f'{cmd}\n'.encode())
                await writer.drain()
                retry = False
            except SerialException:
                await connect(app, reconnect=True)
