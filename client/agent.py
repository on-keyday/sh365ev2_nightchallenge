

import websocket as ws
import json
import random as r

PHRASE=["Sec","Hack","365"]

def on_msg(ws :ws.WebSocket,msg):
    msg = json.loads(msg)    
    print(PHRASE[msg["State"]])
    if msg["State"]==2:
        ws.send(json.dumps({"Say": PHRASE[msg["State"]]}))
    else:
        memb=msg["Member"]
        me=msg["You"]
        param=r.randint(0,memb-1)
        while param==me:
            param=r.randint(0,memb-1)        
        ws.send(json.dumps({"Index": param,"Say": PHRASE[msg["State"]]}))

app=ws.WebSocketApp("ws://localhost:8090/senda"
                ,on_message=on_msg)

app.run_forever()
