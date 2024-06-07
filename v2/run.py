import app

import argparse
import webbrowser
from aiohttp import web

HOST = "127.0.0.1"
PORT = 1312

parser = argparse.ArgumentParser()
parser.add_argument("-d", "--daemon", help="run in background as daemon",  action=argparse.BooleanOptionalAction)
parser.add_argument("-o", "--open-browser-tab", help="open site in browser on startup",  action=argparse.BooleanOptionalAction)
parser.add_argument("-c", "--close-on-zero-clients", help="close server on zero websocket clients",  action=argparse.BooleanOptionalAction)

if __name__ == '__main__':
  args = parser.parse_args()
  if args.open_browser_tab:
    webbrowser.open_new_tab(f'{HOST}:{PORT}')
  web.run_app(app.init(close_on_zero_clients=args.close_on_zero_clients), host=HOST, port=PORT)
