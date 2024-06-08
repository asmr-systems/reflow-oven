import os
import json
import asyncio

from app import types
from .settings import Config
from .types import JobStatus, JobType, TuningPhase, ControlMode, ControlStatus
from .types import make_status_response
from .api import get_temperature


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

        async def request(self, msg, timeout=2):
            # await self.tx.put(msg)
            # await self.rx.get()
            response = None
            await self.lock.acquire()
            try:
                retry = True
                while retry:
                    try:
                        await self.tx.put(msg)
                        response = await asyncio.wait_for(self.rx.get(), timeout=timeout)
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
            self.save            = False
            self.status          = JobStatus.Pending
            self.type            = JobType.Reflow
            self.start_time      = None
            self.elapsed_seconds = None
            self.profile         = None
            self.phase           = TuningPhase.All
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


    def __init__(self, close_on_zero_clients=False):
        self.close_on_zero_clients = close_on_zero_clients
        self.config                = Config()
        self.websockets            = {}
        self.broadcasts            = asyncio.Queue()
        self.serial                = Context.Serial()
        self.profiles              = self.init_profiles()
        self.job                   = Context.Job()
        self.job_q                 = asyncio.Queue()
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

        if job.status != JobStatus.Running or not job.save:
            continue

        with open(app['ctx'].config.data_dir / job.data_file, "a") as myfile:
            myfile.write(f'{data["time"]},{data["temperature"]}\n')


async def broadcast(app):
    while True:
        msg = await app['ctx'].broadcasts.get()
        for ws in app['ctx'].websockets.values():
            await ws.send_str(msg)


async def manage_jobs(app):
    while True:
        run = await app['ctx'].job_q.get()
        initialize = True
        while run:
            msg, status = await get_temperature(app)
            # TODO handle if status is not 200
            await app['ctx'].broadcasts.put(msg)
            await app['ctx'].job.record.put(msg['data'])

            if app['ctx'].job.type == JobType.Reflow:
                run = await manage_reflow(app, msg['data'], initialize=initialize)
            elif app['ctx'].job.type == JobType.Tune:
                run = await manage_tune(app, msg['data'], initialize=initialize)

            if not run:
                # the job was completed, broadcast a complete status
                await app['ctx'].broadcasts.put(make_status_response(app))

            if not app['ctx'].job_q.empty():
                run = await app['ctx'].job_q.get()

            initialize = False


async def manage_reflow(app, temp_data, initialize=False):
    if initialize:
        print("initializing")
    else:
        print("done")
        return False

    return True


async def manage_tune(app, temp_data, initialize=False):
    return True
