from pathlib import Path


BASE_DIR = Path(__file__).parent
STATIC_DIR = BASE_DIR / 'static'
DATA_DIR   = BASE_DIR / 'data'


class Config:
    class Serial:
        def __init__(self):
            self.baud = 115200

    def __init__(self):
        self.static_dir  = STATIC_DIR
        self.data_dir    = DATA_DIR
        self.serial      = Config.Serial()
        self.profiles_fn = DATA_DIR / 'profiles.json'
