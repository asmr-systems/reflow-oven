import app

import webbrowser
from aiohttp import web

HOST = "127.0.0.1"
PORT = 1312

if __name__ == '__main__':
  webbrowser.open_new_tab(f'{HOST}:{PORT}')
  web.run_app(app.init(), host=HOST, port=PORT)
