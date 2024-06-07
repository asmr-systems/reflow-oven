import os
import json
import asyncio

from app import types
from .settings import Config
from .types import JobStatus, JobType, TuningPhase, ControlMode, ControlStatus


class Context:
    class Serial:
        def __init__(self):
            self.port      = '/dev/ttyUSB0'
            self.connected = False
            self.writer    = None
            self.reader    = None
            self.tx        = asyncio.Queue()
            self.rx        = asyncio.Queue()
            self.lock      = asyncio.Lock()

        async def request(self, msg):
            # await self.tx.put(msg)
            # await self.rx.get()
            response = None
            await self.lock.acquire()
            try:
                retry = True
                while retry:
                    try:
                        await self.tx.put(msg)
                        response = await asyncio.wait_for(self.rx.get(), timeout=2)
                        retry = False
                    except asyncio.TimeoutError:
                        print("request timed out, retrying...")
                        continue
            finally:
                self.lock.release()
            return response

    class Job:
        def __init__(self):
            self.record          = asyncio.Queue()
            self.save            = True
            self.status          = JobStatus.Pending
            self.type            = JobType.Reflow
            self.start_time      = None
            self.elapsed_seconds = None
            self.profile         = None
            self.phase           = TuningPhase.Standby
            self.data_file       = None # <oven_name>_<start_time>.csv
            self.details         = ""

    class Oven:
        def __init__(self):
            self.name             = "bismuth" # TODO get from controller
            self.status           = ControlStatus.Idle
            self.mode             = ControlMode.Point
            self.target           = 0.0
            self.max_temp         = None
            self.learned_velocity = None
            self.learned_inertia  = None
            self.max_rate         = None
            self.enabled          = False
            self.latest_temp      = None


    def __init__(self, close_on_zero_clients=False):
        self.close_on_zero_clients = close_on_zero_clients
        self.config                = Config()
        self.websockets            = {}
        self.broadcasts            = asyncio.Queue()
        self.serial                = Context.Serial()
        self.profiles              = self.init_profiles()
        self.job                   = Context.Job()
        self.oven                  = Context.Oven()
        self.state                 = {
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

async def record_timeseries(app):
    while True:
        job = app['ctx'].job
        data = await job.record.get()

        if job.status is JobStatus.Pending or not job.save:
            continue

        with open(job.data_file, "a") as myfile:
            myfile.write(f'{data["time"]},{data["temperature"]}\n')


async def broadcast(app):
    while True:
        msg = await app['ctx'].broadcasts.get()
        for ws in app['ctx'].websockets.values():
            await ws.send_str(msg)
