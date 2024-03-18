from typing import List
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

serial_listener = web.AppKey("serial_listener", asyncio.Task[None])
websockets      = web.AppKey("websockets", List[web.WebSocketResponse])
pending_cmds    = web.AppKey("pending_cmds", List[str])

async def http_handler(request):
  return web.FileResponse("./index.html")

async def websocket_handler(request):
    ws = web.WebSocketResponse()
    request.app[websockets].append(ws)
    await ws.prepare(request)

    async for msg in ws:
      # TODO handle JSON messages
        if msg.type == aiohttp.WSMsgType.TEXT:
            if msg.data == 'close':
                await ws.close()
            elif msg.data == "CMD":
                request.app[pending_cmds].append("CMD")
            else:
                print(msg.data)
                await ws.send_str(msg.data + '/answer')
        elif msg.type == aiohttp.WSMsgType.ERROR:
            print('ws connection closed with exception %s' %
                  ws.exception())

    print('websocket connection closed, shutting down.')
    raise GracefulExit()
    return ws

async def handle_serial_write(app, writer):
  while True:
    await asyncio.sleep(0)
    while len(app[pending_cmds]) > 0:
      cmd = app[pending_cmds].pop(0)
      writer.write(cmd.encode())
      await writer.drain()

async def handle_serial_read(app, reader):
  while True:
    line = await reader.readline()
    print(str(line, 'utf-8'))
    for ws in app[websockets]:
      await ws.send_str(str(line, 'utf-8'))

async def serial_handler(app):
  reader, writer = await open_serial_connection(url=SERIAL_PORT, baudrate=BAUD)
  read_task = asyncio.create_task(handle_serial_write(app, writer))
  write_task = asyncio.create_task(handle_serial_read(app, reader))

def init():
  app = web.Application()
  app[websockets] = []
  app[pending_cmds] = []
  app.on_startup.append(serial_handler)
  app.add_routes([web.get('/', http_handler),
                  web.get('/ws', websocket_handler)])
  return app

if __name__ == '__main__':
  webbrowser.open_new_tab(f'{HOST}:{PORT}')
  web.run_app(init(), host=HOST, port=PORT)
