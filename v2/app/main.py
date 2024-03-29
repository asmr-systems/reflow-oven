import os
import json
import uuid
from typing import List, Dict
import asyncio
import webbrowser
import aiohttp
from aiohttp import web
from aiohttp.web_runner import GracefulExit
from serial import SerialException
from serial_asyncio import open_serial_connection

# TODO:
# [ ] add proper logging
# [x] refactor start/stop command response to just be a status response that includes:
#     {'type': 'status', data: {'connected': true, 'running': true}}
# [x] implement retry logic for if reflow oven is disconnected
#     main reconnect cases to handle:
#       a. disconnect and reconnect when not running, no read/write
#       b. disconnect on read while not running
#       c. disconnect on write while not running
#       d. disconnect when running, no read/write
#       e. disconnect when running, on read
#       f. disconnect on running, on write
#     when this happens, the app should know it is disconnected.
#     when a reconnection happens, everything should be okay.
#     OK: everything seems to work pretty well for reconnect. though we need the app and gui to respond correctly to disconnects.
# [x] when a disconnection happens, send a status messsage to the gui
# [ ] add way to select connected usb device
# [ ] handle the weird case when we open multiple tabs or refresh, the GracefulExit exception bubbles up and a stacktrace is printed on exit.
# [x] erase job data on plot (js) when restarting a job
# [ ] make gui buttons disabled when not connected
# [ ] ENSURE ON CLOSE LOGIC ACTUALLY STOPS OVEN

HOST = "127.0.0.1"
PORT = 1312
SERIAL_PORT = "/dev/ttyUSB0" # "/dev/ttyACM0"
BAUD=9600
PROFILES_FILE = 'profiles.json'

websockets      = web.AppKey("websockets", Dict)
pending_cmds    = web.AppKey("pending_cmds", asyncio.Queue)
profiles        = web.AppKey("profiles", Dict)
serial_conn     = web.AppKey("serial_conn", Dict)
app_state       = web.AppKey("state", Dict)

async def http_handler(request):
  return web.FileResponse("./index.html")

async def profile_handler(request):
  reader = await request.multipart()
  field = await reader.next()

  if field.name == 'file':
    new_profiles = json.loads(await field.read_chunk())
    request.app[profiles].update(new_profiles)
    profile_json = json.dumps(request.app[profiles])
    with open(PROFILES_FILE, 'w') as profile_fd:
      profile_fd.write(profile_json)

    return web.json_response({'message': 'profile load successful'})
  return web.json_response({'error': 'No file provided'})

async def websocket_handler(request):
  ws = web.WebSocketResponse()
  id = uuid.uuid1()
  request.app[websockets][id] = ws
  await ws.prepare(request)

  async for msg in ws:
    if msg.type == aiohttp.WSMsgType.TEXT:
      r = json.loads(msg.data)
      print(r)
      action = r["action"]
      if action == "get":
        if r["type"] == "status":
          await request.app[pending_cmds].put("status")
        elif r["type"] == "profiles":
          await ws.send_str(json.dumps({
            "action": action,
            "type": "profiles",
            "data": request.app[profiles]
          }))
        else:
          print(f'cannot get {r["type"]}')
      elif action == "set":
        pass
      elif action == "start":
        await request.app[pending_cmds].put("start")
      elif action == "pause":
        pass
      elif action == "stop":
        await request.app[pending_cmds].put("stop")
      elif action == "learn":
        await request.app[pending_cmds].put("learn")
      elif action == "test":
        await request.app[pending_cmds].put("test")
      else:
        print(f'cannot perform {action}')

    elif msg.type == aiohttp.WSMsgType.ERROR:
      print('ws connection closed with exception %s' %
            ws.exception())

  # maybe quit
  del request.app[websockets][id]
  await ws.close()
  await asyncio.sleep(2)
  if len(request.app[websockets]) == 0:
    print('websocket connection closed, shutting down.')
    if request.app[serial_conn]["writer"] != None and request.app[app_state]['oven_on']:

      writer_disconnected = True
      while writer_disconnected:
        try:
          request.app[serial_conn]["writer"].write("stop".encode())
          await request.app[serial_conn]["writer"].drain()
          writer_disconnected = False
        except SerialException:
          await connect_serial(app)

      while request.app[app_state]['oven_on']:
        await asyncio.sleep(0.02)
    raise GracefulExit()

  return ws

async def broadcast_to_websockets(app, msg):
  for ws in app[websockets].values():
    await ws.send_str(msg)

async def handle_serial_write(app, writer):
  while True:
    cmd = await app[pending_cmds].get()
    writer = app[serial_conn]["writer"]
    try:
      writer.write(cmd.encode())
      await writer.drain()
      print(f'wrote: {cmd}')
    except SerialException:
      await connect_serial(app)

async def handle_serial_read(app, reader):
  while True:
    not_connected = True
    while not_connected:
      reader = app[serial_conn]["reader"]
      try:
        line = await reader.readline()
        not_connected = False
      except SerialException:
        await connect_serial(app)

    # TODO i don't think we need to track status on backend...
    # print(line)
    # resp = json.loads(line)
    # if resp["action"] == "start":
    #   app[app_state]['oven_on'] = True if resp["data"] == "ok" else False
    # elif resp["command"] == "stop":
    #   app[app_state]['oven_on'] = False if resp["data"] == "ok" else True
    await broadcast_to_websockets(app, str(line, 'utf-8'))

async def init_context(app):
  app[pending_cmds] = asyncio.Queue()
  if os.path.isfile(PROFILES_FILE):
    with open('profiles.json') as json_data:
      try:
        app[profiles] = json.load(json_data)
      except ValueError:
        app[profiles] = {}
  else:
    app[profiles] = {}


async def connect_serial(app, initial_startup=False):
  # TODO add retry logic here
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
  while not_connected:
    try:
      reader, writer = await open_serial_connection(url=SERIAL_PORT, baudrate=BAUD)
      not_connected = False
    except SerialException:
      # try to connect every 0.5 seconds
      print("serial: attempting to reconnect...")
      await asyncio.sleep(0.5)

  print("serial: connected")
  app[serial_conn]["writer"] = writer
  app[serial_conn]["reader"] = reader
  # query mcu for status
  await app[pending_cmds].put("status")
  return reader, writer

async def serial_handler(app):
  reader, writer = await connect_serial(app, initial_startup=True)
  read_task = asyncio.create_task(handle_serial_write(app, writer))
  write_task = asyncio.create_task(handle_serial_read(app, reader))

async def on_shutdown(app):
    for ws in set(app[websockets].values()):
        await ws.close(code=WSCloseCode.GOING_AWAY, message="Server shutdown")

def init():
  app = web.Application()
  app[websockets] = {}
  app[serial_conn] = {
    'writer': None,
    'reader': None,
  }
  app[app_state] = {
    'oven_on': False,
  }
  app.on_startup.append(init_context)
  app.on_startup.append(serial_handler)
  app.on_shutdown.append(on_shutdown)
  app.add_routes([web.get('/', http_handler),
                  web.post('/profile', profile_handler),
                  web.get('/ws', websocket_handler)])
  return app

if __name__ == '__main__':
  webbrowser.open_new_tab(f'{HOST}:{PORT}')
  web.run_app(init(), host=HOST, port=PORT)
