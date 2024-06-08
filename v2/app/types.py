import time
from enum import Enum


class JobStatus(str, Enum):
    Pending  = "pending"
    Running  = "running"
    Aborted  = "aborted"
    Complete = "complete"

class ControlStatus(str, Enum):
    Idle    = "idle"
    Running = "running"
    Tuning  = "tuning"

class JobType(str, Enum):
    Reflow = "reflow"
    Tune   = "tune"

class TuningPhase(str, Enum):
    All         = "all"
    SteadyState = "steady_state"
    Velocity    = "velocity"
    Inertia     = "inertia"

class ControlMode(str, Enum):
    Point     = "point"
    DutyCycle = "duty_cycle"


class SerialResponse:
    class Status(Enum):
        ConnectionError = "connection_error"
        SerialError     = "serial_error"
        Ok              = "ok"

    def __init__(self, app, status, data = None):
        self.status = status
        self.data   = data
        if status is SerialResponse.Status.ConnectionError:
            self.data = make_connection_response(app)
        if status is SerialResponse.Status.SerialError:
            self.data = make_status_response(app)


def make_connection_response(app):
    return {
        'action': 'get',
        'type': 'connection',
        'data': {
            'port': app['ctx'].serial.port,
            'connected': app['ctx'].serial.connected,
        },
    }


def make_job_status(app):
    if app['ctx'].job.status == JobStatus.Running:
        app['ctx'].job.elapsed_seconds = time.time() - app['ctx'].job.start_time

    return {
        'action': 'get',
        'type': 'job',
        'data': {
            'status': app['ctx'].job.status,
            'save': app['ctx'].job.save,
            'type': app['ctx'].job.type,
            'details': app['ctx'].job.details,
            'start_time': app['ctx'].job.start_time,
            'elapsed_seconds': app['ctx'].job.elapsed_seconds,
            'profile': app['ctx'].job.profile,
            'phase': app['ctx'].job.phase,
        },
    }

def make_oven_status(app):
    return {
        'action': 'get',
        'type': 'oven',
        'data': {
            'name': app['ctx'].oven.name,
            'status': app['ctx'].oven.status,
            'mode': app['ctx'].oven.mode,
            'target': app['ctx'].oven.target,
            'rate': app['ctx'].oven.rate,
            'max_temp': app['ctx'].oven.max_temp,
            'learned_velocity': app['ctx'].oven.learned_velocity,
            'learned_inertia': app['ctx'].oven.learned_inertia,
            'max_rate': app['ctx'].oven.max_rate,
            'enabled': app['ctx'].oven.enabled,
        }
    }

def make_status_response(app):
    return {
        'action': 'get',
        'type': 'status',
        'data': {
            'connection': make_connection_response(app)['data'],
            'job': make_job_status(app)['data'],
            'oven': make_oven_status(app)['data']
        },
    }

def make_temperature_response(data):
    return {
        'action': 'get',
        'type': 'temperature',
        'data': {
            'time': data['time'],
            'temperature': data['temperature'],
        }
    }
