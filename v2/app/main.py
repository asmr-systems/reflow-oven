import aiohttp

from app import serial_worker
from .settings import config
from .routes import routes
from .context import Context

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


# NOTE: this isn't being called when GracefulExit occurs. only ctrl-c quit.
# Idea: instead of raising GracefulExit, send a SIGTERM signal to program?
async def on_shutdown(app):
    for ws in set(app['ctx'].websockets.values()):
        await ws.close(code=aiohttp.WSCloseCode.GOING_AWAY, message="Server shutdown")


def init():
  app = aiohttp.web.Application()
  app['ctx'] = Context()

  app.on_startup.append(serial_worker.startup)
  app.on_shutdown.append(on_shutdown)
  app.add_routes(routes)

  return app
