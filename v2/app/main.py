import os
import json
import uuid
from typing import List, Dict
import asyncio
import webbrowser
import aiohttp
from aiohttp import web
from aiohttp.web_runner import GracefulExit
from serial_asyncio import open_serial_connection


HOST = "127.0.0.1"
PORT = 1312
SERIAL_PORT = "/dev/ttyACM0"
BAUD=9600
PROFILES_FILE = 'profiles.json'

websockets      = web.AppKey("websockets", Dict)
pending_cmds    = web.AppKey("pending_cmds", asyncio.Queue)
profiles        = web.AppKey("profiles", Dict)

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
      if r["command"] == "get_profiles":
        profiles_json = json.dumps({
          "command": "profiles",
          "data": request.app[profiles]
        })
        await ws.send_str(profiles_json)
      elif r["command"] == "status":
        await request.app[pending_cmds].put("status")
      elif r["command"] == "start":
        await request.app[pending_cmds].put("start")
      elif r["command"] == "stop":
        await request.app[pending_cmds].put("stop")
      else:
        await request.app[pending_cmds].put(msg.data)
    elif msg.type == aiohttp.WSMsgType.ERROR:
      print('ws connection closed with exception %s' %
            ws.exception())

  # maybe quit
  # await ws.close()
  del request.app[websockets][id]
  await ws.close()
  await asyncio.sleep(2)
  if len(request.app[websockets]) == 0:
    print('websocket connection closed, shutting down.')
    raise GracefulExit()

  return ws

async def handle_serial_write(app, writer):
  while True:
    cmd = await app[pending_cmds].get()
    writer.write(cmd.encode())
    await writer.drain()

async def handle_serial_read(app, reader):
  while True:
    line = await reader.readline()
    for ws in app[websockets].values():
      await ws.send_str(str(line, 'utf-8'))

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

async def serial_handler(app):
  reader, writer = await open_serial_connection(url=SERIAL_PORT, baudrate=BAUD)
  read_task = asyncio.create_task(handle_serial_write(app, writer))
  write_task = asyncio.create_task(handle_serial_read(app, reader))

async def on_shutdown(app):
    for ws in set(app[websockets].values()):
        await ws.close(code=WSCloseCode.GOING_AWAY, message="Server shutdown")

def init():
  app = web.Application()
  app[websockets] = {}
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
