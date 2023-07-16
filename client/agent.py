

import websocket as ws
import json
import random as r


def on_msg(ws :ws.WebSocket,msg):
    msg = json.loads(msg)    
    print(msg["Say"])
    memb=msg.get("Member",None)
    me=msg.get("You",None)
    if memb is None:
        ws.send(json.dumps({"OK": True}))
    else:
        param=r.randint(0,memb-1)
        while param==me:
            param=r.randint(0,memb-1)        
        ws.send(json.dumps({"Index": param}))

app=ws.WebSocketApp("ws://localhost:8090/senda"
                ,on_message=on_msg)

app.run_forever()
