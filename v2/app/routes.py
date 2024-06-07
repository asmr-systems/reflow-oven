import asyncio
import uuid
import json
from json import JSONDecodeError
import aiohttp
from aiohttp import web
from aiohttp.web_runner import GracefulExit

from app import api
from .types import ControlMode

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

# TODO: i think status should return an amalgamation of oven status,
# job status, connection details, current temperature, and more
@routes.get('/status')
async def get_status(request):
    data, status = await api.get_status(request.app)
    return web.Response(text=json.dumps(data), status=status)


@routes.get('/devices')
async def get_devices(request):
    return web.Response(text=json.dumps(api.get_usb_serial_devices()))


@routes.get('/connect')
async def get_connection(request):
    port = None
    if 'port' in request.rel_url.query:
        port = request.rel_url.query['port']
    return web.Response(text=json.dumps(await api.connect(request.app, port=port)))


@routes.get('/job')
async def get_job(request):
    return web.Response(text=json.dumps(api.get_job_status(request.app)))


@routes.get('/temperature')
async def get_temperature(request):
    return web.Response(text=json.dumps(await api.get_temperature(request.app)))


@routes.get('/data')
async def get_data(request):
    downsample = False
    if 'downsample' in request.rel_url.query:
        downsample = request.rel_url.query['downsample']
    return web.Response(text=json.dumps(await api.get_recorded_data(request.app, downsample=downsample)))


@routes.put('/job')
async def set_job(request):
    job_info = request.rel_url.query
    return web.Response(text=json.dumps(await api.set_job(request.app, job_info)))


@routes.put('/start')
async def set_job_start(request):
    return web.Response(text=json.dumps(await api.start_job(request.app)))


@routes.put('/stop')
async def set_job_stop(request):
    return web.Response(text=json.dumps(await api.stop_job(request.app)))


@routes.put('/temperature')
async def set_temperature(request):
    mode = ControlMode.Point
    if 'mode' in request.rel_url.query:
        mode = request.rel_url.query['mode']
    value = 25
    if 'value' in request.rel_url.query:
        value = request.rel_url.query['value']
    return web.Response(text=json.dumps(await api.set_temperature(request.app, mode, value)))


@routes.put('/enable')
async def enable_oven(request):
    return web.Response(text=json.dumps(await api.enable(request.app)))


@routes.put('/disable')
async def disable_oven(request):
    return web.Response(text=json.dumps(await api.disable(request.app)))


@routes.get('/ws')
async def websocket_handler(request):
    ws = web.WebSocketResponse()
    id = uuid.uuid1()
    request.app['ctx'].websockets[id] = ws
    await ws.prepare(request)

    async for msg in ws:
        if msg.type == aiohttp.WSMsgType.TEXT:
            r = json.loads(msg.data)

            action = r["action"]
            if action == "get":
                if r["type"] == "status":
                    await ws.send_str(json.dumps(
                        await api.get_status(request.app)
                    ))
                elif r["type"] == "profiles":
                    await ws.send_str(json.dumps({
                        "action": action,
                        "type": "profiles",
                        "data": request.app['ctx'].profiles
                    }))
                else:
                    print(f'cannot get {r["type"]}')
            else:
                print(f'cannot perform {action}')

        elif msg.type == aiohttp.WSMsgType.ERROR:
            print('ws connection closed with exception %s' %
                  ws.exception())

    # maybe quit
    del request.app['ctx'].websockets[id]
    await ws.close()
    await asyncio.sleep(2)
    if len(request.app['ctx'].websockets) == 0 and request.app['ctx'].close_on_zero_clients:
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
