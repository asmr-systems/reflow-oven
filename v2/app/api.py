def has_startbyte(msg):
    return msg[0] == '\x02'

async def get_status(app):
    resp = " "
    while not has_startbyte(resp):
        resp = await app['ctx'].serial.request("\x02A")
    resp = resp[1:].strip()
    return resp
