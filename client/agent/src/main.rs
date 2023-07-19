
use websocket as ws;
use ws::{Message};
use ws::url::Url;
use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Debug)]
struct Candidate {
    You :u64,
    Member :u64,
    State :u64,
}

fn main() {
    let mut builder = ws::ClientBuilder::from_url(&Url::parse("ws://localhost:8090/senda").unwrap());
    let mut conn = builder.connect_insecure().unwrap();
    loop {
        if let Ok(msg)= conn.recv_message() {
            if msg.is_data(){
                let msg : Message = msg.into();
                let s  = serde_json::from_slice::<Candidate>(&msg.payload);
                
            }
        }
    }
}
