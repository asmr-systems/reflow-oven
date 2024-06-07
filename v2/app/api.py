import glob
import time
import json

from app import serial
from .types import make_connection_response, make_status_response, SerialResponse, make_job_status
from .types import make_temperature_response
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

def decode_status_byte(app, data):
    data_arr = data.split(' ')
    status_byte = int(data_arr[1], 16)
    temp_target_value = float(data_arr[2])

    enabled = (0x01 & status_byte) == 1
    control_status = (0x06 & status_byte) >> 1 # 0:idle, 1:running, 2:tuning
    tuning_phase = (0x18 & status_byte) >> 3 # 0:n/a, 1:steady-state, 2:velocity, 3:inertia
    control_mode = (0x60 & status_byte) >> 5 # 0:n/a, 1:point, 2:rate, 3:duty_cycle

    app['ctx'].oven.enabled = enabled
    app['ctx'].oven.mode = [None, ControlMode.Point, ControlMode.Rate, ControlMode.DutyCycle][control_mode]
    app['ctx'].oven.status = [ControlStatus.Idle, ControlStatus.Running, ControlStatus.Tuning][control_status]
    app['ctx'].oven.target = temp_target_value

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

    decode_status_byte(app, resp.data)

    return make_status_response(app), 200


def get_usb_serial_devices():
    return {
        'action': 'get',
        'type': 'devices',
        'data': glob.glob('/dev/ttyUSB*'),
    }, 200


async def connect(app, port=None):
    ser = app['ctx'].serial
    await serial.connect(app, ser.port if port is None else port)
    return make_connection_response(app), 200


async def get_job_status(app):
    resp = await serial_request(app, "A")
    if resp.status != SerialResponse.Status.Ok:
        return handle_error_response(resp)

    decode_status_byte(app, resp.data)

    return make_job_status(app), 200


async def set_job(app, job_settings):
    if 'save' in job_settings:
        app['ctx'].job.save = json.loads(job_settings['save'])
    if 'type' in job_settings:
        if job_settings['type'] == JobType.Reflow.value:
            app['ctx'].job.type = JobType.Reflow
        if job_settings['type'] == JobType.Tune.value:
            app['ctx'].job.type = JobType.Tune
    if 'details' in job_settings:
        app['ctx'].job.details = job_settings['details']
    if 'profile' in job_settings:
        if job_settings['profile'] in app['ctx'].profiles:
            app['ctx'].job.profile = job_settings['profile']

    return make_job_status(app), 200

# TODO implement me
async def start_job(app):
    # TODO start job -
    # if we have set the job mode to tuning, send the tune command
    # if it is reflow mode, start

    return make_status_response(app), 200

# TODO implement me
async def stop_job(app):
    # TODO send idle command
    return make_status_response(app), 200


async def get_temperature(app):
    resp = await serial_request(app, "F")
    if resp.status != SerialResponse.Status.Ok:
        return handle_error_response(resp)

    # TODO build this into serial_request as optional parameter: match_command=False
    # if resp[0] != "F":
    #     # TODO: handle this error!
    #     return {}

    data = {
        'time': time.time(),
        'temperature': float(resp.data[1:]),
    }

    app['ctx'].oven.latest_temp = data['temperature']
    await app['ctx'].job.record.put(data)

    return make_temperature_response(data), 200

# TODO implement me
async def get_recorded_data(app, downsample=False):
    # read recorded data and pass back in arrays
    pass


async def set_temperature(app, mode, value):
    command = 'G'
    if mode == ControlMode.Rate.value:
        command = 'H'
    elif mode == ControlMode.DutyCycle.value:
        command = 'I'

    resp = await serial_request(app, f"{command} {value}")
    if resp.status != SerialResponse.Status.Ok:
        return handle_error_response(resp)

    decode_status_byte(app, resp.data)

    return make_status_response(app), 200


async def enable(app):
    return await set_enable(app, enable=True)


async def disable(app):
    return await set_enable(app, enable=False)


async def set_enable(app, enable=True):
    resp = await serial_request(app, "D" if enable else "C")
    if resp.status != SerialResponse.Status.Ok:
        return handle_error_response(resp)

    decode_status_byte(app, resp.data)

    return make_status_response(app), 200
