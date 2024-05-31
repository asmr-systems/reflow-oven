import asyncio

from serial import SerialException
from serial_asyncio import open_serial_connection



async def startup(app):
    await connect(app, initial_startup=True)
    asyncio.create_task(read_handler(app))
    asyncio.create_task(write_handler(app))


async def connect(app, initial_startup=False):
    not_connected = True
    reader = None
    writer = None
    if not initial_startup:
        print("serial: disconnected...")
        await broadcast_to_websockets(app, json.dumps({
            "action": "get",
            "type": "status",
            "data": {"connected": False, "running": False}
        }))

    # connect with retries
    while not_connected:
        try:
            serial_config = app['ctx'].config.serial
            reader, writer = await open_serial_connection(
                url=serial_config.port,
                baudrate=serial_config.baud
            )
            not_connected = False
        except SerialException:
            # try to connect every 0.5 seconds
            print("serial: attempting to reconnect...")
            await asyncio.sleep(0.5)

    print("serial: connected")

    app['ctx'].serial.writer = writer
    app['ctx'].serial.reader = reader


async def read_handler(app):
    while True:
        not_connected = True
        while not_connected:
            reader = app['ctx'].serial.reader
            try:
                line = await reader.readline()
                not_connected = False
            except SerialException:
                await connect_serial(app)
            except ValueError:
                # not really sure what to do about this.
                pass
        try:
            msg = str(line, 'utf-8')
            # await process_rx(app, msg)
            await broadcast_to_websockets(app, msg) # TODO eventually get rid of this direct access to websockets..
            await app['ctx'].serial.rx.put(msg)
        except UnicodeDecodeError:
            # similarly, not totally sure how to handle this
            pass


async def write_handler(app):
    while True:
        cmd = await app['ctx'].serial.tx.get()
        writer = app['ctx'].serial.writer
        try:
            writer.write(cmd.encode())
            await writer.drain()
            # print(f'wrote: {cmd.encode()}')
        except SerialException:
            await connect_serial(app)


async def process_rx(app, msg):
  print(msg)
  try:
    d = json.loads(msg)
  except JSONDecodeError:
    # just skip for now
    return

  # change tracked status
  if d["type"] == "status":
    if d["data"]["learning"]:
      if d["data"]["phase"] == "max-ramp":
        app['state']['learning_max_ramp'] = True
    else:
      app['state']['learning_max_ramp'] = False

  if d["type"] == "temp":
    if app['state']['learning_max_ramp']:
      app['state']['temp_data'].append(d["data"])


async def broadcast_to_websockets(app, msg):
  for ws in app['ctx'].websockets.values():
    await ws.send_str(msg)
