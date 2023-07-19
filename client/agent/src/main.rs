
use websocket as ws;
use ws::{Message};
use ws::url::Url;
use serde::{Deserialize, Serialize};
use rand;

#[derive(Serialize, Deserialize, Debug)]
struct Candidate {
    #[serde(default,rename="You")]
    pub you :u64,
    #[serde(default,rename="Member")]
    pub member :u64,
    #[serde(rename="State")]
    pub state :u64,
}

#[derive(Serialize, Deserialize, Debug)]
struct Ack {
    #[serde(rename="Say")]
    pub say :String,
}

#[derive(Serialize, Deserialize, Debug)]
struct Select {
    #[serde(rename="Index")]
    pub index :u64,
    #[serde(rename="Say")]
    pub say :String,
}



fn main()  {
    let url =Url::parse("ws://localhost:8090/senda").unwrap();
    let builder = ws::ClientBuilder::from_url(&url);
    let mut conn = builder.origin(String::from("ws://localhost:8090")).connect_insecure().unwrap();
    loop {
        if let Ok(msg)= conn.recv_message() {
            if msg.is_data(){
                let msg : Message = msg.into();
                let s  = serde_json::from_slice::<Candidate>(&msg.payload).unwrap();
                let y = match s.state {
                    2=>{ 
                        println!("365");
                        let ack =Ack{say: String::from("365") };
                        serde_json::to_string(&ack) 
                    }
                    0|1=>{
                        let sel = Select{
                            index: (s.you+1)%s.member,
                            say: match s.state {
                                0 => { println!("Sec"); String::from("Sec")},
                                1 => { println!("Hack"); String::from("Hack")},
                                _=>unreachable!()
                            }
                        };
                        serde_json::to_string(&sel)
                    }
                    _=>unreachable!()
                }.unwrap();
                let msg =Message::text(y);
                conn.send_message(&msg).unwrap();
            }
        }
        else {
            break;
        }
    }
}
