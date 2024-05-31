from pathlib import Path

BASE_DIR = Path(__file__).parent
STATIC_DIR = BASE_DIR / 'static'
DATA_DIR   = BASE_DIR / 'data'

config = {
    'STATIC_DIR': STATIC_DIR,
    'PROFILES_FILE': DATA_DIR / 'profiles.json',
    'SERIAL': {
        'PORT': '/dev/ttyUSB0',
        'BAUD': 9600,
    }
}

class Config:
    class Serial:
        def __init__(self):
            self.port = '/dev/ttyUSB0'
            self.baud = 9600

    def __init__(self):
        self.static_dir  = STATIC_DIR
        self.serial      = Config.Serial()
        self.profiles_fn = DATA_DIR / 'profiles.json'
