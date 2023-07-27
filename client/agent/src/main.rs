use serde::{Deserialize, Serialize};
use tokio_tungstenite as ws;
use tokio;
use tungstenite::Message;
use futures_util::{SinkExt, StreamExt};

#[derive(Serialize, Deserialize, Debug)]
struct Candidate {
    #[serde(default, rename = "You")]
    pub you: u64,
    #[serde(default, rename = "Member")]
    pub member: u64,
    #[serde(rename = "State")]
    pub state: u64,
}

#[derive(Serialize, Deserialize, Debug)]
struct Ack {
    #[serde(rename = "Say")]
    pub say: String,
}

#[derive(Serialize, Deserialize, Debug)]
struct Select {
    #[serde(rename = "Index")]
    pub index: u64,
    #[serde(rename = "Say")]
    pub say: String,
}

#[tokio::main]
async fn main() {
    let (mut s ,_) = ws::connect_async("ws://localhost:8090/senda").await.unwrap();
    while let Some(msg) =  s.next().await {
        match msg {
            Ok(Message::Text(msg)) => {
                let c = serde_json::from_slice::<Candidate>(msg.as_bytes()).unwrap();
                let y = match c.state {
                    2 => {
                        println!("365");
                        let ack = Ack {
                            say: String::from("365"),
                        };
                        serde_json::to_string(&ack)
                    }
                    0 | 1 => {
                        let sel = Select {
                            index: (c.you + 1) % c.member,
                            say: match c.state {
                                0 => {
                                    println!("Sec");
                                    String::from("Sec")
                                }
                                1 => {
                                    println!("Hack");
                                    String::from("Hack")
                                }
                                _ => unreachable!(),
                            },
                        };
                        serde_json::to_string(&sel)
                    }
                    _ => unreachable!(),
                }
                .unwrap();
                s.send(Message::Text(y)).await.unwrap();
            }
            Ok(Message::Close(_)) => {break;}
            msg=> {panic!("not supported {:?}",msg)}
        }
    }
}
