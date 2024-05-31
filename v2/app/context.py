import os
import json
import asyncio

from .settings import Config


class Context:
    class Serial:
        def __init__(self):
            self.writer = None
            self.reader = None
            self.tx     = asyncio.Queue()
            self.rx     = asyncio.Queue()

        async def request(self, msg):
            # NOTE: this isn't very thread safe.
            # but, maybe it will be good enough.
            await self.tx.put(msg)
            return await self.rx.get()

    def __init__(self):
        self.config           = Config()
        self.websockets       = {}
        self.serial           = Context.Serial()
        self.profiles         = self.init_profiles()
        self.state            = {
            'oven_on': False,
            'learning_max_ramp': False,
            'temp_data': [],
        }

    def init_profiles(self):
        if os.path.isfile(self.config.profiles_fn):
            with open(self.config.profiles_fn) as json_data:
                try:
                    return json.load(json_data)
                except ValueError:
                    pass
        return {}
