`use strict`


const s=new WebSocket("ws://"+location.host+"/senda")

const phrase=["Sec","Hack","365"]

s.addEventListener("message",(msg)=>{
    const parsed = JSON.parse(msg.data)
    console.log(phrase[parsed.State]);
    if(parsed.Member===undefined) {
        s.send(JSON.stringify({"Say":phrase[parsed.State]}))
    }
    else{
        for(;;) {
            const index = Math.floor(Math.random() * parsed.Member);
            if(index==parsed.You){
                continue;
            }
            s.send(JSON.stringify({"Index":index,"Say": phrase[parsed.State]}))
            break;
        }
    }
})
