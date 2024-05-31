import asyncio
import uuid
import json
from json import JSONDecodeError
import aiohttp
from aiohttp import web
from aiohttp.web_runner import GracefulExit

from app import api

routes = web.RouteTableDef()

@routes.get('/')
async def http_handler(request):
    return web.FileResponse(path=str(request.app['ctx'].config.static_dir / 'index.html'))

@routes.get('/profile')
async def profile_handler(request):
  reader = await request.multipart()
  field = await reader.next()

  if field.name == 'file':
    new_profiles = json.loads(await field.read_chunk())
    request.app[profiles].update(new_profiles)
    profile_json = json.dumps(request.app[profiles])
    with open(request.app['PROFILES_FILE'], 'w') as profile_fd:
      profile_fd.write(profile_json)

    return web.json_response({'message': 'profile load successful'})
  return web.json_response({'error': 'No file provided'})

@routes.get('/status')
async def get_status(request):
    return web.Response(text=await api.get_status(request.app))


@routes.get('/ws')
async def websocket_handler(request):
  print("HANDLING WEBSOCKET REQUEST")
  ws = web.WebSocketResponse()
  id = uuid.uuid1()
  request.app['ctx'].websockets[id] = ws
  await ws.prepare(request)

  async for msg in ws:
    if msg.type == aiohttp.WSMsgType.TEXT:
      r = json.loads(msg.data)
      print(r)
      action = r["action"]
      if action == "get":
        if r["type"] == "status":
          await request.app['ctx'].serial.tx.put("\x02A")
        elif r["type"] == "profiles":
          await ws.send_str(json.dumps({
            "action": action,
            "type": "profiles",
            "data": request.app['ctx'].profiles
          }))
        else:
          print(f'cannot get {r["type"]}')
      elif action == "set":
        pass
      elif action == "start":
        await request.app['ctx'].serial.tx.put("start")
      elif action == "pause":
        pass
      elif action == "stop":
        await request.app['ctx'].serial.tx.put("stop")
      elif action == "learn":
        await request.app['ctx'].serial.tx.put("learn")
      elif action == "test":
        await request.app['ctx'].serial.tx.put("test")
      else:
        print(f'cannot perform {action}')

    elif msg.type == aiohttp.WSMsgType.ERROR:
      print('ws connection closed with exception %s' %
            ws.exception())

  # maybe quit
  del request.app['ctx'].websockets[id]
  await ws.close()
  await asyncio.sleep(2)
  if len(request.app['ctx'].websockets) == 0:
    print('websocket connection closed, shutting down.')
    if request.app['ctx'].serial.writer != None and request.app['ctx'].state['oven_on']:

      writer_disconnected = True
      while writer_disconnected:
        try:
          request.app['ctx'].serial.writer.write("stop".encode())
          request.app['ctx'].serial.writer.drain()
          writer_disconnected = False
        except SerialException:
          await connect_serial(app)

      while request.app['ctx'].state['oven_on']:
        await asyncio.sleep(0.02)
    raise GracefulExit()

  return ws
