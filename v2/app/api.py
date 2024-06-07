import glob

from app import serial
from .types import make_connection_response, make_status_response, SerialResponse
from .types import JobStatus, ControlStatus, JobType, TuningPhase, ControlMode


def has_start_byte(msg):
    return msg[0] == '\x02'


async def serial_request(app, command):
    if not app['ctx'].serial.connected:
        return SerialResponse(app, SerialResponse.Status.ConnectionError)

    resp = await app['ctx'].serial.request("\x02" + command)

    if not has_start_byte(resp):
        return SerialResponse(app, SerialResponse.Status.SerialError)

    return SerialResponse(
        app,
        SerialResponse.Status.Ok,
        data=resp[1:].strip()
    )


def handle_error_response(resp):
    if resp.status is SerialResponse.Status.ConnectionError:
        return resp.data, 503 # return 503 Service Unavailable
    if resp.status is SerialResponse.Status.SerialError:
        return resp.data, 500 # return 503 Internal Server Error
    return resp.data, 200

def decode_status_byte(app, status_byte):
    enabled = (0x01 & status_byte) == 1
    control_status = (0x06 & status_byte) >> 1 # 0:idle, 1:running, 2:tuning
    tuning_phase = (0x18 & status_byte) >> 3 # 0:n/a, 1:steady-state, 2:velocity, 3:inertia
    control_mode = (0x60 & status_byte) >> 5 # 0:n/a, 1:point, 2:rate, 3:duty_cycle

    app['ctx'].oven.enabled = enabled
    app['ctx'].oven.mode = [None, ControlMode.Point, ControlMode.Rate, ControlMode.DutyCycle][control_mode]
    app['ctx'].oven.status = [ControlStatus.Idle, ControlStatus.Running, ControlStatus.Tuning][control_status]
    if tuning_phase != 0 and app['ctx'].oven.status == ControlStatus.Tuning:
        app['ctx'].job.type = JobType.Tuning
        app['ctx'].job.status = JobStatus.Running
        app['ctx'].job.phase  = [None, TuningPhase.SteadyState, TuningPhase.Velocity, TuningPhase.Inertia][tuning_phase]
    if control_status == 1:
        app['ctx'].job.type   = JobType.Reflow
        app['ctx'].job.status = JobStatus.Running


async def get_status(app):
    resp = await serial_request(app, "A")
    if resp.status != SerialResponse.Status.Ok:
        return handle_error_response(resp)

    decode_status_byte(app, int(resp.data.split(' ')[1], 16))

    return make_status_response(app), 200


def get_usb_serial_devices():
    return {
        'action': 'get',
        'type': 'devices',
        'data': glob.glob('/dev/ttyUSB*'),
    }


async def connect(app, port=None):
    ser = app['ctx'].serial
    await serial.connect(app, ser.port if port is None else port)
    return make_connection_response(app)


async def get_job_status(app):
    return make_job_status(app)


async def set_job(app, job_settings):
    # TODO implement me
    return {}


async def start_job(app):
    # TODO implement me
    return {}


async def stop_job(app):
    # TODO implement me
    return {}


async def get_temperature(app):
    if not app['ctx'].serial.connected:
        return make_connection_response(app)

    resp = " "
    while not has_startbyte(resp):
        resp = await app['ctx'].serial.request("\x02F")
    resp = resp[1:].strip()

    if resp[0] != "F":
        # TODO: handle this error!
        return {}

    data = {
        'time': time.time(),
        'temperature': float(resp[1:]),
    }

    app['ctx'].oven.latest_temp = data['temperature']
    await app['ctx'].job.record.put(data)

    return make_temperature_response(data)


async def get_recorded_data(app, downsample=False):
    # read recorded data and pass back in arrays
    pass


async def set_temperature(app, mode, value):
    # TODO check stuff
    pass


async def enable(app):
    return await set_enable(app, enable=True)


async def disable(app):
    return await set_enable(app, enable=False)


async def set_enable(app, enable=True):
    resp = await serial_request(app, "D" if enable else "C")
    if resp.status != SerialResponse.Status.Ok:
        return handle_error_response(resp)

    decode_status_byte(app, int(resp.data.split(' ')[1], 16))

    return make_status_response(app), 200
