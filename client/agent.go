package main

import (
	"log"
	"math/rand"

	"golang.org/x/net/websocket"
)

type Candidate struct {
	State  int
	Member int
	You    int
}

type Ack struct {
	Say string
}

type Select struct {
	Index int
	Say   string
}

var phrase = []string{
	"Sec",
	"Hack",
	"365",
}

func main() {
	conn, err := websocket.Dial("ws://localhost:8090/senda", "", "localhost:8090")
	if err != nil {
		log.Println(err)
		return
	}
	for {
		var cand Candidate
		err = websocket.JSON.Receive(conn, &cand)
		if err != nil {
			log.Println(err)
			return
		}
		log.Println(phrase[cand.State])
		if cand.State == 2 {
			log.Println("ack")
			err = websocket.JSON.Send(conn, &Ack{Say: phrase[cand.State]})
			if err != nil {
				log.Println(err)
				return
			}
		} else {
			log.Println("select")
			for {
				i := rand.Intn(cand.Member)
				if i == cand.You {
					continue
				}
				websocket.JSON.Send(conn, &Select{Index: i, Say: phrase[cand.State]})
				break
			}
		}
	}
}
