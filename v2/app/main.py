import asyncio
import aiohttp

from app import serial
from .routes import routes
from .serial import read_handler, write_handler
from .context import Context, record_timeseries, broadcast, manage_jobs

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


async def on_startup(app):
    asyncio.create_task(read_handler(app))
    asyncio.create_task(write_handler(app))
    asyncio.create_task(broadcast(app))
    asyncio.create_task(record_timeseries(app))
    asyncio.create_task(manage_jobs(app))


# NOTE: this isn't being called when GracefulExit occurs. only ctrl-c quit.
# Idea: instead of raising GracefulExit, send a SIGTERM signal to program?
async def on_shutdown(app):
    for ws in set(app['ctx'].websockets.values()):
        await ws.close(code=aiohttp.WSCloseCode.GOING_AWAY, message="Server shutdown")


def init(close_on_zero_clients=False):
  app = aiohttp.web.Application()
  app['ctx'] = Context(close_on_zero_clients=close_on_zero_clients)

  app.on_startup.append(on_startup)
  app.on_shutdown.append(on_shutdown)
  app.add_routes(routes)

  return app
