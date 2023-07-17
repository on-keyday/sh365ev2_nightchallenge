`use strict`


const s=new WebSocket("ws://"+location.host+"/senda")

s.addEventListener("message",(msg)=>{
    const parsed = JSON.parse(msg.data)
    console.log(parsed.Say);
    if(parsed.Member===undefined) {
        s.send(JSON.stringify({"OK":true}))
    }
    else{
        for(;;) {
            const index = Math.floor(Math.random() * parsed.Member);
            if(index==parsed.You){
                continue;
            }
            s.send(JSON.stringify({"Index":index}))
            break;
        }
    }
})
